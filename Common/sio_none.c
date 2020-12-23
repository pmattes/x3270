/*
 * Copyright (c) 2017, 2019-2020 Paul Mattes.
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
 *	sio_none.c
 *		Secure I/O non-support.
 */

#include "globals.h"

#include "tls_config.h"

#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "sioc.h"

bool
sio_supported(void)
{
    return false;
}

sio_init_ret_t
sio_init(tls_config_t *config, const char *password, sio_t *sio_ret)
{
    sioc_set_error("TLS not supported");
    *sio_ret = NULL;
    return SI_FAILURE;
}

sio_negotiate_ret_t
sio_negotiate(sio_t sio, socket_t sock, const char *hostname, bool *data)
{
    sioc_set_error("TLS not supported");
    *data = false;
    return SIG_FAILURE;
}

int
sio_read(sio_t sio, char *buf, size_t buflen)
{
    sioc_set_error("TLS not supported");
    return SIO_FATAL_ERROR;
}

int
sio_write(sio_t sio, const char *buf, size_t buflen)
{
    sioc_set_error("TLS not supported");
    return SIO_FATAL_ERROR;
}

void
sio_close(sio_t sio)
{
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
sio_server_subject_names(sio_t sio)
{
    return "None";
}

const char *
sio_provider(void)
{
    return "None";
}
