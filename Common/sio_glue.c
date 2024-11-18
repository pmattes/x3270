/*
 * Copyright (c) 2017-2024 Paul Mattes.
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

#include "boolstr.h"
#include "opts.h"
#include "popups.h"
#include "utils.h"
#include "sio.h"
#include "sio_glue.h"
#include "sio_internal.h"
#include "telnet.h"
#include "txa.h"
#include "toggles.h"
#include "varbuf.h"

#define TLS_PROTOCOLS "SSL2|SSL3|TLS1|TLS1_1|TLS1_2|TLS1_3"

/* Typedefs */
typedef struct {
    unsigned flag;
    res_t res;
} flagged_res_t;

/* Globals */

/* Statics */

/* Note: These are ordered by bitmap (flag) value, lowest to highest. */
static flagged_res_t sio_flagged_res[] = {
    { TLS_OPT_ACCEPT_HOSTNAME,
	{ ResAcceptHostname, aoffset(tls.accept_hostname), XRM_STRING } },
    { TLS_OPT_VERIFY_HOST_CERT,
	{ ResVerifyHostCert, aoffset(tls.verify_host_cert), XRM_BOOLEAN } },
    { TLS_OPT_STARTTLS,
	{ ResStartTls, aoffset(tls.starttls), XRM_BOOLEAN } },
    { TLS_OPT_CA_DIR,
	{ ResCaDir, aoffset(tls.ca_dir), XRM_STRING } },
    { TLS_OPT_CA_FILE,
	{ ResCaFile, aoffset(tls.ca_file), XRM_STRING } },
    { TLS_OPT_CERT_FILE,
	{ ResCertFile, aoffset(tls.cert_file), XRM_STRING } },
    { TLS_OPT_CERT_FILE_TYPE,
	{ ResCertFileType,aoffset(tls.cert_file_type), XRM_STRING } },
    { TLS_OPT_CHAIN_FILE,
	{ ResChainFile, aoffset(tls.chain_file), XRM_STRING } },
    { TLS_OPT_KEY_FILE,
	{ ResKeyFile, aoffset(tls.key_file), XRM_STRING } },
    { TLS_OPT_KEY_FILE_TYPE,
	{ ResKeyFileType, aoffset(tls.key_file_type),XRM_STRING } },
    { TLS_OPT_KEY_PASSWD,
	{ ResKeyPasswd, aoffset(tls.key_passwd), XRM_STRING } },
    { TLS_OPT_CLIENT_CERT,
	{ ResClientCert, aoffset(tls.client_cert), XRM_STRING } },
    { TLS_OPT_MIN_PROTOCOL,
	{ ResTlsMinProtocol, aoffset(tls.min_protocol), XRM_STRING } },
    { TLS_OPT_MAX_PROTOCOL,
	{ ResTlsMaxProtocol, aoffset(tls.max_protocol), XRM_STRING } },
    { TLS_OPT_SECURITY_LEVEL,
	{ ResTlsSecurityLevel, aoffset(tls.security_level), XRM_STRING } },
};
static int n_sio_flagged_res = (int)array_count(sio_flagged_res);

/*
 * Add TLS options.
 */
