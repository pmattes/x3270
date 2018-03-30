/*
 * Copyright (c) 2017-2018 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 *	sio_glue.c
 *		Resource and options glue logic for secure I/O.
 */

#include "globals.h"

#include <stdarg.h>

#include "appres.h"
#include "resources.h"

#include "opts.h"
#include "lazya.h"
#include "popups.h"
#include "utils.h"
#include "sio.h"
#include "sio_glue.h"
#include "sio_internal.h"
#include "telnet.h"
#include "toggles.h"
#include "varbuf.h"

/* Typedefs */
typedef struct {
    unsigned flag;
    res_t res;
} flagged_res_t;

/* Globals */

/* Statics */

/* Note: These are ordered by bitmap (flag) value, lowest to highest. */
static flagged_res_t sio_flagged_res[] = {
    { SSL_OPT_ACCEPT_HOSTNAME,
	{ ResAcceptHostname, aoffset(ssl.accept_hostname), XRM_STRING } },
    { SSL_OPT_VERIFY_HOST_CERT,
	{ ResVerifyHostCert, aoffset(ssl.verify_host_cert), XRM_BOOLEAN } },
    { SSL_OPT_STARTTLS,
	{ ResStartTls, aoffset(ssl.starttls), XRM_BOOLEAN } },
    { SSL_OPT_CA_DIR,
	{ ResCaDir, aoffset(ssl.ca_dir), XRM_STRING } },
    { SSL_OPT_CA_FILE,
	{ ResCaFile, aoffset(ssl.ca_file), XRM_STRING } },
    { SSL_OPT_CERT_FILE,
	{ ResCertFile, aoffset(ssl.cert_file), XRM_STRING } },
    { SSL_OPT_CERT_FILE_TYPE,
	{ ResCertFileType,aoffset(ssl.cert_file_type), XRM_STRING } },
    { SSL_OPT_CHAIN_FILE,
	{ ResChainFile, aoffset(ssl.chain_file), XRM_STRING } },
    { SSL_OPT_KEY_FILE,
	{ ResKeyFile, aoffset(ssl.key_file), XRM_STRING } },
    { SSL_OPT_KEY_FILE_TYPE,
	{ ResKeyFileType, aoffset(ssl.key_file_type),XRM_STRING } },
    { SSL_OPT_KEY_PASSWD,
	{ ResKeyPasswd, aoffset(ssl.key_passwd), XRM_STRING } },
    { SSL_OPT_CLIENT_CERT,
	{ ResClientCert, aoffset(ssl.client_cert), XRM_STRING } }
};
static int n_sio_flagged_res = (int)array_count(sio_flagged_res);

/*
 * Add SSL options.
 */
