/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes nor the names of their
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
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
 *	tls_config.h
 *		Secure I/O configuration.
 */

typedef struct {
    /* Required options. */
    char	*accept_hostname;
    bool	 verify_host_cert;
    bool	 starttls;

    /* Options that might or might not be supported. */
    char	*ca_dir;
    char	*ca_file;
    char	*cert_file;
    char	*cert_file_type;
    char	*chain_file;
    char	*key_file;
    char	*key_file_type;
    char	*key_passwd;
    char	*client_cert;
    char	*min_protocol;
    char	*max_protocol;
    char	*security_level;
} tls_config_t;

/* Required options. */
#define TLS_OPT_ACCEPT_HOSTNAME		0x00000001
#define TLS_OPT_VERIFY_HOST_CERT	0x00000002
#define TLS_OPT_STARTTLS		0x00000004
#define TLS_REQUIRED_OPTS \
    (TLS_OPT_ACCEPT_HOSTNAME | TLS_OPT_VERIFY_HOST_CERT | TLS_OPT_STARTTLS)

/* Options optionally supported by specific implementations. */
#define TLS_OPT_CA_DIR			0x00000008
#define TLS_OPT_CA_FILE			0x00000010
#define TLS_OPT_CERT_FILE		0x00000020
#define TLS_OPT_CERT_FILE_TYPE		0x00000040
#define TLS_OPT_CHAIN_FILE		0x00000080
#define TLS_OPT_KEY_FILE		0x00000100
#define TLS_OPT_KEY_FILE_TYPE		0x00000200
#define TLS_OPT_KEY_PASSWD		0x00000400
#define TLS_OPT_CLIENT_CERT		0x00000800
#define TLS_OPT_MIN_PROTOCOL		0x00001000
#define TLS_OPT_MAX_PROTOCOL		0x00002000
#define TLS_OPT_SECURITY_LEVEL		0x00004000

#define TLS_OPTIONAL_OPTS \
    (TLS_OPT_CA_DIR | TLS_OPT_CA_FILE | TLS_OPT_CERT_FILE | \
     TLS_OPT_CERT_FILE_TYPE | TLS_OPT_CHAIN_FILE | TLS_OPT_KEY_FILE | \
     TLS_OPT_KEY_FILE_TYPE | TLS_OPT_KEY_PASSWD | TLS_OPT_CLIENT_CERT | \
     TLS_OPT_MIN_PROTOCOL | TLS_OPT_MAX_PROTOCOL | TLS_OPT_SECURITY_LEVEL)

#define TLS_ALL_OPTS	(TLS_REQUIRED_OPTS | TLS_OPTIONAL_OPTS)

#define FOREACH_TLS_OPTS(opt) { \
	unsigned opt = 1; \
	while (TLS_ALL_OPTS & opt) {
#define FOREACH_TLS_OPTS_END(opt) \
	    opt <<= 1; \
	} \
    }
