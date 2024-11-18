/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	sio_openssl.c
 *		Secure I/O via the OpenSSL library.
 */

#include "globals.h"

#if defined(_WIN32) /*[*/
# error "Not supported on Windows"
#endif /*]*/

#include <stdint.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
 
#include "tls_config.h"

#include "names.h"
#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "resources.h"
#include "sioc.h"
#include "trace.h"
#include "utils.h"

#if !defined(LIBRESSL_VERSION_NUMBER) /*[*/
# if OPENSSL_VERSION_NUMBER >= 0x10100000L /*[*/
#  define OPENSSL110
# endif /*]*/
# if OPENSSL_VERSION_NUMBER >= 0x10002000L /*[*/
#  define OPENSSL102
# endif /*]*/
#endif /*]*/

#define CN_EQ		"CN = "
#define CN_EQ_SIZE	strlen(CN_EQ)

/* Globals */

/* Statics */
typedef struct {
    tls_config_t *config;
    SSL_CTX *ctx;
    SSL *con;
    socket_t sock;
    const char *hostname;
    char *accept_dnsname;
    char *password;
    bool need_password;
    bool secure_unverified;
    char *session_info;
    char *server_cert_info;
    char *server_subjects;
    bool negotiate_pending;
    bool negotiated;
} ssl_sio_t;

static ssl_sio_t *current_sio;
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*[*/
# define INFO_CONST const
#else /*][*/
# define INFO_CONST
#endif /*]*/

static int proto_map[] = { -1, SSL3_VERSION, TLS1_VERSION, TLS1_1_VERSION, TLS1_2_VERSION, TLS1_3_VERSION };

static void client_info_callback(INFO_CONST SSL *s, int where, int ret);
#if !defined(OPENSSL102) /*[*/
static char *spc_verify_cert_hostname(X509 *cert, const char *hostname);
#endif /*]*/

/* Verify function. */
static int
ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx _is_unused)
{
    /*
     * Succeed if OpenSSL already thinks the cert is okay, or if we're not
     * supposed to check.
     */
    return preverify_ok || !current_sio->config->verify_host_cert;
}

#if !defined(OPENSSL102) /*[*/
/*
 * Check the name in the host certificate.
 *
 * Returns true if the certificate is okay (or doesn't need to be), false if
 * the connection should fail because of a bad certificate.
 */
static bool
check_cert_name(ssl_sio_t *s)
{
    X509 *cert;
    char *unmatched_names;

    cert = SSL_get_peer_certificate(s->con);
    if (cert == NULL) {
	if (s->config->verify_host_cert) {
	    sioc_set_error("No host certificate");
	    return false;
	} else {
	    s->secure_unverified = true;
	    vtrace("No host certificate.\n");
	    return true;
	}
    }

    unmatched_names = spc_verify_cert_hostname(cert,
	    (s->accept_dnsname != NULL)? s->accept_dnsname: s->hostname);
    if (unmatched_names != NULL) {
	X509_free(cert);
	if (s->config->verify_host_cert) {
	    sioc_set_error("Host certificate name(s) do not match '%s':\n%s",
		    s->hostname, unmatched_names);
	    return false;
	} else {
	    char *reason;

	    s->secure_unverified = true;
	    vtrace("Host certificate name(s) do not match hostname.\n");
	    reason = Asprintf("Host certificate name(s) do not match '%s': "
		    "%s", s->hostname, unmatched_names);
	    Free(reason);
	    return true;
	}
	Free(unmatched_names);
    }
    X509_free(cert);
    return true;
}
#endif /*]*/

/* Password callback. */
static int
passwd_cb(char *buf, int size, int rwflag _is_unused, void *userdata)
{
    ssl_sio_t *s = (ssl_sio_t *)userdata;
    int pass_len;
    char *p;
    bool need_free = false;

    if (s->password != NULL) {
	/* Interactive password overrides everything else. */
	p = s->password;
    } else if (s->config->key_passwd == NULL) {
	/* No configured password. We need to ask the GUI. Fail for now. */
	s->need_password = true;
	return 0;
    } else {
	/* Parse the configured password. */
	p = sioc_parse_password_spec(s->config->key_passwd);
	if (p == NULL) {
	    return 0;
	}
	need_free = true;
    }

    pass_len = (int)strlen(p);
    if (pass_len > size - 1) {
	pass_len = size - 1;
    }
    strncpy(buf, p, size - 1);
    buf[pass_len] = '\0';
    if (need_free) {
	Free(p);
    }
    return pass_len;
}

