/*
 * Copyright (c) 2014-2024 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *      httpd-nodes.c
 *              x3270 webserver, methods for HTTP objects
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <unistd.h>
# include <netinet/in.h>
# include <sys/select.h>
# include <arpa/inet.h>
#endif /*]*/
#include <signal.h>
#include <fcntl.h>
#include <assert.h>

#include "fprint_screen.h"
#include "json.h"
#include "s3270_proto.h"
#include "txa.h"
#include "varbuf.h"

#include "httpd-core.h"
#include "httpd-io.h"
#include "httpd-nodes.h"
#include "task.h"

#if defined(_WIN32) /*[*/
# include "winprint.h"
#endif /*]*/

extern unsigned char favicon[];
extern unsigned favicon_size;

/**
 * Capture the screen image.
 *
 * @param[in] dhandle	Session handle
 * @param[out] image	Image in HTML, if succeeded; must free when finished
 * @param[out] status	Error code, if failed
 *
 * @return true for success, false for failure
 */
static bool
hn_image(void *dhandle, varbuf_t *image, httpd_status_t *status)
{
    httpd_status_t rv;
    int fd;
    FILE *f;
    char *temp_name;
    char buf[8192];

    /* Open the temporary file. */
#if defined(_WIN32) /*[*/
    fd = win_mkstemp(&temp_name, P_HTML);
#else /*][*/
    temp_name = NewString("/tmp/x3hXXXXXX");
    fd = mkstemp(temp_name);
#endif /*]*/
    if (fd < 0) {
	rv = httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"Internal error (open)");
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    }
    f = fdopen(fd, "w+");
    if (f == NULL) {
	rv = httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"Internal error (fdopen)");
	close(fd);
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    }

    /* Write the screen to it in HTML. */
    switch (fprint_screen(f, P_HTML, FPS_NO_HEADER | FPS_OIA, NULL, NULL,
		NULL)) {
    case FPS_STATUS_SUCCESS:
    case FPS_STATUS_SUCCESS_WRITTEN:
	break;
    case FPS_STATUS_ERROR:
    case FPS_STATUS_CANCEL:
	rv = httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"Internal error (fprint_screen)");
	fclose(f);
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    case FPS_STATUS_WAIT:
	assert(false);
	return false;
    }


    /* Read it back into a varbuf_t. */
    fflush(f);
    rewind(f);
    vb_init(image);
    while (fgets(buf, sizeof(buf), f) != NULL) {
	vb_appends(image, buf);
    }

    /* Dispose of the file. */
    fclose(f);
    unlink(temp_name);
    Free(temp_name);

    /* Success. */
    return true;
}

/**
 * Callback for the screen image dynamic node.
 *
 * @param[in] uri	URI
 * @param[in] dhandle	Session handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
hn_screen_image(const char *uri, void *dhandle)
{
    httpd_status_t rv;
    varbuf_t r;

    /* Get the image. */
    if (hn_image(dhandle, &r, &rv)) {
	/* Success: Write the response. */
	rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>3270 Screen Image</title>\n\
</head>\n\
<body>\n\
%.*s\n", (int)vb_len(&r), vb_buf(&r));
	vb_free(&r);
    }
    return rv;
}

/* The tiny HTML form on the interactive page. */
#define CMD_FORM \
"<form method=\"GET\" accept-charset=\"UTF-8\" target=\"_self\">\n\
Action and parameters:<br>\n\
<input type=\"text\" name=\"action\" size=\"50\" autofocus>\n\
<input type=\"submit\" value=\"Submit\">\n\
</form>\n"