static void
add_ssl_opts(void)
{
    struct {
	unsigned flag;
	opt_t opt;
    } flagged_opts[] = {
	{ SSL_OPT_ACCEPT_HOSTNAME,
	    { OptAcceptHostname, OPT_STRING, false, ResAcceptHostname,
		aoffset(ssl.accept_hostname), "[DNS:]<name>",
		"Host name to accept from server certificate" } },
	{ SSL_OPT_VERIFY_HOST_CERT,
	    { OptVerifyHostCert, OPT_BOOLEAN, true, ResVerifyHostCert,
		aoffset(ssl.verify_host_cert),
		NULL, "Enable SSL/TLS host certificate validation (set by default)" } },
	{ SSL_OPT_VERIFY_HOST_CERT,
	    { OptNoVerifyHostCert, OPT_BOOLEAN, false, ResVerifyHostCert,
		aoffset(ssl.verify_host_cert),
		NULL, "Disable SSL/TLS host certificate validation" } },
	{ SSL_OPT_CA_DIR,
	    { OptCaDir, OPT_STRING, false, ResCaDir, aoffset(ssl.ca_dir),
		"<directory>","SSL/TLS CA certificate database directory" } },
	{ SSL_OPT_CA_FILE,
	    { OptCaFile, OPT_STRING, false, ResCaFile, aoffset(ssl.ca_file),
		"<filename>", "SSL/TLS CA certificate file" } },
	{ SSL_OPT_CERT_FILE,
	    { OptCertFile, OPT_STRING, false, ResCertFile,
		aoffset(ssl.cert_file),
		"<filename>", "SSL/TLS client certificate file" } },
	{ SSL_OPT_CERT_FILE_TYPE,
	    { OptCertFileType, OPT_STRING, false, ResCertFileType,
		aoffset(ssl.cert_file_type),
		"pem|asn1", "SSL/TLS client certificate file type" } },
	{ SSL_OPT_CHAIN_FILE,
	    { OptChainFile,OPT_STRING, false,ResChainFile,
		aoffset(ssl.chain_file),
		"<filename>", "SSL/TLS certificate chain file" } },
	{ SSL_OPT_KEY_FILE,
	    { OptKeyFile, OPT_STRING, false, ResKeyFile, aoffset(ssl.key_file),
		"<filename>", "Get SSL/TLS private key from <filename>" } },
	{ SSL_OPT_KEY_FILE_TYPE,
	    { OptKeyFileType, OPT_STRING, false, ResKeyFileType,
		aoffset(ssl.key_file_type),
		"pem|asn1", "SSL/TLS private key file type" } },
	{ SSL_OPT_KEY_PASSWD,
	    { OptKeyPasswd,OPT_STRING, false, ResKeyPasswd,
		aoffset(ssl.key_passwd),
		"file:<filename>|string:<text>",
		"SSL/TLS private key password" } },
	{ SSL_OPT_CLIENT_CERT,
	    { OptClientCert, OPT_STRING, false, ResClientCert,
		aoffset(ssl.client_cert),
		"<name>", "SSL/TLS client certificate name" } }
    };
    int n_opts = (int)array_count(flagged_opts);
    unsigned n_ssl_opts = 0;
    opt_t *ssl_opts;
    int add_ix = 0;

    /* Fetch the list from the implementation. */
    unsigned supported_options = sio_all_options_supported();

    /* Match options against the supported ones. */
    FOREACH_SSL_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_opts; j++) {
		if (flagged_opts[j].flag == opt) {
		    n_ssl_opts++;
		}
	    }
	}
    } FOREACH_SSL_OPTS_END(opt);

    if (!n_ssl_opts) {
	return;
    }

    /* Construct the list of options to add. */
    ssl_opts = (opt_t *)Malloc(n_ssl_opts * sizeof(opt_t));
    FOREACH_SSL_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_opts; j++) {
		if (flagged_opts[j].flag == opt) {
		    ssl_opts[add_ix++] = flagged_opts[j].opt; /* struct copy */
		}
	    }
	}
    } FOREACH_SSL_OPTS_END(opt);

    /* Add them. */
    register_opts(ssl_opts, n_ssl_opts);
}

static void
add_ssl_resources(void)
{
    unsigned n_ssl_res = 0;
    res_t *ssl_res;
    int add_ix = 0;
    int i;

    /* Fetch the list from the implementation. */
    unsigned supported_options = sio_all_options_supported();

    /* Match options against the supported ones. */
    FOREACH_SSL_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_sio_flagged_res; j++) {
		if (sio_flagged_res[j].flag == opt) {
		    n_ssl_res++;
		    break;
		}
	    }
	}
    } FOREACH_SSL_OPTS_END(opt);

    if (!n_ssl_res) {
	return;
    }

    /* Construct the list of resources to add. */
    ssl_res = (res_t *)Malloc(n_ssl_res * sizeof(res_t));
    i = 0;
    FOREACH_SSL_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_sio_flagged_res; j++) {
		if (sio_flagged_res[j].flag == opt) {
		    ssl_res[add_ix++] = sio_flagged_res[j].res; /* struct copy */
		}
	    }
	}
	i++;
    } FOREACH_SSL_OPTS_END(opt);

    /* Add them. */
    register_resources(ssl_res, n_ssl_res);
}

/*
 * Translate an option flag to its name.
 */