static int
parse_file_type(const char *s)
{
    if (s == NULL || !strcasecmp(s, "pem")) {
	return SSL_FILETYPE_PEM;
    } else if (!strcasecmp(s, "asn1")) {
	return SSL_FILETYPE_ASN1;
    } else {
	return -1;
    }
}

static char *
get_ssl_error(char *buf)
{
    unsigned long e = ERR_get_error();

    if (getenv("SSL_VERBOSE_ERRORS")) {
	ERR_error_string(e, buf);
    } else {
	char xbuf[120];
	char *colon;

	ERR_error_string(e, xbuf);
	colon = strrchr(xbuf, ':');
	if (colon != NULL) {
	    strcpy(buf, colon + 1);
	} else {
	    strcpy(buf, xbuf);
	}
    }
    return buf;
}

/* One-time initialization. */
static void
base_init(void)
{
    static bool initted = false;

    if (initted) {
	return;
    }
    initted = true;

#if defined(OPENSSL110) /*[*/
    OPENSSL_init_ssl(0, NULL);
#else /*][*/
    SSL_load_error_strings();
    SSL_library_init();
#endif /*]*/
}

/* Returns true if secure I/O is supported. */
bool
sio_supported(void)
{
    return true;
}

#if !defined(OPENSSL102) /*[*/
/* Hostname match function. */
static int
hostname_matches(const char *hostname, const char *cn, size_t len)
{
    /*
     * If the name from the certificate contains an embedded NUL, then by
     * definition it will not match the hostname.
     */
    if (strlen(cn) < len) {
	return 0;
    }

    /*
     * Try a direct comparison.
     */
    if (!strcasecmp(hostname, cn)) {
	return 1;
    }

    /*
     * Try a wild-card comparison.
     */
    if (!strncmp(cn, "*.", 2) &&
	    strlen(hostname) > strlen(cn + 1) &&
	    !strcasecmp(hostname + strlen(hostname) - strlen(cn + 1),
		cn + 1)) {
	return 1;
    }

    return 0;
}

/*
 * Certificate hostname expansion function.
 * Mostly, this expands NULs.
 */
static char *
expand_hostname(const char *cn, size_t len)
{
    static char buf[1024];
    int ix = 0;

    if (len > sizeof(buf) / 2 + 1) {
	len = sizeof(buf) / 2 + 1;
    }

    while (len--) {
	char c = *cn++;

	if (c) {
	    buf[ix++] = c;
	} else {
		buf[ix++] = '\\';
		buf[ix++] = '0';
	}
    }
    buf[ix] = '\0';

    return buf;
}

/*
 * Add a unique element to a NULL-terminated list of strings.
 * Return the old list, or free it and return a new one.
 */
static char **
add_to_namelist(char **list, char *item)
{
    char **new;
    int count;

    if (list == NULL) {
	/* First element. */
	new = (char **)Malloc(2 * sizeof(char *));
	new[0] = NewString(item);
	new[1] = NULL;
	return new;
    }

    /* Count the number of elements, and bail if we find a match. */
    for (count = 0; list[count] != NULL; count++) {
	if (!strcasecmp(list[count], item)) {
	    return list;
	}
    }

    new = (char **)Malloc((count + 2) * sizeof(char *));
    memcpy(new, list, count * sizeof(char *));
    Free(list);
    new[count] = NewString(item);
    new[count + 1] = NULL;
    return new;
}

/*
 * Free a namelist.
 */
static void
free_namelist(char **list)
{
    int i;

    for (i = 0; list[i] != NULL; i++) {
	Free(list[i]);
    }
    Free(list);
}

/*
 * Expand a namelist into text.
 */
static char *
expand_namelist(char **list)
{
    int i;
    char *r = NULL;

    if (list != NULL) {
	for (i = 0; list[i] != NULL; i++) {
	    char *new;

	    new = Asprintf("%s%s%s", r? r: "", r? " ": "", list[i]);
	    Replace(r, new);
	}
    }
    return r? r: NewString("(none)");
}
#endif /*]*/

