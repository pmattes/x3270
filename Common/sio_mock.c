/*
 * Copyright (c) 2017-2024 Paul Mattes.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, "AS IS" AND ANY EXPRESS OR
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
 *	sio_mock.c
 *		Secure I/O testing support.
 */

#include "globals.h"

#include "tls_config.h"

#include "sio.h"
#include "sioc.h"

/*
 * This module's behavior is controlled by an environment variable, SIO_MOCK.
 */
#define SIO_MOCK	"SIO_MOCK"

typedef enum {
    SM_UNKNOWN,
    SM_UNSUPPORTED,		/* No sio support */
    SM_INIT_FAIL,		/* Fail sio_init */
    SM_INIT_WRONG_PASSWORD,	/* sio_init always says wrong password */
    SM_INIT_WRONG_PASSWORD_ONCE,/* sio_init says wrong password once */
    SM_INIT_NEED_PASSWORD,	/* sio_init needs password once */
    SM_NEGOTIATE_FAIL,		/* Fail sio_negotiate */
    SM_READ_FAIL,		/* Fail sio_read */
    SM_READ_EOF,		/* sio_read always returns EOF */
    SM_READ_EWOULDBLOCK,	/* sio_read always returns EWOULDBLOCK */
    SM_WRITE_FAIL,		/* Fail sio_write */
    SM_TRANSPARENT,		/* No TLS at all */
    N_MODES
} fail_mode_t;
fail_mode_t fail_mode = SM_UNKNOWN;

static const char *fail_mode_names[] = {
    "UNKNOWN",
    "UNSUPPORTED",
    "INIT_FAIL",
    "INIT_WRONG_PASSWORD",
    "INIT_WRONG_PASSWORD_ONCE",
    "INIT_NEED_PASSWORD",
    "NEGOTIATE_FAIL",
    "READ_FAIL",
    "READ_EOF",
    "READ_EWOULDBLOCK",
    "WRITE_FAIL",
    "TRANSPARENT"
};

typedef struct {
    socket_t sock;
} test_sio_t;

static int wrongs;

static void
set_fail_mode(void)
{
    const char *s = getenv(SIO_MOCK);
    int i;

    if (fail_mode != SM_UNKNOWN) {
	return;
    }

    if (s == NULL) {
	fail_mode = SM_UNSUPPORTED;
	return;
    }

    for (i = SM_UNKNOWN + 1; i < N_MODES; i++) {
	if (!strcmp(s, fail_mode_names[i])) {
	    fail_mode = i;
	    return;
	}
    }

    fprintf(stderr, "Unrecognized %s mode '%s'\n", SIO_MOCK, s);
    fail_mode = SM_UNSUPPORTED;
}

bool
sio_supported(void)
{
    set_fail_mode();
    return fail_mode != SM_UNSUPPORTED;
}

sio_init_ret_t
sio_init(tls_config_t *config, const char *password, sio_t *sio_ret)
{
    test_sio_t *t;

    set_fail_mode();
    switch (fail_mode) {
    case SM_UNSUPPORTED:
	sioc_set_error("TLS not supported");
	*sio_ret = NULL;
	return SI_FAILURE;
    case SM_INIT_FAIL:
	sioc_set_error("Not feeling well");
	*sio_ret = NULL;
	return SI_FAILURE;
    case SM_INIT_WRONG_PASSWORD:
	sioc_set_error("Password is always wrong");
	*sio_ret = NULL;
	return SI_WRONG_PASSWORD;
    case SM_INIT_NEED_PASSWORD:
    case SM_INIT_WRONG_PASSWORD_ONCE:
    default:
	if (fail_mode == SM_INIT_NEED_PASSWORD) {
	    if (!wrongs++) {
		*sio_ret = NULL;
		return SI_NEED_PASSWORD;
	    }
	} else if (fail_mode == SM_INIT_WRONG_PASSWORD_ONCE) {
	    if (!wrongs) {
		wrongs++;
		*sio_ret = NULL;
		return SI_NEED_PASSWORD;
	    } else if (wrongs == 1) {
		wrongs++;
		*sio_ret = NULL;
		return SI_WRONG_PASSWORD;
	    }
	}

	t = (test_sio_t *)Malloc(sizeof(test_sio_t));
	memset(t, 0, sizeof(test_sio_t));
	t->sock = INVALID_SOCKET;
	*sio_ret = (sio_t *)t;
	return SI_SUCCESS;
    }
}

sio_negotiate_ret_t
sio_negotiate(sio_t sio, socket_t sock, const char *hostname, bool *data)
{
    test_sio_t *t = (test_sio_t *)sio;

    set_fail_mode();
    *data = false;
    switch (fail_mode) {
    case SM_UNSUPPORTED:
	sioc_set_error("TLS not supported");
	return SIG_FAILURE;
    case SM_NEGOTIATE_FAIL:
	sioc_set_error("Host does not like us");
	return SIG_FAILURE;
    default:
	t->sock = sock;
	return SIG_SUCCESS;
    }
}

int
sio_read(sio_t sio, char *buf, size_t buflen)
{
    test_sio_t *t = (test_sio_t *)sio;

    set_fail_mode();
    switch (fail_mode) {
    case SM_UNSUPPORTED:
	sioc_set_error("TLS not supported");
	return false;
    case SM_READ_FAIL:
	sioc_set_error("Socket not feeling well");
	return SIO_FATAL_ERROR;
    case SM_READ_EOF:
	return SIO_EOF;
    case SM_READ_EWOULDBLOCK:
	return SIO_EWOULDBLOCK;
    default:
	return recv(t->sock, buf, buflen, 0);
    }
}

int
sio_write(sio_t sio, const char *buf, size_t buflen)
{
    test_sio_t *t = (test_sio_t *)sio;

    set_fail_mode();
    switch (fail_mode) {
    case SM_UNSUPPORTED:
	sioc_set_error("TLS not supported");
	return false;
    case SM_WRITE_FAIL:
	sioc_set_error("Socket not feeling well");
	return SIO_FATAL_ERROR;
    default:
	return send(t->sock, buf, buflen, 0);
    }
}

void
sio_close(sio_t sio)
{
    Free(sio);
}

bool
sio_secure_unverified(sio_t sio)
{
    return false;
}

unsigned
sio_options_supported(void)
{
    return 0;
}

const char *
sio_session_info(sio_t sio)
{
    return "None";
}

const char *
sio_server_cert_info(sio_t sio)
{
    return "None";
}

const char *
sio_provider(void)
{
    return "Mock";
}
