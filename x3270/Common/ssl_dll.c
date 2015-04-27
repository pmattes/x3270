/*
 * Copyright (c) 2012-2013 Paul Mattes.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES,
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ssl_dll.c
 *		Windows-specific interface to the (possibly missing) OpenSSL
 *		DLLs.
 */

#include "ssl_dll.h"	/* name translations */
#include "globals.h"

#if defined(HAVE_LIBSSL) /*[*/

#include <openssl/ssl.h>

const char *ssl_fail_reason = NULL;

/* Enumeration for each function type. */
typedef enum {
    T_ASN1_STRING_to_UTF8 = 0,
    T_CRYPTO_free,
    T_ERR_error_string,
    T_ERR_get_error,
    T_SSLv23_method,
    T_SSL_connect,
    T_SSL_get_peer_certificate,
    T_SSL_CTX_check_private_key,
    T_SSL_CTX_ctrl,
    T_SSL_CTX_free,
    T_SSL_CTX_load_verify_locations,
    T_SSL_CTX_new,
    T_SSL_CTX_set_default_passwd_cb,
    T_SSL_CTX_set_default_verify_paths,
    T_SSL_CTX_set_info_callback,
    T_SSL_CTX_use_certificate_chain_file,
    T_SSL_CTX_use_certificate_file,
    T_SSL_CTX_use_PrivateKey_file,
    T_SSL_free,
    T_SSL_get_verify_result,
    T_SSL_library_init,
    T_SSL_load_error_strings,
    T_SSL_new,
    T_SSL_read,
    T_SSL_set_fd,
    T_SSL_set_verify,
    T_SSL_set_verify_depth,
    T_SSL_shutdown,
    T_SSL_state_string,
    T_SSL_state_string_long,
    T_SSL_write,
    T_X509_NAME_get_text_by_NID,
    T_X509_STORE_CTX_get_error,
    T_X509_free,
    T_X509_get_ext_d2i,
    T_X509_get_subject_name,
    T_X509_verify_cert_error_string,
    T_sk_num,
    T_sk_value,
    NUM_DLL_FUNCS
} ssl_dll_t;

/* DLL function names. */
static const char *ssl_dll_name[NUM_DLL_FUNCS] = {
    "ASN1_STRING_to_UTF8",
    "CRYPTO_free",
    "ERR_error_string",
    "ERR_get_error",
    "SSLv23_method",
    "SSL_connect",
    "SSL_get_peer_certificate",
    "SSL_CTX_check_private_key",
    "SSL_CTX_ctrl",
    "SSL_CTX_free",
    "SSL_CTX_load_verify_locations",
    "SSL_CTX_new",
    "SSL_CTX_set_default_passwd_cb",
    "SSL_CTX_set_default_verify_paths",
    "SSL_CTX_set_info_callback",
    "SSL_CTX_use_certificate_chain_file",
    "SSL_CTX_use_certificate_file",
    "SSL_CTX_use_PrivateKey_file",
    "SSL_free",
    "SSL_get_verify_result",
    "SSL_library_init",
    "SSL_load_error_strings",
    "SSL_new",
    "SSL_read",
    "SSL_set_fd",
    "SSL_set_verify",
    "SSL_set_verify_depth",
    "SSL_shutdown",
    "SSL_state_string",
    "SSL_state_string_long",
    "SSL_write",
    "X509_NAME_get_text_by_NID",
    "X509_STORE_CTX_get_error",
    "X509_free",
    "X509_get_ext_d2i",
    "X509_get_subject_name",
    "X509_verify_cert_error_string",
    "sk_num",
    "sk_value"
};

/* Function prototypes. */
typedef int (*ASN1_STRING_to_UTF8_t)(unsigned char **out, ASN1_STRING *in);
typedef void (*CRYPTO_free_t)(void *);
typedef char *(*ERR_error_string_t)(unsigned long e, char *buf);
typedef unsigned long (*ERR_get_error_t)(void);
typedef const SSL_METHOD *(*SSLv23_method_t)(void);
typedef int (*SSL_connect_t)(SSL *ssl);
typedef X509 *(*SSL_get_peer_certificate_t)(const SSL *ssl);
typedef int (*SSL_CTX_check_private_key_t)(const SSL_CTX *ctx);
typedef long (*SSL_CTX_ctrl_t)(SSL_CTX *ctx, int cmd, long larg, void *parg);
typedef void (*SSL_CTX_free_t)(SSL_CTX *ctx);
typedef int (*SSL_CTX_load_verify_locations_t)(SSL_CTX *ctx,
	const char *CAfile, const char *CApath);