#if !defined(OPENSSL102) /*[*/
/* Hostname validation function. */
static char *
spc_verify_cert_hostname(X509 *cert, const char *hostname)
{
    int ok = 0;
    X509_NAME *subj;
    char name[256];
    GENERAL_NAMES *values;
    GENERAL_NAME *value;
    int num_an, i;
    unsigned char *dns;
    int len;
    char **namelist = NULL;
    char *nnl;

    /* Check the common name. */
    if (!ok &&
	(subj = X509_get_subject_name(cert)) &&
	(len = X509_NAME_get_text_by_NID(subj, NID_commonName, name,
	    sizeof(name))) > 0) {

	name[sizeof(name) - 1] = '\0';
	if (!strcmp(hostname, "*") || hostname_matches(hostname, name, len)) {
	    ok = 1;
	    vtrace("SSL_connect: commonName %s matches hostname %s\n", name,
		    hostname);
	} else {
	    vtrace("SSL_connect: non-matching commonName: %s\n",
		    expand_hostname(name, len));
	    nnl = Asprintf("DNS:%s", expand_hostname(name, len));
	    namelist = add_to_namelist(namelist, nnl);
	    Free(nnl);
	}
    }

    /* Check the alternate names. */
    if (!ok &&
	(values = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0))) {
	num_an = sk_GENERAL_NAME_num(values);
	for (i = 0; i < num_an && !ok; i++) {
	    value = sk_GENERAL_NAME_value(values, i);
	    if (value->type == GEN_DNS) {
		len = ASN1_STRING_to_UTF8(&dns, value->d.dNSName);
		if (!strcmp(hostname, "*") ||
		    hostname_matches(hostname, (char *)dns, len)) {

		    ok = 1;
		    vtrace("SSL_connect: alternameName DNS:%s matches "
			    "hostname %s\n", expand_hostname((char *)dns, len),
			    hostname);
		    OPENSSL_free(dns);
		    break;
		} else {
		    vtrace("SSL_connect: non-matching alternateName: DNS:%s\n",
			    expand_hostname((char *)dns, len));
		    nnl = Asprintf("DNS:%s", expand_hostname((char *)dns,
				len));
		    namelist = add_to_namelist(namelist, nnl);
		    Free(nnl);
		}
		OPENSSL_free(dns);
	    }
	    if (ok) {
		break;
	    }
	}
    }

    if (ok) {
	if (namelist) {
	    free_namelist(namelist);
	}
	return NULL;
    } else if (namelist == NULL) {
	return NewString("(none)");
    } else {
	nnl = expand_namelist(namelist);
	free_namelist(namelist);
	return nnl;
    }
}
#endif /*]*/

/*
 * Create a new OpenSSL connection.
 */