static void
add_tls_opts(void)
{
    struct {
	unsigned flag;
	opt_t opt;
    } flagged_opts[] = {
	{ TLS_OPT_ACCEPT_HOSTNAME,
	    { OptAcceptHostname, OPT_STRING, false, ResAcceptHostname,
		aoffset(tls.accept_hostname), "[DNS:]<name>",
		"Host name to accept from server certificate" } },
	{ TLS_OPT_VERIFY_HOST_CERT,
	    { OptVerifyHostCert, OPT_BOOLEAN, true, ResVerifyHostCert,
		aoffset(tls.verify_host_cert),
		NULL, "Enable TLS host certificate validation (set by default)" } },
	{ TLS_OPT_VERIFY_HOST_CERT,
	    { OptNoVerifyHostCert, OPT_BOOLEAN, false, ResVerifyHostCert,
		aoffset(tls.verify_host_cert),
		NULL, "Disable TLS host certificate validation" } },
	{ TLS_OPT_CA_DIR,
	    { OptCaDir, OPT_STRING, false, ResCaDir, aoffset(tls.ca_dir),
		"<directory>","TLS CA certificate database directory" } },
	{ TLS_OPT_CA_FILE,
	    { OptCaFile, OPT_STRING, false, ResCaFile, aoffset(tls.ca_file),
		"<filename>", "TLS CA certificate file" } },
	{ TLS_OPT_CERT_FILE,
	    { OptCertFile, OPT_STRING, false, ResCertFile,
		aoffset(tls.cert_file),
		"<filename>", "TLS client certificate file" } },
	{ TLS_OPT_CERT_FILE_TYPE,
	    { OptCertFileType, OPT_STRING, false, ResCertFileType,
		aoffset(tls.cert_file_type),
		"pem|asn1", "TLS client certificate file type" } },
	{ TLS_OPT_CHAIN_FILE,
	    { OptChainFile,OPT_STRING, false,ResChainFile,
		aoffset(tls.chain_file),
		"<filename>", "TLS certificate chain file" } },
	{ TLS_OPT_KEY_FILE,
	    { OptKeyFile, OPT_STRING, false, ResKeyFile, aoffset(tls.key_file),
		"<filename>", "Get TLS private key from <filename>" } },
	{ TLS_OPT_KEY_FILE_TYPE,
	    { OptKeyFileType, OPT_STRING, false, ResKeyFileType,
		aoffset(tls.key_file_type),
		"pem|asn1", "TLS private key file type" } },
	{ TLS_OPT_KEY_PASSWD,
	    { OptKeyPasswd,OPT_STRING, false, ResKeyPasswd,
		aoffset(tls.key_passwd),
		"file:<filename>|string:<text>",
		"TLS private key password" } },
	{ TLS_OPT_CLIENT_CERT,
	    { OptClientCert, OPT_STRING, false, ResClientCert,
		aoffset(tls.client_cert),
		"<name>", "TLS client certificate name" } },
	{ TLS_OPT_MIN_PROTOCOL,
	    { OptTlsMinProtocol, OPT_STRING, false, ResTlsMinProtocol,
		aoffset(tls.min_protocol),
		TLS_PROTOCOLS, "TLS minimum protocol version" } },
	{ TLS_OPT_MAX_PROTOCOL,
	    { OptTlsMaxProtocol, OPT_STRING, false, ResTlsMaxProtocol,
		aoffset(tls.max_protocol),
		TLS_PROTOCOLS, "TLS maximum protocol version" } },
    };
    int n_opts = (int)array_count(flagged_opts);
    unsigned n_tls_opts = 0;
    opt_t *tls_opts;
    int add_ix = 0;

    /* Fetch the list from the implementation. */
    unsigned supported_options = sio_all_options_supported();

    /* Match options against the supported ones. */
    FOREACH_TLS_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_opts; j++) {
		if (flagged_opts[j].flag == opt) {
		    n_tls_opts++;
		}
	    }
	}
    } FOREACH_TLS_OPTS_END(opt);

    if (!n_tls_opts) {
	return;
    }

    /* Construct the list of options to add. */
    tls_opts = (opt_t *)Malloc(n_tls_opts * sizeof(opt_t));
    FOREACH_TLS_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_opts; j++) {
		if (flagged_opts[j].flag == opt) {
		    tls_opts[add_ix++] = flagged_opts[j].opt; /* struct copy */
		}
	    }
	}
    } FOREACH_TLS_OPTS_END(opt);

    /* Add them. */
    register_opts(tls_opts, n_tls_opts);
}

static void
add_tls_resources(void)
{
    unsigned n_tls_res = 0;
    res_t *tls_res;
    int add_ix = 0;

    /* Fetch the list from the implementation. */
    unsigned supported_options = sio_all_options_supported();

    /* Match options against the supported ones. */
    FOREACH_TLS_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_sio_flagged_res; j++) {
		if (sio_flagged_res[j].flag == opt) {
		    n_tls_res++;
		    break;
		}
	    }
	}
    } FOREACH_TLS_OPTS_END(opt);

    if (!n_tls_res) {
	return;
    }

    /* Construct the list of resources to add. */
    tls_res = (res_t *)Malloc(n_tls_res * sizeof(res_t));
    FOREACH_TLS_OPTS(opt) {
	if (supported_options & opt) {
	    int j;

	    for (j = 0; j < n_sio_flagged_res; j++) {
		if (sio_flagged_res[j].flag == opt) {
		    tls_res[add_ix++] = sio_flagged_res[j].res; /* struct copy */
		}
	    }
	}
    } FOREACH_TLS_OPTS_END(opt);

    /* Add them. */
    register_resources(tls_res, n_tls_res);
}

/*
 * Translate an option flag to its name.
 */
static const char *
sio_option_name(unsigned option)
{
    int i = 0;

    FOREACH_TLS_OPTS(opt) {
	if (option & opt) {
	    return sio_flagged_res[i].res.name;
	}
	i++;
    } FOREACH_TLS_OPTS_END(opt);
    return NULL;
}