typedef SSL_CTX *(*SSL_CTX_new_t)(const SSL_METHOD *meth);
typedef void (*SSL_CTX_set_default_passwd_cb_t)(SSL_CTX *ctx,
	pem_password_cb *cb);
typedef int (*SSL_CTX_set_default_verify_paths_t)(SSL_CTX *ctx);
typedef void (*SSL_CTX_set_info_callback_t)(SSL_CTX *ctx,
	void (*cb)(const SSL *ssl,int type,int val));
typedef int (*SSL_CTX_use_certificate_chain_file_t)(SSL_CTX *ctx,
	const char *file);
typedef int (*SSL_CTX_use_certificate_file_t)(SSL_CTX *ctx, const char *file,
	int type);
typedef int (*SSL_CTX_use_PrivateKey_file_t)(SSL_CTX *ctx, const char *file,
	int type);
typedef void (*SSL_free_t)(SSL *ssl);
typedef long (*SSL_get_verify_result_t)(const SSL *ssl);
typedef int (*SSL_library_init_t)(void);
typedef void (*SSL_load_error_strings_t)(void);
typedef SSL *(*SSL_new_t)(SSL_CTX *ctx);
typedef int  (*SSL_read_t)(SSL *ssl,void *buf,int num);
typedef int (*SSL_set_fd_t)(SSL *s, int fd);
typedef void (*SSL_set_verify_t)(SSL *s, int mode,
	int (*callback)(int ok, X509_STORE_CTX *ctx));
typedef void (*SSL_set_verify_depth_t)(SSL *s, int depth);
typedef int (*SSL_shutdown_t)(SSL *s);
typedef const char *(*SSL_state_string_t)(const SSL *s);
typedef const char *(*SSL_state_string_long_t)(const SSL *s);
typedef int  (*SSL_write_t)(SSL *ssl,const void *buf,int num);
typedef int (*X509_NAME_get_text_by_NID_t)(X509_NAME *name, int nid,
	char *buf, int len);
typedef int (*X509_STORE_CTX_get_error_t)(X509_STORE_CTX *ctx);
typedef void (*X509_free_t)(X509 *a);
typedef void *(*X509_get_ext_d2i_t)(X509 *x, int nid, int *crit, int *idx);
typedef X509_NAME *(*X509_get_subject_name_t)(X509 *a);
typedef const char *(*X509_verify_cert_error_string_t)(long n);
typedef int (*sk_num_t)(const _STACK *);
typedef void *(*sk_value_t)(const _STACK *, int);

/* DLL handles. */
static HMODULE ssleay32_handle = NULL;
static HMODULE libeay32_handle = NULL;

/* Function pointers, resolved from the DLLs. */
static FARPROC ssl_dll_func[NUM_DLL_FUNCS];

static int ssl_dll_initted = 0;
#define REQUIRE_INIT do { \
    if (ssl_dll_initted == 0) \
	/*ssl_dll_init();*/ abort(); \
    if (ssl_dll_initted < 0) { \
	    abort(); \
    } \
} while(0);

/* Resolve a symbol from the SSL library. */
static FARPROC
get_ssl_t(const char *symbol)
{
	FARPROC p;

	p = GetProcAddress(ssleay32_handle, symbol);
	if (p == NULL)
		p = GetProcAddress(libeay32_handle, symbol);
	return p;
}

/*
 * Open the OpenSSL DLLs and resolve all of the symbols we need.
 * If this function returns -1, then none of the other entry points are
 * valid.
 * Returns 0 on success.
 */
int
ssl_dll_init(void)
{
	int i;
	int rv = 0;

	if (ssl_dll_initted)
	    	return (ssl_dll_initted < 0)? -1: 0;

	if (getenv("FAIL_SSL_DLL")) {
	    	ssl_fail_reason = "Testing purposes";
		rv = -1;
		goto done;
	}

	/* Open the DLLs. */
	ssleay32_handle = LoadLibrary("ssleay32.dll");
	if (ssleay32_handle == NULL) {
		ssl_fail_reason = "Cannot load ssleay32.dll";
		rv = -1;
		goto done;
	}
	libeay32_handle = LoadLibrary("libeay32.dll");
	if (libeay32_handle == NULL) {
		ssl_fail_reason = "Cannot load libeay32.dll";
		rv = -1;
		goto done;
	}

	/* Resolve each of the symbols. */
	if (sizeof(ssl_dll_name)/sizeof(ssl_dll_name[0]) !=
	    sizeof(ssl_dll_func)/sizeof(ssl_dll_func[0])) {
		abort();
	}
	for (i = 0; i < NUM_DLL_FUNCS; i++) {
		if (ssl_dll_name[i] == NULL)
			abort();
		ssl_dll_func[i] = get_ssl_t(ssl_dll_name[i]);
		if (ssl_dll_func[i] == NULL) {
		    ssl_fail_reason = "Cannot resolve symbol(s)";
		    rv = -1;
		    break;
		}
	}

	/* Done. */
    done:
	ssl_dll_initted = rv? rv: 1;
	return rv;
}

