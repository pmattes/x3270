/*
 * Copyright (c) 2017-2024 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	sio.h
 *		External definitions for functions and data for secure I/O,
 *		implemented in various platform-specific ways.
 */

/* Special return values from sio_read and sio_write. */
#define SIO_EOF			0
#define SIO_FATAL_ERROR		(-1)
#define SIO_EWOULDBLOCK		(-2)

/* Return values from sio_init. */
typedef enum {
    SI_SUCCESS,		/* success */
    SI_FAILURE,		/* failure, reason in sio_last_error  */
    SI_NEED_PASSWORD,	/* need a password */
    SI_WRONG_PASSWORD	/* password is wrong */
} sio_init_ret_t;

/* Return values from sio_negotiate. */
typedef enum {
    SIG_SUCCESS,	/* success */
    SIG_FAILURE,	/* failure */
    SIG_WANTMORE	/* more input needed */
} sio_negotiate_ret_t;

/* TLS protocol versions, used to implement max/min options. */
typedef enum {
    SIP_SSL2,		/* SSL 2.0 (dangerously obsolete) */
    SIP_SSL3,		/* SSL 3.0 (dangerously obsolete) */
    SIP_TLS1,		/* TLS 1.0 */
    SIP_TLS1_1,		/* TLS 1.1 */
    SIP_TLS1_2,		/* TLS 1.2 */
    SIP_TLS1_3		/* TLS 1.3 */
} sio_protocol_t;

typedef void *sio_t;

/* Implemented in common code. */
const char *sio_last_error(void);
unsigned sio_all_options_supported(void);

/* Implemented in platform-specific code. */
bool sio_supported(void);
const char *sio_provider(void);
unsigned sio_options_supported(void);
sio_init_ret_t sio_init(tls_config_t *config, const char *password,
	sio_t *sio_ret);
sio_negotiate_ret_t sio_negotiate(sio_t sio, socket_t sock,
	const char *hostname, bool *data);
int sio_read(sio_t sio, char *buf, size_t buflen);
int sio_write(sio_t sio, const char *buf, size_t buflen);
void sio_close(sio_t sio);
bool sio_secure_unverified(sio_t sio);
const char *sio_session_info(sio_t sio);
const char *sio_server_cert_info(sio_t sio);
const char *sio_server_subject_names(sio_t sio);