/*
 * Translate an option flag to its appres address.
 */
static void *
sio_address(unsigned option)
{
    int i = 0;

    FOREACH_TLS_OPTS(opt) {
	if (option & opt) {
	    return sio_flagged_res[i].res.address;
	}
	i++;
    } FOREACH_TLS_OPTS_END(opt);
    return NULL;
}

/*
 * Translate an option flag to its appres type.
 */
static enum resource_type
sio_type(unsigned option)
{
    int i = 0;

    FOREACH_TLS_OPTS(opt) {
	if (option & opt) {
	    return sio_flagged_res[i].res.type;
	}
	i++;
    } FOREACH_TLS_OPTS_END(opt);
    return XRM_INT + 1; /* XXX */
}

/*
 * Translate an option to its flag value.
 */
static unsigned
sio_toggle_value(const char *name)
{
    int i = 0;

    FOREACH_TLS_OPTS(opt) {
	if (!strcasecmp(sio_flagged_res[i].res.name, name)) {
	    return opt;
	}
	i++;
    } FOREACH_TLS_OPTS_END(opt);
    return 0;
}

/* Translate supported TLS options to a list of names. */
char *
sio_option_names(void)
{
    unsigned options = sio_all_options_supported();
    varbuf_t v;
    char *sep = "";

    vb_init(&v);
    FOREACH_TLS_OPTS(opt) {
	if (options & opt) {
	    const char *opt_name = sio_option_name(opt);

	    if (opt_name != NULL) {
		vb_appendf(&v, "%s%s", sep, opt_name);
		sep = " ";
	    }
	}
    } FOREACH_TLS_OPTS_END(opt);

    return txdFree(vb_consume(&v));
}

/*
 * Toggle for TLS parameters.
 */
static toggle_upcall_ret_t
sio_toggle(const char *name, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg;
    bool b;

    bool connected = (cstate != NOT_CONNECTED);
    if (connected && !(flags & XN_DEFER)) {
	popup_an_error("%s cannot change while connected", name);
	return TU_FAILURE;
    }

    switch (sio_toggle_value(name)) {
    case TLS_OPT_ACCEPT_HOSTNAME:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.accept_hostname, value[0]? NewString(value) : NULL);
	}
	break;
    case TLS_OPT_VERIFY_HOST_CERT:
	if ((errmsg = boolstr(value, &b)) != NULL) {
	    popup_an_error("%s %s", name, errmsg);
	    return TU_FAILURE;
	}
	if (connected) {
	    toggle_save_disconnect_set(name, TrueFalse(b), ia);
	} else {
	    appres.tls.verify_host_cert = b;
	}
	break;
    case TLS_OPT_STARTTLS:
	if ((errmsg = boolstr(value, &b)) != NULL) {
	    popup_an_error("%s %s", name, errmsg);
	    return TU_FAILURE;
	}
	if (connected) {
	    toggle_save_disconnect_set(name, TrueFalse(b), ia);
	} else {
	    appres.tls.starttls = b;
	}
	break;
    case TLS_OPT_CA_DIR:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.ca_dir, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_CA_FILE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.ca_file, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_CERT_FILE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.cert_file, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_CERT_FILE_TYPE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.cert_file_type, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_CHAIN_FILE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.chain_file, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_KEY_FILE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.key_file, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_KEY_FILE_TYPE:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.key_file_type, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_KEY_PASSWD:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.key_passwd, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_CLIENT_CERT:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.client_cert, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_MIN_PROTOCOL:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.min_protocol, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_MAX_PROTOCOL:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    Replace(appres.tls.max_protocol, value[0]? NewString(value): NULL);
	}
	break;
    case TLS_OPT_SECURITY_LEVEL:
	if (connected) {
	    toggle_save_disconnect_set(name, value, ia);
	} else {
	    appres.tls.security_level = NewString(value);
	}
	break;
    default:
	popup_an_error("Unknown name '%s'", name);
	return TU_FAILURE;
    }

    return connected? TU_DEFERRED: TU_SUCCESS;
}

/*
 * Register TLS-specific options and resources.
 */
void
sio_glue_register(void)
{
    unsigned supported_options = sio_all_options_supported();

    add_tls_opts();
    add_tls_resources();

    FOREACH_TLS_OPTS(opt) {
	if (supported_options & opt) {
	    register_extended_toggle(sio_option_name(opt), sio_toggle, NULL,
		    NULL, sio_address(opt), sio_type(opt));
	}
    } FOREACH_TLS_OPTS_END(opt);
}