/* OpenSSL functions used by wc370/ws3270/wpr3287. */

int
ASN1_STRING_to_UTF8(unsigned char **out, ASN1_STRING *in)
{
	REQUIRE_INIT;
	return ((ASN1_STRING_to_UTF8_t)
		ssl_dll_func[T_ASN1_STRING_to_UTF8])(out, in);
}

void
CRYPTO_free(void *p)
{
	REQUIRE_INIT;
	((CRYPTO_free_t)ssl_dll_func[T_CRYPTO_free])(p);
}

char *
ERR_error_string(unsigned long e, char *buf)
{
	REQUIRE_INIT;
	return ((ERR_error_string_t)ssl_dll_func[T_ERR_error_string])(e, buf);
}

unsigned long
ERR_get_error(void)
{
	REQUIRE_INIT;
    	return ((ERR_get_error_t)ssl_dll_func[T_ERR_get_error])();
}

const SSL_METHOD *
SSLv23_method(void)
{
	REQUIRE_INIT;
    	return ((SSLv23_method_t)ssl_dll_func[T_SSLv23_method])();
}

int
SSL_connect(SSL *ssl)
{
	REQUIRE_INIT;
	return ((SSL_connect_t)ssl_dll_func[T_SSL_connect])(ssl);
}

X509 *
SSL_get_peer_certificate(const SSL *ssl)
{
	REQUIRE_INIT;
	return ((SSL_get_peer_certificate_t)
		ssl_dll_func[T_SSL_get_peer_certificate])(ssl);
}

int
SSL_CTX_check_private_key(const SSL_CTX *ctx)
{
	REQUIRE_INIT;
	return ((SSL_CTX_check_private_key_t)
		ssl_dll_func[T_SSL_CTX_check_private_key])(ctx);
}

long SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
{
	REQUIRE_INIT;
	return ((SSL_CTX_ctrl_t)
		ssl_dll_func[T_SSL_CTX_ctrl])(ctx, cmd, larg, parg);
}

void
SSL_CTX_free(SSL_CTX *ctx)
{
	REQUIRE_INIT;
	((SSL_CTX_free_t)ssl_dll_func[T_SSL_CTX_free])(ctx);
}

int
SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
	const char *CApath)
{
	REQUIRE_INIT;
	return ((SSL_CTX_load_verify_locations_t)
		ssl_dll_func[T_SSL_CTX_load_verify_locations])(ctx, CAfile,
		    CApath);
}

SSL_CTX *
SSL_CTX_new(const SSL_METHOD *meth)
{
	REQUIRE_INIT;
	return ((SSL_CTX_new_t)ssl_dll_func[T_SSL_CTX_new])(meth);
}

void
SSL_CTX_set_default_passwd_cb(SSL_CTX *ctx, pem_password_cb *cb)
{
	REQUIRE_INIT;
	((SSL_CTX_set_default_passwd_cb_t)
	 ssl_dll_func[T_SSL_CTX_set_default_passwd_cb])(ctx, cb);
}

int
SSL_CTX_set_default_verify_paths(SSL_CTX *ctx)
{
	REQUIRE_INIT;
	return ((SSL_CTX_set_default_verify_paths_t)
		ssl_dll_func[T_SSL_CTX_set_default_verify_paths])(ctx);
}

void
SSL_CTX_set_info_callback(SSL_CTX *ctx,
	void (*cb)(const SSL *ssl,int type,int val))
{
	REQUIRE_INIT;
	((SSL_CTX_set_info_callback_t)
	 ssl_dll_func[T_SSL_CTX_set_info_callback])(ctx, cb);
}

int
SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file)
{
	REQUIRE_INIT;
	return ((SSL_CTX_use_certificate_chain_file_t)
		ssl_dll_func[T_SSL_CTX_use_certificate_chain_file])(ctx, file);
}

int
SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type)
{
	REQUIRE_INIT;
	return ((SSL_CTX_use_certificate_file_t)
		ssl_dll_func[T_SSL_CTX_use_certificate_file])(ctx, file, type);
}