sio_init_ret_t
sio_init(tls_config_t *config, const char *password, sio_t *sio_ret)
{
    ssl_sio_t *s = NULL;
    char err_buf[120];
    int cert_file_type = SSL_FILETYPE_PEM;
    sio_init_ret_t err_ret = SI_FAILURE;
    int min_protocol = -1;
    int max_protocol = -1;
    char *proto_error;

    sioc_error_reset();

    /* Base initialization. */
    base_init();

    s = (ssl_sio_t *)Malloc(sizeof(ssl_sio_t));
    memset(s, 0, sizeof(*s));
    s->sock = INVALID_SOCKET;

#if defined(OPENSSL110) /*[*/
    s->ctx = SSL_CTX_new(TLS_method());
#else /*][*/
    s->ctx = SSL_CTX_new(SSLv23_method());
#endif /*]*/
    if (s->ctx == NULL) {
	sioc_set_error("SSL_CTX_new failed");
	goto fail;
    }
    SSL_CTX_set_options(s->ctx, SSL_OP_ALL);
    proto_error = sioc_parse_protocol_min_max(config->min_protocol, config->max_protocol, SIP_SSL3, -1, &min_protocol,
	    &max_protocol);
    if (proto_error != NULL) {
	sioc_set_error("%s", proto_error);
	Free(proto_error);
	goto fail;
    }
    if (min_protocol >= 0 && SSL_CTX_set_min_proto_version(s->ctx, proto_map[min_protocol]) == 0) {
	sioc_set_error("SSL_CTX_set_min_proto_version failed");
	goto fail;
    }
    if (max_protocol >= 0 && SSL_CTX_set_max_proto_version(s->ctx, proto_map[max_protocol]) == 0) {
	sioc_set_error("SSL_CTX_set_max_proto_version failed");
	goto fail;
    }
    SSL_CTX_set_info_callback(s->ctx, client_info_callback);
    SSL_CTX_set_default_passwd_cb_userdata(s->ctx, s);
    SSL_CTX_set_default_passwd_cb(s->ctx, passwd_cb);
    if (config->security_level != NULL && config->security_level[0] != '\0') {
	char *end;
	unsigned long i = strtoul(config->security_level, &end, 10);

	if (*end != '\0' || i > INT_MAX) {
	    sioc_set_error("Invalid %s: '%s'", ResTlsSecurityLevel, config->security_level);
	    goto fail;
	}

	SSL_CTX_set_security_level(s->ctx, (int)i);
    }

    s->config = config;

    vtrace("TLS: will%s verify host certificate\n",
	    s->config->verify_host_cert? "": " not");

    if (password != NULL) {
	s->password = NewString(password);
    }

    /* Parse the -accepthostname option. */
    if (s->config->accept_hostname != NULL) {
	if (!strcasecmp(s->config->accept_hostname, "any") ||
	    !strcmp(s->config->accept_hostname, "*")) {
	    s->accept_dnsname = "*";
	} else if (!strncasecmp(s->config->accept_hostname, "DNS:", 4) &&
		    s->config->accept_hostname[4] != '\0') {
	    s->accept_dnsname = &s->config->accept_hostname[4];
	} else if (!strncasecmp(s->config->accept_hostname, "IP:", 3) &&
		    s->config->accept_hostname[3] != '\0') {
	    sioc_set_error("Cannot use 'IP:' for acceptHostname");
	    goto fail;
	} else {
	    s->accept_dnsname = s->config->accept_hostname;
	}
    }

    /* Pull in the CA certificate file. */
    if (s->config->ca_file != NULL || s->config->ca_dir != NULL) {
	if (SSL_CTX_load_verify_locations(s->ctx, s->config->ca_file,
		    s->config->ca_dir) != 1) {
	    sioc_set_error("CA database load (%s%s%s%s%s%s%s%s%s) failed:\n%s",
		    s->config->ca_file? "file ": "",
		    s->config->ca_file? "\"": "",
		    s->config->ca_file? s->config->ca_file: "",
		    s->config->ca_file? "\"": "",
		    (s->config->ca_file && s->config->ca_dir)? ", ": "",
		    s->config->ca_dir? "dir ": "",
		    s->config->ca_dir? "\"": "",
		    s->config->ca_dir? s->config->ca_dir: "",
		    s->config->ca_dir? "\"": "",
		    get_ssl_error(err_buf));
	    goto fail;
	}
    } else {
	SSL_CTX_set_default_verify_paths(s->ctx);
    }

    /* Pull in the client certificate file. */
    if (s->config->chain_file != NULL) {
	if (SSL_CTX_use_certificate_chain_file(s->ctx,
		    s->config->chain_file) != 1) {
	    sioc_set_error("Client certificate chain file load (\"%s\") "
		    "failed:\n%s", s->config->chain_file,
		    get_ssl_error(err_buf));
	    goto fail;
	}
    } else if (s->config->cert_file != NULL) {
	cert_file_type = parse_file_type(s->config->cert_file_type);
	if (cert_file_type == -1) {
	    sioc_set_error("Invalid client certificate file type '%s'",
		    s->config->cert_file_type);
	    goto fail;
	}
	if (SSL_CTX_use_certificate_file(s->ctx, s->config->cert_file,
		    cert_file_type) != 1) {
	    sioc_set_error("Client certificate file load (\"%s\") failed:\n%s",
		    s->config->cert_file, get_ssl_error(err_buf));
	    goto fail;
	}
    }

    /* Pull in the private key file. */
    if (s->config->key_file != NULL) {
	int key_file_type = parse_file_type(s->config->key_file_type);

	if (key_file_type == -1) {
	    sioc_set_error("Invalid private key file type '%s'",
		    s->config->key_file_type);
	    goto fail;
	}
	if (SSL_CTX_use_PrivateKey_file(s->ctx, s->config->key_file,
		    key_file_type) != 1) {
	    sioc_set_error("Private key file load (\"%s\") failed:\n%s",
		    s->config->key_file, get_ssl_error(err_buf));
	    err_ret = s->need_password? SI_NEED_PASSWORD: SI_WRONG_PASSWORD;
	    goto fail;
	}
    } else if (s->config->chain_file != NULL) {
	if (SSL_CTX_use_PrivateKey_file(s->ctx, s->config->chain_file,
		    SSL_FILETYPE_PEM) != 1) {
	    sioc_set_error("Private key file load (\"%s\") failed:\n%s",
		    s->config->chain_file, get_ssl_error(err_buf));
	    err_ret = s->need_password? SI_NEED_PASSWORD: SI_WRONG_PASSWORD;
	    goto fail;
	}
    } else if (s->config->cert_file != NULL) {
	if (SSL_CTX_use_PrivateKey_file(s->ctx, s->config->cert_file,
		    cert_file_type) != 1) {
	    sioc_set_error("Private key file load (\"%s\") failed:\n%s",
		    s->config->cert_file, get_ssl_error(err_buf));
	    err_ret = s->need_password? SI_NEED_PASSWORD: SI_WRONG_PASSWORD;
	    goto fail;
	}
    }

    /* Check the key. */
    if (s->config->key_file != NULL && SSL_CTX_check_private_key(s->ctx) != 1) {
	sioc_set_error("Private key check failed:\n%s", get_ssl_error(err_buf));
	goto fail;
    }

    s->con = SSL_new(s->ctx);
    if (s->con == NULL) {
	sioc_set_error("SSL_new failed");
	goto fail;
    }
    SSL_set_verify_depth(s->con, 64);

    /* Success. */
    *sio_ret = (sio_t *)s;
    return SI_SUCCESS;

fail:
    /* Failure. */
    if (s != NULL) {
	if (s->ctx != NULL) {
	    SSL_CTX_free(s->ctx);
	    s->ctx = NULL;
	}
	if (s->con != NULL) {
	    SSL_free(s->con);
	    s->con = NULL;
	}
	if (s->password != NULL) {
	    Free(s->password);
	    s->password = NULL;
	}
	Free(s);
    }
    return err_ret;
}

