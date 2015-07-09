/*
 * Copyright (c) 2014-2015 Paul Mattes.
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

#include "fprint_screen.h"
#include "varbuf.h"

#include "httpd-core.h"
#include "httpd-io.h"
#include "httpd-nodes.h"

#if defined(_WIN32) /*[*/
# include "winprint.h"
#endif /*]*/

extern unsigned char favicon[];
extern unsigned favicon_size;

/**
 * Capture the screen image.
 *
 * @param[in] d		Session handle
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
	rv = httpd_dyn_error(dhandle, 400, "Internal error (open)");
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    }
    f = fdopen(fd, "w+");
    if (f == NULL) {
	rv = httpd_dyn_error(dhandle, 400, "Internal error (fdopen)");
	close(fd);
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    }

    /* Write the screen to it in HTML. */
    switch (fprint_screen(f, P_HTML, FPS_NO_HEADER, NULL, NULL)) {
    case FPS_STATUS_SUCCESS:
    case FPS_STATUS_SUCCESS_WRITTEN:
	break;
    case FPS_STATUS_ERROR:
    case FPS_STATUS_CANCEL:
	rv = httpd_dyn_error(dhandle, 400, "Internal error (fprint_screen)");
	fclose(f);
	unlink(temp_name);
	Free(temp_name);
	*status = rv;
	return false;
    }

    /* Read it back into a varbuf_t. */
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
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
dyn_form_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, const char *sl_buf, size_t sl_len)
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
		    sl_len, sl_buf,
		    len, buf);
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
		    sl_len, sl_buf);
	    }
	    vb_free(&r);
	}
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, 400, "%.*s", len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, 500, "%.*s", len, buf);
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

    /* If the specified an action, execute it. */
    action = httpd_fetch_query(dhandle, "action");
    if (action && *action) {
	switch (hio_to3270(action, dyn_form_complete, dhandle, CT_TEXT)) {
	case SENDTO_COMPLETE:
	    return HS_SUCCESS_OPEN; /* not strictly accurate */
	case SENDTO_PENDING:
	    return HS_PENDING;
	case SENDTO_INVALID:
	    return httpd_dyn_error(dhandle, 400, "Invalid 3270 action.\n");
	default:
	case SENDTO_FAILURE:
	    return httpd_dyn_error(dhandle, 500, "Processing error.\n");
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
 * @param[in] sl_buf	Status line buffer (ignored)
 * @param[in] sl_len	Status line length (ignored)
 */
static void
rest_dyn_text_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	rv = httpd_dyn_complete(dhandle, "%.*s", len, buf);
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, 400, "%.*s", len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, 400, "%.*s", len, buf);
	break;
    }
    hio_async_done(dhandle, rv);
}

/**
 * Callback for the REST API plain-text nonterminal dynamic node
 * (/3270/rest/html).
 *
 * @param[in] url	URL fragment
 * @param[in] dhandle	daemon handle
 *
 * @return httpd_status_t
 */
static httpd_status_t
rest_text_dyn(const char *url, void *dhandle)
{
    if (!*url) {
	return httpd_dyn_error(dhandle, 400, "Missing 3270 action.\n");
    }

    switch (hio_to3270(url, rest_dyn_text_complete, dhandle, CT_TEXT)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	return httpd_dyn_error(dhandle, 400, "Invalid 3270 action.\n");
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, 500, "Processing error.\n");
    }
}

