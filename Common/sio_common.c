/*
 * Copyright (c) 2017 Paul Mattes.
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
 *	sio_common.c
 *		Common logic for secure I/O.
 */

#include "globals.h"

#include <stdarg.h>

#include "appres.h"
#include "resources.h"

#include "opts.h"
#include "utils.h"
#include "sio.h"

/* Typedefs */

/* Statics */

/* Globals */

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
    int n_opts = (int)(sizeof(flagged_opts) / sizeof(flagged_opts[0]));
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
    struct {
	unsigned flag;
	res_t res;
    } flagged_res[] = {
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
    int n_res = (int)(sizeof(flagged_res) / sizeof(flagged_res[0]));
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

	    for (j = 0; j < n_res; j++) {
		if (flagged_res[j].flag == opt) {
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

	    for (j = 0; j < n_res; j++) {
		if (flagged_res[j].flag == opt) {
		    ssl_res[add_ix++] = flagged_res[j].res; /* struct copy */
		}
	    }
	}
	i++;
    } FOREACH_SSL_OPTS_END(opt);

    /* Add them. */
    register_resources(ssl_res, n_ssl_res);
}

/*
 * Register SSL-specific options and resources.
 */
void
sio_register(void)
{
    add_ssl_opts();
    add_ssl_resources();
}

/*
 * Translate an option flag to its name.
 */
const char *
sio_option_name(unsigned option)
{
    /* Option names, in bitmap order. */
    static const char *sio_option_names[] = {
	ResAcceptHostname,
	ResVerifyHostCert,
	ResStartTls,
	ResCaDir,
	ResCaFile,
	ResCertFile,
	ResCertFileType,
	ResChainFile,
	ResKeyFile,
	ResKeyFileType,
	ResKeyPasswd,
	ResClientCert
    };
    int i = 0;

    FOREACH_SSL_OPTS(opt) {
	if (option & opt) {
	    return sio_option_names[i];
	}
	i++;
    } FOREACH_SSL_OPTS_END(opt);
    return NULL;
}