/* Callback for tracing protocol negotiation. */
static void
client_info_callback(INFO_CONST SSL *s, int where, int ret)
{
    if (where == SSL_CB_CONNECT_LOOP) {
	vtrace("SSL_connect trace: %s %s\n", SSL_state_string(s),
		SSL_state_string_long(s));
    } else if (where == SSL_CB_CONNECT_EXIT) {
	if (ret == 0) {
	    vtrace("SSL_connect trace: failed in %s\n",
		    SSL_state_string_long(s));
	} else if (ret < 0) {
	    unsigned long e;
	    char err_buf[1024];
	    char *st;
	    char *colon;

	    err_buf[0] = '\n';
	    e = ERR_get_error();
	    if (e != 0) {
		ERR_error_string(e, err_buf + 1);
	    } else if (errno != 0) {
		strcpy(err_buf + 1, strerror(errno));
	    } else {
		err_buf[0] = '\0';
	    }
	    st = Asprintf("SSL_connect trace: error in %s%s",
		    SSL_state_string_long(s), err_buf);
	    if ((colon = strrchr(st, ':')) != NULL) {
		*colon = '\n';
	    }

	    sioc_set_error("%s", st);
	    Free(st);
	}
    }
}

/* Display a certificate. */
void
display_cert(varbuf_t *v, X509 *cert, int level, const char *who)
{
    EVP_PKEY *pkey;
    BIO *mem;
    long nw;
    char *p;

    /* Public key. */
    if ((pkey = X509_get_pubkey(cert)) == NULL) {
	vb_appendf(v, "%*sError getting cert public key\n", level, "");
    }
    if (pkey != NULL) {
	vb_appendf(v, "%*s%sPublic key: %d bit ", level, "", who,
		EVP_PKEY_bits(pkey));
	switch (EVP_PKEY_base_id(pkey)) {
	case EVP_PKEY_RSA:
	    vb_appendf(v, "RSA");
	    break;
	case EVP_PKEY_DSA:
	    vb_appendf(v, "DSA");
	    break;
	default:
	    vb_appendf(v, "non-RSA/DSA");
	    break;
	}
	vb_appendf(v, "\n");
    }

    /* Subject and issuer. */
    mem = BIO_new(BIO_s_mem());
    X509_NAME_print_ex(mem, X509_get_subject_name(cert), 0,
	    XN_FLAG_ONELINE | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_FN_SN);
    nw = BIO_get_mem_data(mem, &p);
    vb_appendf(v, "%*s%sSubject: %.*s\n", level, "", who, (int)nw, p);
    BIO_free(mem);

    mem = BIO_new(BIO_s_mem());
    X509_NAME_print_ex(mem, X509_get_issuer_name(cert), 0,
	    XN_FLAG_ONELINE | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_FN_SN);
    nw = BIO_get_mem_data(mem, &p);
    vb_appendf(v, "%*s%sIssuer: %.*s\n", level, "", who, (int)nw, p);
    BIO_free(mem);

    /* Alternate names. */
    if (level == 0) {
	GENERAL_NAMES *values;
	GENERAL_NAME *value;
	int num_an, j, len;
	unsigned char *dns;

	if ((values = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0))) {
	    bool header = false;

	    num_an = sk_GENERAL_NAME_num(values);
	    for (j = 0; j < num_an; j++) {
		value = sk_GENERAL_NAME_value(values, j);
		if (value->type == GEN_DNS) {
		    len = ASN1_STRING_to_UTF8(&dns, value->d.dNSName);
		    if (!header) {
			vb_appendf(v, "%*sAlternate DNS names:", level, "");
			header = true;
		    }
		    vb_appendf(v, " %.*s", len, (char *)dns);
		    OPENSSL_free(dns);
		}
	    }
	    if (header) {
		vb_appendf(v, "\n");
	    }
	    sk_GENERAL_NAME_pop_free(values, GENERAL_NAME_free);
	}
    }
}

