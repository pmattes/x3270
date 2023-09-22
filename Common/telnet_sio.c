/*
 * Copyright (c) 2017-2023 Paul Mattes.
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
 *	telnet_sio.c
 *		Glue between telnet and secure I/O (sio).
 */

#include "globals.h"

#include <stdint.h>

#include "appres.h"

#include "popups.h"
#include "sio.h"
#include "telnet_sio.h"
#include "task.h"
#include "tls_passwd_gui.h"
#include "trace.h"

/*
 * Password cache.
 * We remember interactive passwords, keyed by the configuration (cert_file,
 * cert_file_type, chain_file, key_file, key_file_type, client_cert).
 */
typedef struct _password_cache {
    struct _password_cache *next;
    tls_config_t config;
    char *password;
} password_cache_t;

static password_cache_t *password_cache;

/* Compare two possibly-null strings. */
static bool nstreq(const char *a, const char *b)
{
    return (a == NULL && b == NULL) ||
	(a != NULL && b != NULL && !strcmp(a, b));
}

/* Duplicate a possibly-null string. */
static char *nstrdup(const char *a)
{
    return (a != NULL)? NewString(a): NULL;
}

/* Add or update an entry in the password cache. */
static void
add_to_cache(tls_config_t *config, const char *password)
{
    password_cache_t *p;

    for (p = password_cache; p != NULL; p++) {
	if (nstreq(p->config.cert_file, config->cert_file) &&
		nstreq(p->config.cert_file_type, config->cert_file_type) &&
		nstreq(p->config.chain_file, config->chain_file) &&
		nstreq(p->config.key_file, config->key_file) &&
		nstreq(p->config.key_file_type, config->key_file_type) &&
		nstreq(p->config.client_cert, config->client_cert)) {

	    /* Overwrite existing entry. */
	    Replace(p->password, NewString(password));
	    return;
	}
    }

    /* Create a new entry. */
    p = (password_cache_t *)Malloc(sizeof(password_cache_t));
    memset(&p->config, 0, sizeof(tls_config_t));
    p->config.cert_file = nstrdup(config->cert_file);
    p->config.cert_file_type = nstrdup(config->cert_file_type);
    p->config.chain_file = nstrdup(config->chain_file);
    p->config.key_file = nstrdup(config->key_file);
    p->config.key_file_type = nstrdup(config->key_file_type);
    p->config.client_cert = nstrdup(config->client_cert);
    p->password = NewString(password);
    p->next = password_cache;
    password_cache = p;
}

/* Look up an entry in the password cache. */
static char *
lookup_cache(tls_config_t *config)
{
    password_cache_t *p;

    for (p = password_cache; p != NULL; p++) {
	if (nstreq(p->config.cert_file, config->cert_file) &&
		nstreq(p->config.cert_file_type, config->cert_file_type) &&
		nstreq(p->config.chain_file, config->chain_file) &&
		nstreq(p->config.key_file, config->key_file) &&
		nstreq(p->config.key_file_type, config->key_file_type) &&
		nstreq(p->config.client_cert, config->client_cert)) {

	    return p->password;
	}
    }
    return NULL;
}

/*
 * Set up TLS, integrated with password prompting.
 */
sio_t
sio_init_wrapper(const char *password, bool force_no_verify, char *accept,
	bool *pending)
{
    char password_buf[1024];
    sio_t s;
    bool again = false;
    static tls_config_t *config = NULL;

    /* Create a temporary config for sio to consume. */
    Replace(config, Malloc(sizeof(tls_config_t)));
    memcpy(config, &appres.tls, sizeof(tls_config_t));
    if (force_no_verify) {
	config->verify_host_cert = false;
    }
    if (accept) {
	config->accept_hostname = accept;
    }

    if (password == NULL) {
	password = lookup_cache(&appres.tls);
	if (password != NULL) {
	    vtrace("TLS: Using cached password\n");
	}
    } else {
	add_to_cache(&appres.tls, password);
    }

    *pending = false;
    while (true) {
	sio_init_ret_t ret = sio_init(config, password, &s);

	switch (ret) {
	case SI_SUCCESS:
	    return s;
	case SI_FAILURE:
	    connect_error("%s", sio_last_error());
	    return NULL;
	case SI_WRONG_PASSWORD:
	    vtrace("TLS: Password is wrong\n");
	    if (password == NULL) {
		connect_error("%s", sio_last_error());
		return NULL;
	    }
	    again = true;
	    /* else fall through, letting them enter another password */
	case SI_NEED_PASSWORD:
	    switch (tls_passwd_gui_callback(password_buf,
			sizeof(password_buf), again)) {
	    case SP_SUCCESS:
		/* Got it right away. */
		vtrace("TLS: Password needed, supplied by GUI\n");
		password = password_buf;
		add_to_cache(&appres.tls, password);
		/* Try again. */
		break;
	    case SP_FAILURE:
		vtrace("TLS: Password needed, GUI failed\n");
		return NULL;
	    case SP_PENDING:
		vtrace("TLS: Password needed, GUI pending\n");
		*pending = true;
		return NULL;
	    case SP_NOT_SUPPORTED:
		vtrace("TLS: Password needed, GUI unavailable\n");
		connect_error("Private key password needed");
		return NULL;
	    }
	    break;
	}
    }
}