static const char *
sio_option_name(unsigned option)
{
    int i = 0;

    FOREACH_SSL_OPTS(opt) {
	if (option & opt) {
	    return sio_flagged_res[i].res.name;
	}
	i++;
    } FOREACH_SSL_OPTS_END(opt);
    return NULL;
}

/*
 * Translate an option to its flag value.
 */
static unsigned
sio_toggle_value(const char *name)
{
    int i = 0;

    FOREACH_SSL_OPTS(opt) {
	if (!strcasecmp(sio_flagged_res[i].res.name, name)) {
	    return opt;
	}
	i++;
    } FOREACH_SSL_OPTS_END(opt);
    return 0;
}

/* Translate supported SSL options to a list of names. */
char *
sio_option_names(void)
{
    unsigned options = sio_all_options_supported();
    varbuf_t v;
    char *sep = "";

    vb_init(&v);
    FOREACH_SSL_OPTS(opt) {
	if (options & opt) {
	    const char *opt_name = sio_option_name(opt);

	    if (opt_name != NULL) {
		vb_appendf(&v, "%s%s", sep, opt_name);
		sep = " ";
	    }
	}
    } FOREACH_SSL_OPTS_END(opt);

    return lazya(vb_consume(&v));
}

/*
 * Parse a Boolean value.
 */
static bool
parse_bool(const char *value, bool *res)
{
    if (!strcasecmp(value, "true")) {
	*res = true;
	return true;
    }
    if (!strcasecmp(value, "false")) {
	*res = false;
	return true;
    }
    return false;
}

/*
 * Toggle for TLS parameters.
 */
static bool
sio_toggle(const char *name, const char *value, char **canonical_value)
{
    bool b;

    if (cstate != NOT_CONNECTED) {
	popup_an_error("Toggle(%s): Cannot change while connected", name);
	return false;
    }

    /*
     * Many of these are memory leaks, so if someone changes them enough,
     * we will run out of memory.
     *
     * At some point, it would make sense to copy every string in appres into
     * the heap at init time, so they can be replaced without leaking.
     */
    switch (sio_toggle_value(name)) {
    case SSL_OPT_ACCEPT_HOSTNAME:
	appres.ssl.accept_hostname = value[0]? NewString(value) : NULL;
	break;
    case SSL_OPT_VERIFY_HOST_CERT:
	if (!parse_bool(value, &b)) {
	    popup_an_error("Toggle(%s): Invalid value '%s'", name, value);
	    return false;
	}
	appres.ssl.verify_host_cert = b;
	break;
    case SSL_OPT_STARTTLS:
	if (!parse_bool(value, &b)) {
	    popup_an_error("Toggle(%s): Invalid value '%s'", name, value);
	    return false;
	}
	appres.ssl.starttls = b;
	break;
    case SSL_OPT_CA_DIR:
	appres.ssl.ca_dir = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_CA_FILE:
	appres.ssl.ca_file = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_CERT_FILE:
	appres.ssl.cert_file = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_CERT_FILE_TYPE:
	appres.ssl.cert_file_type = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_CHAIN_FILE:
	appres.ssl.chain_file = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_KEY_FILE:
	appres.ssl.key_file = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_KEY_FILE_TYPE:
	appres.ssl.key_file_type = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_KEY_PASSWD:
	appres.ssl.key_passwd = value[0]? NewString(value): NULL;
	break;
    case SSL_OPT_CLIENT_CERT:
	appres.ssl.client_cert = value[0]? NewString(value): NULL;
	break;
    default:
	popup_an_error("Toggle(%s): Unknown name", name);
	return false;
    }

    return true;
}

/*
 * Register SSL-specific options and resources.
 */
void
sio_glue_register(void)
{
    unsigned supported_options = sio_all_options_supported();

    add_ssl_opts();
    add_ssl_resources();

    FOREACH_SSL_OPTS(opt) {
	if (supported_options & opt) {
	    register_extended_toggle(sio_option_name(opt), sio_toggle, NULL,
		    NULL);
	}
    } FOREACH_SSL_OPTS_END(opt);
}