/**
 * Completion callback for the 3270 text command node (/3270/rest/stext).
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
rest_dyn_status_text_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, const char *sl_buf, size_t sl_len)
{
    httpd_status_t rv = HS_CONTINUE;

    switch (cbs) {
    case SC_SUCCESS:
	rv = httpd_dyn_complete(dhandle, "%.*s\n%.*s",
		sl_len, sl_buf,
		len, buf);
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, 400, "%.*s", len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, 500, "%.*s", len, buf);
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
    if (!*url) {
	return httpd_dyn_error(dhandle, 400, "Missing 3270 action.\n");
    }

    switch (hio_to3270(url, rest_dyn_status_text_complete, dhandle, CT_TEXT)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	return httpd_dyn_error(dhandle, 400, "Invalid 3270 action.\n");
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, 500, "Processing error.\n");
    }
}

/**
 * Completion callback for the 3270 html command node (/3270/rest/html).
 *
 * @param[in] dhandle	daemon handle
 * @param[in] cbs	completion status
 * @param[in] buf	data buffer
 * @param[in] len	length of data buffer
 * @param[in] sl_buf	status-line buffer
 * @param[in] sl_len	length of status-line buffer
 */
static void
rest_dyn_html_complete(void *dhandle, sendto_cbs_t cbs, const char *buf,
	size_t len, const char *sl_buf, size_t sl_len)
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
		sl_len, sl_buf,
		len, buf);
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
		sl_len, sl_buf);
	}
	break;
    case SC_USER_ERROR:
	rv = httpd_dyn_error(dhandle, 400, "%.*s", len, buf);
	break;
    case SC_SYSTEM_ERROR:
	rv = httpd_dyn_error(dhandle, 500, "%.*s", len, buf);
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
    if (!*url) {
	return httpd_dyn_error(dhandle, 400, "Missing 3270 action.\n");
    }

    switch (hio_to3270(url, rest_dyn_html_complete, dhandle, CT_HTML)) {
    case SENDTO_COMPLETE:
	return HS_SUCCESS_OPEN; /* not strictly accurate */
    case SENDTO_PENDING:
	return HS_PENDING;
    case SENDTO_INVALID:
	return httpd_dyn_error(dhandle, 400, "Invalid 3270 action.\n");
    default:
    case SENDTO_FAILURE:
	return httpd_dyn_error(dhandle, 500, "Processing error.\n");
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
    httpd_status_t rv;

    rv = httpd_dyn_error(dhandle, 501, "JSON support coming soon.\n");
    return rv;
}

/**
 * Initialize the HTTP object hierarchy.
 */
void
httpd_objects_init(void)
{
    void *nhandle;

    (void) httpd_register_dir("/3270", "Emulator state");
    (void) httpd_register_dyn_term("/3270/screen.html", "Screen image",
	    CT_HTML, "text/html; charset=utf-8", HF_TRAILER, hn_screen_image);
    (void) httpd_register_dyn_term("/3270/interact.html", "Interactive form",
	    CT_HTML, "text/html; charset=utf-8", HF_TRAILER, hn_interact);
    (void) httpd_register_dir("/3270/rest", "REST interface");
    (void) httpd_register_fixed_binary("/favicon.ico", "Browser icon",
	    CT_BINARY, "image/vnd.microsoft.icon", HF_HIDDEN, favicon,
	    favicon_size);
    nhandle = httpd_register_dyn_nonterm("/3270/rest/text",
	    "REST plain text interface", CT_TEXT, "text/plain; charset=utf-8",
	    HF_NONE, rest_text_dyn);
    httpd_set_alias(nhandle, "text/Query()");
    nhandle = httpd_register_dyn_nonterm("/3270/rest/stext",
	    "REST plain text interface with status line", CT_TEXT,
	    "text/plain; charset=utf-8",
	    HF_NONE, rest_status_text_dyn);
    httpd_set_alias(nhandle, "stext/Query()");
    nhandle = httpd_register_dyn_nonterm("/3270/rest/html",
	    "REST HTML interface", CT_HTML, "text/html; charset=utf-8",
	    HF_TRAILER, rest_html_dyn);
    httpd_set_alias(nhandle, "html/Query()");
    (void) httpd_register_dyn_nonterm("/3270/rest/json",
	    "REST JSON interface", CT_TEXT, "text/plain; charset=utf-8",
	    HF_NONE, rest_json_dyn);
}