/* Display session info. */
static void
display_session(varbuf_t *v, SSL *con)
{
    vb_appendf(v, "Version: %s\n", SSL_get_version(con));
    vb_appendf(v, "Cipher: %s\n", SSL_get_cipher_name(con));
    vb_appendf(v, "Security level: %d\n", SSL_get_security_level(con));
}

/* Display server certificate info. */
static void
display_server_cert(varbuf_t *v, SSL *con)
{
    X509 *cert;
    STACK_OF(X509) *chain;
    int i;

    chain = SSL_get_peer_cert_chain(con);
    if (chain == NULL) {
	cert = SSL_get_peer_certificate(con);
	if (cert == NULL) {
	    vb_appendf(v, "Error getting server cert\n");
	    return;
	}
	chain = sk_X509_new_null();
	sk_X509_push(chain, cert);
    }

    for (i = 0; i < sk_X509_num(chain); i++) {
	char *who = i? Asprintf("CA %d ", i): "";

	cert = sk_X509_value(chain, i);
	display_cert(v, cert, 0, who);
	if (i > 0) {
	    Free(who);
	}
    }
}

/* Display server subject names. */
static void
display_server_subjects(varbuf_t *v, SSL *con)
{
    X509 *cert;
    STACK_OF(X509) *chain;
    BIO *mem;
    long nw;
    char *p;
    char *pcopy;
    char *cn;
    GENERAL_NAMES *values;
    GENERAL_NAME *value;
    int num_an, j, len;
    unsigned char *dns;
    char **subjects = NULL;

    chain = SSL_get_peer_cert_chain(con);
    if (chain == NULL) {
	cert = SSL_get_peer_certificate(con);
	if (cert == NULL) {
	    vb_appendf(v, "Error getting server cert\n");
	    return;
	}
	chain = sk_X509_new_null();
	sk_X509_push(chain, cert);
    }

    /*
     * Get the subject name from the server cert. This is a bit of a hack
     * because it understands the format of the output of the print_ex
     * function.
     */
    cert = sk_X509_value(chain, 0);
    mem = BIO_new(BIO_s_mem());
    X509_NAME_print_ex(mem, X509_get_subject_name(cert), 0,
	    XN_FLAG_ONELINE | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_FN_SN);
    nw = BIO_get_mem_data(mem, &p);
    pcopy = Malloc(nw + 1);
    strncpy(pcopy, p, nw);
    pcopy[nw] = '\0';
    cn = strstr(pcopy, CN_EQ);
    if (cn != NULL) {
	sioc_subject_add(&subjects, cn + CN_EQ_SIZE, (ssize_t)-1);
    }
    Free(pcopy);
    BIO_free(mem);

    /* Add the alternate names. */
    if ((values = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0))) {
	num_an = sk_GENERAL_NAME_num(values);
	for (j = 0; j < num_an; j++) {
	    value = sk_GENERAL_NAME_value(values, j);
	    if (value->type == GEN_DNS) {
		len = ASN1_STRING_to_UTF8(&dns, value->d.dNSName);
		sioc_subject_add(&subjects, (char *)dns, len);
		OPENSSL_free(dns);
	    }
	}
	sk_GENERAL_NAME_pop_free(values, GENERAL_NAME_free);
    }
    sioc_subject_print(v, &subjects);
}

/*
 * Negotiate an SSL connection.
 * Returns true for success, false for failure.
 * If it returns false, the socket should be disconnected.
 */
