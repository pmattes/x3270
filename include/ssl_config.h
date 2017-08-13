/*
 * Copyright (c) 1993-2012, 2016-2017 Paul Mattes.
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
 *	ssl_config.h
 *		Secure I/O configuration.
 */

typedef struct {
    /* Required options. */
    char	*accept_hostname;
    bool	 verify_host_cert;
    bool	 tls;

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
} ssl_config_t;

/*
 * Options optionally supported by specific implementations.
 * Options not listed here must be supported by all implementations.
 */
#define SSL_OPT_CA_DIR			0x00000001
#define SSL_OPT_CA_FILE			0x00000002
#define SSL_OPT_CERT_FILE		0x00000004
#define SSL_OPT_CERT_FILE_TYPE		0x00000008
#define SSL_OPT_CHAIN_FILE		0x00000010
#define SSL_OPT_KEY_FILE		0x00000020
#define SSL_OPT_KEY_FILE_TYPE		0x00000040
#define SSL_OPT_KEY_PASSWD		0x00000080
#define SSL_OPT_CLIENT_CERT		0x00000100

#define SSL_ALL_OPTS \
    (SSL_OPT_CA_DIR | SSL_OPT_CA_FILE | SSL_OPT_CERT_FILE \
     | SSL_OPT_CERT_FILE_TYPE | SSL_OPT_CHAIN_FILE | SSL_OPT_KEY_FILE \
     | SSL_OPT_KEY_FILE_TYPE | SSL_OPT_KEY_PASSWD | SSL_OPT_CLIENT_CERT)