/**
 * Completion callback for the interactive form.
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] jresult	JSON result buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
dyn_form_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, json_t *jresult, const char *sl_buf, size_t sl_len)
{
    varbuf_t r;
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	if (hn_image(dhandle, &r, &rv)) {
	    if (len) {
		rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>Interactive Form</title>\n\
</head>\n\
<body>\n"
CMD_FORM
"<br>\n\
%s\n\
<h2>Status</h2>\n\
<pre>%.*s</pre>\n\
<h2>Result</h2>\n\
<pre>%.*s</pre>",
		    vb_buf(&r),
		    (int)sl_len, sl_buf,
		    (int)len, buf);
	    } else {
		rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>Interactive Form</title>\n\
</head>\n\
<body>\n"
CMD_FORM
"<br>\n\
%s\n\
<h2>Status</h2>\n\
<pre>%.*s</pre>\n\
<h2>Result</h2>\n\
<i>(none)</i>",
		    vb_buf(&r),
		    (int)sl_len, sl_buf);
	    }
	    vb_free(&r);
	}
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, CT_HTML, 400, NULL, "%.*s", (int)len,
		buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, CT_HTML, 500, NULL, "%.*s", (int)len,
		buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Callback for the interactive form dynamic node.
 *
 * @param[in] uri	URI
 * @param[in] dhandle	Session handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
hn_interact(const char *uri, void *dhandle)
{
    const char *action;
    httpd_status_t rv;
    varbuf_t r;
    char *errmsg;

    /* If the specified an action, execute it. */
    action = httpd_fetch_query(dhandle, "action");
    if (action && *action) {
	switch (hio_to3270(action, dyn_form_complete, dhandle, CT_TEXT,
		    CT_TEXT, &errmsg)) {
	case SENDTO_COMPLETE:
	    return HS_SUCCESS_OPEN; /* not strictly accurate */
	case SENDTO_PENDING:
	    return HS_PENDING;
	case SENDTO_INVALID:
	    txdFree(errmsg);
	    return httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		    "%s", txAsprintf("%s\n", errmsg));
	default:
	case SENDTO_FAILURE:
	    return httpd_dyn_error(dhandle, CT_HTML, 500, NULL,
		    "Processing error.\n");
	}
    }

    /* Otherwise, display the empty form. */
    if (!hn_image(dhandle, &r, &rv)) {
	return rv;
    }

    rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>Interactive Form</title>\n\
</head>\n\
<body>\n"
CMD_FORM
"<br>\n\
%s\n",
	    vb_buf(&r));

    vb_free(&r);

    return rv;
}

/**
 * Completion callback for the 3270 text command node (/3270/rest/text).
 *
 * @param[in] dhandle	Session handle
 * @param[in] cbs	Request status
 * @param[in] buf	Result / explanatory text buffer
 * @param[in] len	Result / explanatory text buffer length
 * @param[in] jresult	JSON result buffer
 * @param[in] sl_buf	Status line buffer (ignored)
 * @param[in] sl_len	Status line length (ignored)
 */
static void
rest_dyn_text_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, json_t *jresult, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	rv = httpd_dyn_complete(dhandle, "%.*s", (int)len, buf);
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, CT_TEXT, 400, NULL, "%.*s", (int)len,
		buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, CT_TEXT, 400, NULL, "%.*s", (int)len,
		buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Callback for the REST API plain-text nonterminal dynamic node
 * (/3270/rest/text).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_text_dyn(const char *url, void *dhandle)
{
    char *errmsg;

    if (!*url) {
	return httpd_dyn_error(dhandle, CT_TEXT, 400, NULL,
		"Missing 3270 action.\n");
    }

    switch (hio_to3270(url, rest_dyn_text_complete, dhandle, CT_TEXT,
		CT_TEXT, &errmsg)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	txdFree(errmsg);
	return httpd_dyn_error(dhandle, CT_TEXT, 400, NULL, "%s",
		txAsprintf("%s\n", errmsg));
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, CT_TEXT, 500, NULL,
		"Processing error.\n");
    }
}