sio_negotiate_ret_t
sio_negotiate(sio_t sio, socket_t sock, const char *hostname, bool *data)
{
    ssl_sio_t *s;
    int rv;
    varbuf_t v;
    size_t len;
    long vr;

    sioc_error_reset();

    *data = false;
    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIG_FAILURE;
    }
    s = (ssl_sio_t *)sio;
    if (s->con == NULL ||
	    (s->negotiate_pending && s->sock == INVALID_SOCKET) ||
	    (!s->negotiate_pending && s->sock != INVALID_SOCKET) ||
	    s->negotiated) {
	sioc_set_error("Invalid sio");
	return SIG_FAILURE;
    }

    vtrace("%s OpenSSL negotiation, host '%s'",
	    s->negotiate_pending? "Continuing": "Starting",
	    hostname);
    if (s->accept_dnsname != NULL) {
	vtrace(", accept name '%s'", s->accept_dnsname);
    }
    vtrace(".\n");

    if (!s->negotiate_pending) {
	s->sock = sock;
	s->hostname = hostname;

#if defined(OPENSSL102) /*[*/
	/* Have OpenSSL verify the hostname. */
	if (s->config->verify_host_cert &&
		(s->accept_dnsname == NULL || strcmp(s->accept_dnsname, "*"))) {
	    X509_VERIFY_PARAM *param = SSL_get0_param(s->con);

	    if (!X509_VERIFY_PARAM_set1_host(param,
		(s->accept_dnsname != NULL)? s->accept_dnsname: s->hostname,
		0)) {
		char err_buf[1024];

		sioc_set_error("Set host failed:\n%s", get_ssl_error(err_buf));
		return SIG_FAILURE;
	    }
	}
#endif /*]*/

	SSL_set_verify(s->con, SSL_VERIFY_PEER, ssl_verify_callback);

	/* Set up the TLS/SSL connection. */
	if (SSL_set_fd(s->con, (int)s->sock) != 1) {
	    vtrace("OpenSSL sio_negotiate: can't set fd\n");
	    return SIG_FAILURE;
	}
    }

    current_sio = s;
    rv = SSL_connect(s->con);
    current_sio = NULL;

    if (rv == -1 && SSL_get_error(s->con, rv) == SSL_ERROR_WANT_READ) {
	s->negotiate_pending = true;
	return SIG_WANTMORE;
    }

    if (s->config->verify_host_cert) {
	vr = SSL_get_verify_result(s->con);
	if (vr != X509_V_OK) {
	    sioc_set_error("Host certificate verification failed:\n%s (%ld)%s",
		    X509_verify_cert_error_string(vr), vr,
		    (vr == X509_V_ERR_HOSTNAME_MISMATCH)?
			"\nTry Y: to connect and " AnShow "(" KwTlsSubjectNames ") to list names":
			"");
	    return SIG_FAILURE;
	}
    } else {
	s->secure_unverified = true;
    }

    if (rv != 1) {
	unsigned long e = SSL_get_error(s->con, rv);
	char err_buf[120];

	if (e == SSL_ERROR_SYSCALL) {
	    if (errno == 0) {
		sioc_set_error("SSL_connect failed:\nUnexpected EOF");
	    } else {
		sioc_set_error("SSL_connect failed:\n%s", strerror(errno));
	    }
	} else if (e == SSL_ERROR_ZERO_RETURN) {
	    sioc_set_error("SSL_connect failed:\nUnexpected EOF");
	} else {
	    sioc_set_error("SSL_connect failed %d/%ld:\n%s", rv, e,
		    get_ssl_error(err_buf));
	}
	return SIG_FAILURE;
    }

#if !defined(OPENSSL102) /*[*/
    /* Check the host certificate. */
    if (!check_cert_name(s)) {
	vtrace("disconnect: check_cert_name failed\n");
	return SIG_FAILURE;
    }
#endif /*]*/

    /* Display the session info. */
    vb_init(&v);
    display_session(&v, s->con);
    s->session_info = vb_consume(&v);
    len = strlen(s->session_info);
    if (len > 0 && s->session_info[len - 1] == '\n') {
	 s->session_info[len - 1] = '\0';
    }

    /* Display the server cert. */
    vb_init(&v);
    display_server_cert(&v, s->con);
    s->server_cert_info = vb_consume(&v);
    len = strlen(s->server_cert_info);
    if (len > 0 && s->server_cert_info[len - 1] == '\n') {
	 s->server_cert_info[len - 1] = '\0';
    }

    /* Display the server subject name. */
    vb_init(&v);
    display_server_subjects(&v, s->con);
    s->server_subjects = vb_consume(&v);
    len = strlen(s->server_subjects);
    if (len > 0 && s->server_subjects[len - 1] == '\n') {
	 s->server_subjects[len - 1] = '\0';
    }

    s->negotiated = true;
    return SIG_SUCCESS;
}

/*
 * Read encrypted data from a socket.
 * Returns the data length, SIO_EOF for EOF, SIO_FATAL_ERROR for a fatal error,
 * SIO_NONFATAL_ERROR for a non-fatal error.
 */
