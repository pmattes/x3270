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
 *      httpd-io.h
 *              x3270 webserver, header file for I/O module
 */

typedef enum {
    SENDTO_PENDING = 0, /* command queued */
    SENDTO_COMPLETE = 1, /* command executed (successfully or not) */
    SENDTO_INVALID = -1,/* invalid command */
    SENDTO_FAILURE = -2 /* IPC failure */
} sendto_t;

typedef enum {
    SC_SUCCESS,         /* command succeeded */
    SC_USER_ERROR,      /* user errror (400) */
    SC_SYSTEM_ERROR     /* system error (500) */
} sendto_cbs_t;

typedef struct hio_listener hio_listener_t;

/* Callback function for hio_to3270(). */
typedef void sendto_callback_t(void *, sendto_cbs_t, const char *buf,
	size_t len, json_t *jresult, const char *slbuf, size_t sl_len);

sendto_t hio_to3270(const char *cmd, sendto_callback_t *callback,
	void *dhandle, content_t request_content_type,
	content_t return_content_type, char **errmsg);

void hio_send(void *mhandle, const char *buf, size_t len);

void hio_async_done(void *dhandle, httpd_status_t rv);

void hio_init(struct sockaddr *sa, socklen_t sa_len);
hio_listener_t *hio_init_x(struct sockaddr *sa, socklen_t sa_len);
void hio_stop(void);
void hio_stop_x(hio_listener_t *l);
void hio_register(void);
content_t hio_content_type(void *dhandle);
char *hio_content(void *dhandle);
void hio_error_timeout(ioid_t id);