int
SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type)
{
	REQUIRE_INIT;
	return ((SSL_CTX_use_PrivateKey_file_t)
		ssl_dll_func[T_SSL_CTX_use_PrivateKey_file])(ctx, file, type);
}

void
SSL_free(SSL *ssl)
{
	REQUIRE_INIT;
	((SSL_free_t)ssl_dll_func[T_SSL_free])(ssl);
}

long
SSL_get_verify_result(const SSL *ssl)
{
	REQUIRE_INIT;
	return ((SSL_get_verify_result_t)
		ssl_dll_func[T_SSL_get_verify_result])(ssl);
}

int
SSL_library_init(void)
{
	REQUIRE_INIT;
	return ((SSL_library_init_t)ssl_dll_func[T_SSL_library_init])();
}

void
SSL_load_error_strings(void)
{
	REQUIRE_INIT;
    	((SSL_load_error_strings_t)ssl_dll_func[T_SSL_load_error_strings])();
}

SSL *
SSL_new(SSL_CTX *ctx)
{
	REQUIRE_INIT;
    	return ((SSL_new_t)ssl_dll_func[T_SSL_new])(ctx);
}

int
SSL_read(SSL *ssl, void *buf, int num)
{
	REQUIRE_INIT;
	return ((SSL_read_t)ssl_dll_func[T_SSL_read])(ssl, buf, num);
}

int
SSL_set_fd(SSL *s, int fd)
{
	REQUIRE_INIT;
	return ((SSL_set_fd_t)ssl_dll_func[T_SSL_set_fd])(s, fd);
}

void
SSL_set_verify(SSL *s, int mode, int (*callback)(int ok,X509_STORE_CTX *ctx))
{
	REQUIRE_INIT;
	((SSL_set_verify_t)ssl_dll_func[T_SSL_set_verify])(s, mode, callback);
}

void
SSL_set_verify_depth(SSL *s, int depth)
{
	REQUIRE_INIT;
	((SSL_set_verify_depth_t)
	 ssl_dll_func[T_SSL_set_verify_depth])(s, depth);
}

int
SSL_shutdown(SSL *s)
{
	REQUIRE_INIT;
	return ((SSL_shutdown_t)ssl_dll_func[T_SSL_shutdown])(s);
}

const char *
SSL_state_string(const SSL *s)
{
	REQUIRE_INIT;
	return ((SSL_state_string_t)ssl_dll_func[T_SSL_state_string])(s);
}

const char *
SSL_state_string_long(const SSL *s)
{
	REQUIRE_INIT;
	return ((SSL_state_string_long_t)
		ssl_dll_func[T_SSL_state_string_long])(s);
}

int
SSL_write(SSL *ssl, const void *buf, int num)
{
	REQUIRE_INIT;
	return ((SSL_write_t)ssl_dll_func[T_SSL_write])(ssl, buf, num);
}

int
X509_NAME_get_text_by_NID(X509_NAME *name, int nid, char *buf, int len)
{
	REQUIRE_INIT;
	return ((X509_NAME_get_text_by_NID_t)
		ssl_dll_func[T_X509_NAME_get_text_by_NID])(name, nid, buf,
		    len);
}

int
X509_STORE_CTX_get_error(X509_STORE_CTX *ctx)
{
	REQUIRE_INIT;
	return ((X509_STORE_CTX_get_error_t)
		ssl_dll_func[T_X509_STORE_CTX_get_error])(ctx);
}

void
X509_free(X509 *a)
{
	REQUIRE_INIT;
	((X509_free_t)ssl_dll_func[T_X509_free])(a);
}

void *
X509_get_ext_d2i(X509 *x, int nid, int *crit, int *idx)
{
	REQUIRE_INIT;
	return ((X509_get_ext_d2i_t)
		ssl_dll_func[T_X509_get_ext_d2i])(x, nid, crit, idx);
}

X509_NAME *
X509_get_subject_name(X509 *a)
{
	REQUIRE_INIT;
	return ((X509_get_subject_name_t)
		ssl_dll_func[T_X509_get_subject_name])(a);
}

const char *
X509_verify_cert_error_string(long n)
{
	REQUIRE_INIT;
	return ((X509_verify_cert_error_string_t)
		ssl_dll_func[T_X509_verify_cert_error_string])(n);
}

int
sk_num(const _STACK *s)
{
    	REQUIRE_INIT;
	return ((sk_num_t)ssl_dll_func[T_sk_num])(s);
}

void *
sk_value(const _STACK *s, int i)
{
    	REQUIRE_INIT;
	return ((sk_value_t)ssl_dll_func[T_sk_value])(s, i);
}

#endif /*]*/