int
sio_read(sio_t sio, char *buf, size_t buflen)
{
    ssl_sio_t *s;
    int nr;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (ssl_sio_t *)sio;
    if (s->con == NULL || s->sock == INVALID_SOCKET || !s->negotiated) {
	sioc_set_error("Invalid sio");
	return SIO_FATAL_ERROR;
    }

    nr = SSL_read(s->con, buf, buflen);
    if (nr < 0) {
	unsigned long e;
	char err_buf[120];

	if (errno == EWOULDBLOCK) {
	    vtrace("SSL_read: EWOULDBLOCK\n");
	    return SIO_EWOULDBLOCK;
	}
	e = ERR_get_error();
	if (e != 0) {
	    ERR_error_string(e, err_buf);
	} else {
	    strcpy(err_buf, "unknown error");
	}
	vtrace("RCVD SSL_read error %ld (%s)\n", e, err_buf);
	sioc_set_error("SSL_read:\n%s", err_buf);
	return SIO_FATAL_ERROR;
    }

    return nr;
}

/*
 * Write encrypted data on the socket.
 * Returns the data length or SIO_FATAL_ERROR.
 */
int
sio_write(sio_t sio, const char *buf, size_t buflen)
{
    ssl_sio_t *s;
    int nw;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (ssl_sio_t *)sio;
    if (s->con == NULL || s->sock == INVALID_SOCKET || !s->negotiated) {
	sioc_set_error("Invalid sio");
	return SIO_FATAL_ERROR;
    }

    nw = SSL_write(s->con, buf, (int)buflen);
    if (nw < 0) {
	unsigned long e;
	char err_buf[120];

	e = ERR_get_error();
	ERR_error_string(e, err_buf);
	vtrace("RCVD SSL_write error %ld (%s)\n", e, err_buf);
	sioc_set_error("SSL_write:\n%s", err_buf);
	return SIO_FATAL_ERROR;
    }

    return nw;
}

/* Closes the SSL connection. */
void
sio_close(sio_t sio)
{
    ssl_sio_t *s;

    if (sio == NULL) {
	return;
    }
    s = (ssl_sio_t *)sio;

    if (s->ctx != NULL) {
	SSL_CTX_free(s->ctx);
	s->ctx = NULL;
    }
    if (s->password != NULL) {
	Free(s->password);
	s->password = NULL;
    }
    if (s->session_info != NULL) {
	Free(s->session_info);
	s->session_info = NULL;
    }
    if (s->server_cert_info != NULL) {
	Free(s->server_cert_info);
	s->server_cert_info = NULL;
    }
    if (s->server_subjects != NULL) {
	Free(s->server_subjects);
	s->server_subjects = NULL;
    }

    if (s->con != NULL) {
	SSL_shutdown(s->con);
	SSL_free(s->con);
	s->con = NULL;
    }

    s->sock = INVALID_SOCKET;

    Free(s);
}

/*
 * Returns true if the current connection is unverified.
 */
bool
sio_secure_unverified(sio_t sio)
{
    ssl_sio_t *s = (ssl_sio_t *)sio;
    return (s != NULL)? s->secure_unverified: false;
}

/*
 * Returns a bitmap of the supported options.
 */
unsigned
sio_options_supported(void)
{
    return TLS_OPT_CA_DIR | TLS_OPT_CA_FILE | TLS_OPT_CERT_FILE
	| TLS_OPT_CERT_FILE_TYPE | TLS_OPT_CHAIN_FILE | TLS_OPT_KEY_FILE
	| TLS_OPT_KEY_FILE_TYPE | TLS_OPT_KEY_PASSWD | TLS_OPT_MIN_PROTOCOL
	| TLS_OPT_MAX_PROTOCOL | TLS_OPT_SECURITY_LEVEL;
}

/*
 * Returns session info.
 */
const char *
sio_session_info(sio_t sio)
{
    ssl_sio_t *s = (ssl_sio_t *)sio;
    return (s != NULL)? s->session_info: NULL;
}

/*
 * Returns server certificate info.
 */
const char *
sio_server_cert_info(sio_t sio)
{
    ssl_sio_t *s = (ssl_sio_t *)sio;
    return (s != NULL)? s->server_cert_info: NULL;
}

/*
 * Returns server subject names.
 */
const char *
sio_server_subject_names(sio_t sio)
{
    ssl_sio_t *s = (ssl_sio_t *)sio;
    return (s != NULL)? s->server_subjects: NULL;
}

/*
 * Returns the name of the provider.
 */
const char *
sio_provider(void)
{
    return SSLeay_version(SSLEAY_VERSION);
}