/**
 * Completion callback for the 3270 text command node (/3270/rest/stext).
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] jresult	JSON result buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
rest_dyn_status_text_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, json_t *jresult, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	rv = httpd_dyn_complete(dhandle, "%.*s\n%.*s",
		(int)sl_len, sl_buf,
		(int)len, buf);
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, CT_TEXT, 400, NULL, "%.*s\n%.*s",
		(int)sl_len, sl_buf, (int)len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, CT_TEXT, 500, NULL, "%.*s\n%.*s",
		(int)sl_len, sl_buf, (int)len, buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Callback for the REST API plain-text plus status nonterminal dynamic node
 * (/3270/rest/stext).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_status_text_dyn(const char *url, void *dhandle)
{
    char *errmsg;

    if (!*url) {
	return httpd_dyn_error(dhandle, CT_TEXT, 400, NULL,
		"%s\nMissing 3270 action.\n", task_status_string());
    }

    switch (hio_to3270(url, rest_dyn_status_text_complete, dhandle, CT_TEXT,
		CT_TEXT, &errmsg)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	txdFree(errmsg);
	return httpd_dyn_error(dhandle, CT_TEXT, 400, NULL,
		"%s", txAsprintf("%s\n%s\n", task_status_string(), errmsg));
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, CT_TEXT, 500, NULL,
		"%s\nProcessing error.\n", task_status_string());
    }
}

/**
 * Completion callback for the 3270 html command node (/3270/rest/html).
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] jresult	JSON result buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
rest_dyn_html_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, json_t *jresult, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	if (len) {
	    rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>Success</title>\n\
</head>\n\
<body>\n\
<h1>Success</h1>\n\
<h2>Status</h2>\n\
<pre>%.*s</pre>\n\
<h2>Result</h2>\n\
<pre>%.*s</pre>",
		(int)sl_len, sl_buf,
		(int)len, buf);
	} else {
	    rv = httpd_dyn_complete(dhandle,
"<head>\n\
<title>Success</title>\n\
</head>\n\
<body>\n\
<h1>Success</h1>\n\
<h2>Status</h2>\n\
<pre>%.*s</pre>\n\
<h2>Result</h2>\n\
<i>(none)</i>",
		(int)sl_len, sl_buf);
	}
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"<h2>Status</h2>\n<pre>%.*s</pre>\n"
		"<h2>Result</h2><pre>%.*s</pre>", (int)sl_len, sl_buf,
		(int)len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, CT_HTML, 500, NULL,
		"<h2>Status</h2>\n<pre>%.*s</pre>\n"
		"<h2>Result</h2><pre>%.*s</pre>", (int)sl_len, sl_buf,
		(int)len, buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Completion callback for the 3270 JSON command node (/3270/rest/json).
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] jresult	JSON result buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
rest_dyn_json_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, json_t *jresult, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;
    char *w;

    switch (cbs) {
    case SC_SUCCESS:
	if (jresult != NULL) {
	    json_object_set(jresult, JRET_STATUS, NT, json_string(sl_buf, sl_len));
	    w = json_write_o(jresult, JW_ONE_LINE);
	    rv = httpd_dyn_complete(dhandle, "%s\n", w);
	    Free(w);
	} else {
	    json_t *j = json_object();

	    json_object_set(j, JRET_RESULT, NT, json_array());
	    json_object_set(j, JRET_RESULT_ERR, NT, json_array());
	    json_object_set(j, JRET_STATUS, NT, json_string(sl_buf, sl_len));
	    w = json_write_o(j, JW_ONE_LINE);
	    rv = httpd_dyn_complete(dhandle, "%s\n", w);
	    Free(w);
	    json_free(j);
	}
	break;
    case SC_USER_ERROR:
	json_object_set(jresult, JRET_STATUS, NT, json_string(sl_buf, sl_len));
	rv = httpd_dyn_error(dhandle, CT_JSON, 400, jresult, "%.*s", (int)len,
		buf);
	break;
    case SC_SYSTEM_ERROR:
	json_object_set(jresult, JRET_STATUS, NT, json_string(sl_buf, sl_len));
	rv = httpd_dyn_error(dhandle, CT_JSON, 500, jresult, "%.*s", (int)len,
		buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Callback for the REST API HTML nonterminal dynamic node (/3270/rest/html).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_html_dyn(const char *url, void *dhandle)
{
    char *errmsg;

    if (!*url) {
	return httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"<h2>Status</h2>\n<pre>%s</pre>\n"
		"<h2>Result</h2><pre>Missing 3270 action.</pre>",
		task_status_string());
    }

    switch (hio_to3270(url, rest_dyn_html_complete, dhandle, CT_TEXT,
		CT_HTML, &errmsg)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	txdFree(errmsg);
	return httpd_dyn_error(dhandle, CT_HTML, 400, NULL,
		"%s", txAsprintf("%s\n", errmsg));
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, CT_HTML, 500, NULL,
		"Processing error.\n");
    }
}

/**
 * Callback for the REST API JSON nonterminal dynamic node (/3270/rest/json).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_json_dyn(const char *url, void *dhandle)
{
    char *errmsg;

    if (!*url) {
	return httpd_dyn_error(dhandle, CT_JSON, 400, NULL, "Missing 3270 action.\n");
    }

    switch (hio_to3270(url, rest_dyn_json_complete, dhandle, CT_TEXT,
		CT_JSON, &errmsg)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	txdFree(errmsg);
	return httpd_dyn_error(dhandle, CT_TEXT, 400, NULL,
		"%s", txAsprintf("%s\n", errmsg));
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, CT_JSON, 500, NULL,
		"Processing error.\n");
    }
}

/**
 * Callback for the REST API POST nonterminal dynamic node (/3270/rest/post).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_post_dyn(const char *url, void *dhandle)
{
    content_t request_content_type;
    sendto_callback_t *callback;
    char *content = hio_content(dhandle);
    char *errmsg;

    if (content == NULL || !*content) {
	/* Do nothing, successfully. */
	return HS_SUCCESS_OPEN;
    }

    switch ((request_content_type = hio_content_type(dhandle))) {
    case CT_TEXT:	/* plain text */
	callback = rest_dyn_status_text_complete;
	break;
    case CT_JSON:	/* JSON-encoded text */
	callback = rest_dyn_json_complete;
	break;
#if 0
    case CT_XML:	/* XML-encoded text */
	callback = rest_dyn_xml_complete;
	break;
#endif
    default:
	return httpd_dyn_error(dhandle, CT_TEXT, 415, NULL,
		"Unsupported media type.\n");
    }

    switch (hio_to3270(content, callback, dhandle, request_content_type,
		request_content_type, &errmsg)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	txdFree(errmsg);
	return httpd_dyn_error(dhandle, request_content_type, 400, NULL,
		"%s", txAsprintf("%s\n", errmsg));
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, request_content_type, 500, NULL,
		"Processing error.\n");
    }
}

