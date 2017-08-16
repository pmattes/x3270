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
 *	sio_internal.c
 *		Common internal data and logic for secure I/O.
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
#include "sio_internal.h"
#include "telnet.h"
#include "varbuf.h"

/* Typedefs */

/* Statics */

/* Globals */
flagged_res_t sio_flagged_res[] = {
    { SSL_OPT_ACCEPT_HOSTNAME,
	{ ResAcceptHostname, aoffset(ssl.accept_hostname), XRM_STRING } },
    { SSL_OPT_VERIFY_HOST_CERT,
	{ ResVerifyHostCert, aoffset(ssl.verify_host_cert), XRM_BOOLEAN } },
    { SSL_OPT_TLS,
	{ ResTls, aoffset(ssl.tls), XRM_BOOLEAN } },
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
int n_sio_flagged_res = (int)array_count(sio_flagged_res);

/*
 * Translate an option flag to its name.
 */
static const char *
sio_option_name(unsigned option)
{
    /* Option names, in bitmap order. */
    static const char *sio_option_names[] = {
	ResAcceptHostname,
	ResVerifyHostCert,
	ResTls,
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
