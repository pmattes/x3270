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
 *      httpd-core.h
 *              x3270 webserver, header file for core protocol module
 */

#define SECURITY_COOKIE	"x3270-security"

typedef enum {
    CT_HTML,
    CT_TEXT,
    CT_JSON,
    CT_BINARY,
#if 0
    CT_XML,
#endif
    CT_UNSPECIFIED
} content_t;

/* Flags. */
#define HF_NONE		0x0
#define HF_TRAILER	0x1	/* include standard trailer */
#define HF_HIDDEN	0x2	/* do not include in directory listings */

typedef enum {
    HS_CONTINUE = 0,		/* incomplete request */
    HS_SUCCESS_OPEN = 1,	/* request succeeded, leave socket open */
    HS_ERROR_OPEN = 2,		/* request failed, leave socket open */
    HS_PENDING = 3,		/* request is pending (async) */
    HS_ERROR_CLOSE = -1,	/* request failed, close socket */
    HS_SUCCESS_CLOSE = -2	/* request succeeded, close socket */
} httpd_status_t;

typedef enum {			/* Supported verbs: */
    VERB_GET = 1,		/*  GET */
    VERB_HEAD = 2,		/*  HEAD */
    VERB_POST = 4,		/*  POST */
    VERB_OTHER = 8		/*  anything else */
} verb_t;

/* Registration functions. */
typedef httpd_status_t reg_dyn_t(const char *uri, void *dhandle);
void *httpd_register_dir(const char *path, const char *desc);
void *httpd_register_fixed(const char *path, const char *desc,
	content_t content_type, const char *content_str, unsigned flags,
	const char *fixed);
void *httpd_register_fixed_binary(const char *path, const char *desc,
	content_t content_type, const char *content_str, unsigned flags,
	const unsigned char *fixed, unsigned lenrth);
void *httpd_register_dyn_term(const char *path, const char *desc,
	content_t content_type, const char *content_str, verb_t verbs,
	unsigned flags, reg_dyn_t *dyn);
void *httpd_register_dyn_nonterm(const char *path, const char *desc,
	content_t content_type, const char *content_str, verb_t verbs,
	unsigned flags, reg_dyn_t *dyn);
void httpd_set_alias(void *nhandle, const char *text);

/* Called from the main logic. */
void *httpd_mhandle(void *dhandle);
void *httpd_new(void *mhandle, const char *client_name);
httpd_status_t httpd_input(void *dhandle, const char *data, size_t len);
void httpd_close(void *dhandle, const char *why);

/* Callable from methods. */
httpd_status_t httpd_dyn_complete(void *dhandle,
	const char *format, ...) printflike(2, 3);
httpd_status_t httpd_dyn_error(void *dhandle, content_t content_type,
	int status_code, json_t *jresult, const char *format, ...)
	printflike(5, 6);
char *html_quote(const char *text);
char *uri_quote(const char *text);
const char *httpd_fetch_query(void *dhandle, const char *name);

content_t httpd_content_type(void *dhandle);
char *httpd_content(void *dhandle);
verb_t httpd_verb(void *dhandle);
char *percent_decode(const char *uri, size_t len, bool plus);

bool httpd_waiting(void *dhandle, ioid_t id);