/**
 * Initialize the HTTP object hierarchy.
 */
void
httpd_objects_init(void)
{
    static bool initted = false;
    void *nhandle;

    if (initted) {
	return;
    }
    initted = true;

    httpd_register_dir("/3270", "Emulator state");
    httpd_register_dyn_term("/3270/screen.html", "Screen image",
	    CT_HTML, "text/html", VERB_GET | VERB_HEAD, HF_TRAILER,
	    hn_screen_image);
    httpd_register_dyn_term("/3270/interact.html", "Interactive form",
	    CT_HTML, "text/html", VERB_GET | VERB_HEAD, HF_TRAILER,
	    hn_interact);
    httpd_register_dir("/3270/rest", "REST interface");
    httpd_register_fixed_binary("/favicon.ico", "Browser icon",
	    CT_BINARY, "image/vnd.microsoft.icon", HF_HIDDEN, favicon,
	    favicon_size);
    nhandle = httpd_register_dyn_nonterm("/3270/rest/text",
	    "REST plain text interface", CT_TEXT, "text/plain",
	    VERB_GET | VERB_HEAD, HF_NONE, rest_text_dyn);
    httpd_set_alias(nhandle, "text/Query()");
    nhandle = httpd_register_dyn_nonterm("/3270/rest/stext",
	    "REST plain text interface with status line", CT_TEXT,
	    "text/plain",
	    VERB_GET | VERB_HEAD, HF_NONE, rest_status_text_dyn);
    httpd_set_alias(nhandle, "stext/Query()");
    nhandle = httpd_register_dyn_nonterm("/3270/rest/html",
	    "REST HTML interface", CT_HTML, "text/html",
	    VERB_GET | VERB_HEAD, HF_TRAILER, rest_html_dyn);
    httpd_set_alias(nhandle, "html/Query()");
    nhandle = httpd_register_dyn_nonterm("/3270/rest/json",
	    "REST JSON interface", CT_JSON, "application/json",
	    VERB_GET | VERB_HEAD, HF_NONE, rest_json_dyn);
    httpd_set_alias(nhandle, "json/Query()");
    httpd_register_dyn_term("/3270/rest/post", "REST POST interface",
	    CT_UNSPECIFIED, "text/plain", VERB_POST, HF_NONE, rest_post_dyn);
}
