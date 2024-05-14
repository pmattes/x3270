/*
 * Copyright (c) 2017-2024 Paul Mattes.
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
 *	sio_secure_transport.c
 *		Secure I/O via the MacOS Secure Transport facility.
 */

#include "globals.h"

#include <Security/Security.h>
#include <Security/SecureTransport.h>

#include <string.h>

#include "tls_config.h"

#include "names.h"
#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "sioc.h"
#include "tls_passwd_gui.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"

#define ARRAY_SIZE(n)	(int)(sizeof(n) / sizeof(n[0]))

/* Globals */

/* Statics */
typedef struct {
    socket_t sock;			/* socket */
    const char *hostname;		/* server name */
    bool negotiate_pending;		/* true if negotiation pending */
    bool secure_unverified;		/* true if server cert not verified */
    SSLContextRef context;		/* secure transport context */
    char *session_info;			/* session information */
    char *server_cert_info;		/* server cert information */
    char *server_subjects;		/* server cert subjects */
} stransport_sio_t;

static tls_config_t *config;
static char *interactive_password;

static SSLProtocol proto_map[] = { kSSLProtocol2, kSSLProtocol3, kTLSProtocol1, kTLSProtocol11, kTLSProtocol12 };

#define CIPHER(s)	{ s, #s }
typedef struct {
    int value;
    const char *name;
} cipher_name_t;
cipher_name_t cipher_names[] = {
    CIPHER(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_DHE_DSS_WITH_DES_CBC_SHA),
    CIPHER(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_DHE_RSA_WITH_DES_CBC_SHA),
    CIPHER(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_DH_DSS_WITH_DES_CBC_SHA),
    CIPHER(SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_DH_RSA_WITH_DES_CBC_SHA),
    CIPHER(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5),
    CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_DH_anon_WITH_DES_CBC_SHA),
    CIPHER(SSL_DH_anon_WITH_RC4_128_MD5),
    CIPHER(SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA),
    CIPHER(SSL_FORTEZZA_DMS_WITH_NULL_SHA),
    CIPHER(SSL_NO_SUCH_CIPHERSUITE),
    CIPHER(SSL_NULL_WITH_NULL_NULL),
    CIPHER(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA),
    CIPHER(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5),
    CIPHER(SSL_RSA_EXPORT_WITH_RC4_40_MD5),
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_MD5),
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(SSL_RSA_WITH_DES_CBC_MD5),
    CIPHER(SSL_RSA_WITH_DES_CBC_SHA),
    CIPHER(SSL_RSA_WITH_IDEA_CBC_MD5),
    CIPHER(SSL_RSA_WITH_IDEA_CBC_SHA),
    CIPHER(SSL_RSA_WITH_NULL_MD5),
    CIPHER(SSL_RSA_WITH_NULL_SHA),
    CIPHER(SSL_RSA_WITH_RC2_CBC_MD5),
    CIPHER(SSL_RSA_WITH_RC4_128_MD5),
    CIPHER(SSL_RSA_WITH_RC4_128_SHA),
    CIPHER(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DHE_DSS_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_DHE_DSS_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DHE_PSK_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DHE_PSK_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DHE_PSK_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_DHE_PSK_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DHE_PSK_WITH_NULL_SHA),
    CIPHER(TLS_DHE_PSK_WITH_NULL_SHA256),
    CIPHER(TLS_DHE_PSK_WITH_NULL_SHA384),
    CIPHER(TLS_DHE_PSK_WITH_RC4_128_SHA),
    CIPHER(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DH_DSS_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DH_DSS_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DH_DSS_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_DH_DSS_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DH_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DH_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DH_RSA_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_DH_RSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DH_anon_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_DH_anon_WITH_RC4_128_MD5),
    CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_RC4_128_SHA),
    CIPHER(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_ECDH_ECDSA_WITH_NULL_SHA),
    CIPHER(TLS_ECDH_ECDSA_WITH_RC4_128_SHA),
    CIPHER(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_ECDH_RSA_WITH_NULL_SHA),
    CIPHER(TLS_ECDH_RSA_WITH_RC4_128_SHA),
    CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDH_anon_WITH_NULL_SHA),
    CIPHER(TLS_ECDH_anon_WITH_RC4_128_SHA),
    CIPHER(TLS_EMPTY_RENEGOTIATION_INFO_SCSV),
    CIPHER(TLS_NULL_WITH_NULL_NULL),
    CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_PSK_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_PSK_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_PSK_WITH_NULL_SHA),
    CIPHER(TLS_PSK_WITH_NULL_SHA256),
    CIPHER(TLS_PSK_WITH_NULL_SHA384),
    CIPHER(TLS_PSK_WITH_RC4_128_SHA),
    CIPHER(TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_RSA_PSK_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_RSA_PSK_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_RSA_PSK_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_RSA_PSK_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_RSA_PSK_WITH_NULL_SHA),
    CIPHER(TLS_RSA_PSK_WITH_NULL_SHA256),
    CIPHER(TLS_RSA_PSK_WITH_NULL_SHA384),
    CIPHER(TLS_RSA_PSK_WITH_RC4_128_SHA),
    CIPHER(TLS_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256),
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384),
    CIPHER(TLS_RSA_WITH_NULL_MD5),
    CIPHER(TLS_RSA_WITH_NULL_SHA),
    CIPHER(TLS_RSA_WITH_NULL_SHA256),
    CIPHER(TLS_RSA_WITH_RC4_128_MD5),
    CIPHER(TLS_RSA_WITH_RC4_128_SHA),
    { 0, NULL }
};

/* Record an error from a Secure Transport call. */
static void
set_oserror(OSStatus status, const char *fmt, ...)
{
    va_list args;
    char *t;
    CFStringRef errmsg;
    char *explanation = "";

    va_start(args, fmt);
    t = Vasprintf(fmt, args);
    va_end(args);

    if (status == errSSLXCertChainInvalid) {
	explanation = "\nTry Y: to connect and " AnShow "(" KwTlsSubjectNames ") to list names";
    }

    errmsg = SecCopyErrorMessageString(status, NULL);
    if (errmsg != NULL) {
	sioc_set_error("%s: %s%s", t,
		CFStringGetCStringPtr(errmsg, kCFStringEncodingASCII),
		explanation);
	CFRelease(errmsg);
    } else {
	sioc_set_error("%s: Error %d%s", t, (int)status, explanation);
    }
    Free(t);
}

/* Read function called by Secure Transport. */
OSStatus
read_func(SSLConnectionRef connection, void *data, size_t *data_length)
{
    stransport_sio_t *s = (stransport_sio_t *)connection;
    int nr;
    size_t n_read = 0;

    if (s->sock == INVALID_SOCKET) {
	*data_length = 0;
	return errSecIO;
    }

    /*
     * They want us to return all of the data, or errSSLWouldBlock.
     */
    while (n_read < *data_length) {
	nr = recv(s->sock, (char *)data + n_read, *data_length - n_read, 0);
	vtrace("TLS: read %d/%d bytes\n", nr, (int)(*data_length - n_read));
	if (nr < 0) {
	    if (errno == EWOULDBLOCK) {
		*data_length = n_read;
		return errSSLWouldBlock;
	    }
	    vtrace("TLS recv: %s\n", strerror(errno));
	    *data_length = n_read;
	    return errSecIO;
	} else if (nr == 0) {
	    *data_length = n_read;
	    return errSSLClosedGraceful;
	}
	n_read += nr;
    }

    *data_length = n_read;
    return errSecSuccess;
}

/* Write function called by Secure Transport. */
OSStatus
write_func(SSLConnectionRef connection, const void *data, size_t *data_length)
{
    stransport_sio_t *s = (stransport_sio_t *)connection;
    int nw;

    if (s->sock == INVALID_SOCKET) {
	*data_length = 0;
	return errSecIO;
    }

    nw = send(s->sock, data, *data_length, 0);
    vtrace("TLS: wrote %d/%d bytes\n", nw, (int)*data_length);
    if (nw < 0) {
	vtrace("TLS send: %s\n", strerror(errno));
	*data_length = 0;
	return errSecIO;
    } else {
	*data_length = nw;
	return errSecSuccess;
    }
}

/* Get the subject or issuer name details from a cert. */
static char *
name_details(CFArrayRef array)
{
    const void *keys[] = {
	kSecOIDCommonName,
	kSecOIDEmailAddress,
	kSecOIDOrganizationalUnitName,
	kSecOIDOrganizationName,
	kSecOIDLocalityName,
	kSecOIDStateProvinceName,
	kSecOIDCountryName
    };
    static const char *labels[] = { "CN", "E", "OU", "O", "L", "S", "C", "E" };
    varbuf_t v;
    char *comma = "";

    vb_init(&v);

    for (int i = 0; i < ARRAY_SIZE(keys);  i++) {
	CFIndex n;

	for (n = 0 ; n < CFArrayGetCount(array); n++) {
	    CFDictionaryRef dict;
	    CFTypeRef dictkey;
	    CFStringRef str;
	    char buf[1024];

	    dict = CFArrayGetValueAtIndex(array, n);
	    if (CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
		continue;
	    }
	    dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
	    if (!CFEqual(dictkey, keys[i])) {
		continue;
	    }
	    str = (CFStringRef)CFDictionaryGetValue(dict, kSecPropertyKeyValue);
	    if (CFStringGetCString(str, buf, sizeof(buf),
			kCFStringEncodingUTF8)) {
		vb_appendf(&v, "%s%s=%s", comma, labels[i], buf);
		comma = ", ";
	    }
	}
    }
    return vb_consume(&v);
}

/* Get the alternate names from a cert. */
static char *
alt_names(CFArrayRef array)
{
    const void *keys[] = {
	CFSTR("DNS Name")	/* XXX: There must be a constant for this */
    };
    varbuf_t v;
    char *comma = "";
    int i;

    vb_init(&v);

    for (i = 0; i < ARRAY_SIZE(keys);  i++) {
	CFIndex n;

	for (n = 0 ; n < CFArrayGetCount(array); n++) {
	    CFDictionaryRef dict;
	    CFTypeRef dictkey;
	    CFStringRef str;
	    char buf[1024];

	    dict = CFArrayGetValueAtIndex(array, n);
	    if (CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
		continue;
	    }
	    dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
	    if (!CFEqual(dictkey, keys[i])) {
		continue;
	    }
	    str = (CFStringRef)CFDictionaryGetValue(dict, kSecPropertyKeyValue);
	    if (CFStringGetCString(str, buf, sizeof(buf),
			kCFStringEncodingUTF8)) {
		vb_appendf(&v, "%s%s", comma, buf);
		comma = ", ";
	    }
	}
    }
    return vb_consume(&v);
}

/* Get details from a cert. */
static char *
cert_details(const char *prefix, SecCertificateRef certificateRef)
{
    CFErrorRef error;
    const void *keys[] = {
	kSecOIDX509V1SubjectName,
	kSecOIDX509V1IssuerName,
	kSecOIDSubjectAltName
    };
    static const char *labels[] = {
	"Subject", "Issuer", "Subject alternate names"
    };
    static char *(*decoders[])(CFArrayRef) = {
	name_details,
	name_details,
	alt_names
    };
    CFArrayRef keySelection = CFArrayCreate(NULL, keys, ARRAY_SIZE(keys),
	    &kCFTypeArrayCallBacks);
    CFDictionaryRef vals = SecCertificateCopyValues(certificateRef,
	    keySelection, &error);
    varbuf_t v;
    int i;

    vb_init(&v);

    /* So I can see the OIDs and figure out which one is the alt name. */
    for (i = 0; i < ARRAY_SIZE(keys); i++) {
	CFDictionaryRef dict;
	CFArrayRef values;
	char *s;

	dict = CFDictionaryGetValue(vals, keys[i]);
	if (dict == NULL) {
	    continue;
	}
	values = CFDictionaryGetValue(dict, kSecPropertyKeyValue);
	if (values == NULL) {
	    continue;
	}
	s = decoders[i](values);
	vb_appendf(&v, "%s%s: %s\n", prefix, labels[i], s);
	Free(s);
    }

    CFRelease(vals);

    return vb_consume(&v);
}

/* Display certificate information. */
static void
display_cert(varbuf_t *v, const char *prefix, SecCertificateRef cert)
{
#if defined(LONG_DESC_IS_USEFUL) /*[*/
    CFStringRef desc = SecCertificateCopyLongDescription(NULL, cert, NULL);

    if (desc != NULL) {
	char text[1024];

	memset(text, 0, sizeof(text));
	if (CFStringGetCString(desc, text, sizeof(text),
		    kCFStringEncodingUTF8)) {
	    vb_appendf(v, "%s cert: %s\n", prefix, text);
	}
	CFRelease(desc);
    }
#endif /*]*/

    char *s = cert_details(prefix, cert);
    vb_appends(v, s);
    Free(s);
}

/* Convert a cipher to its name. */
const char *
cipher_name(int n)
{
    int i;
    struct {
	const char *orig;
	const char *subst;
    } substs[] = {
	{ "_", " " },
	{ "WITH", "with" },
	{ "NULL", "null" },
	{ "FORTEZZA", "Fortezza" },
	{ NULL, NULL }
    };

    for (i = 0; cipher_names[i].name != NULL; i++) {
	if (cipher_names[i].value == n) {
	    char *s = txAsprintf("%s", cipher_names[i].name);
	    int j;

	    for (j = 0; substs[j].orig != NULL; j++) {
		char *t;

		while ((t = strstr(s, substs[j].orig)) != NULL) {
		    strncpy(t, substs[j].subst, strlen(substs[j].subst));
		}
	    }
	    return s;
	}
    }

    return txAsprintf("0x%x\n", n);
}

/* Display connection info. */
static void
display_connection_info(varbuf_t *v, stransport_sio_t *s)
{
    OSStatus status;
    SSLProtocol protocol;
    SSLCipherSuite cipher_suite;

    status = SSLGetNegotiatedProtocolVersion(s->context, &protocol);
    if (status == errSecSuccess) {
	vb_appendf(v, "Protocol version: ");
	switch (protocol) {
	case kSSLProtocol2:
	    vb_appendf(v, "SSL 2");
	    break;
	case kSSLProtocol3:
	    vb_appendf(v, "SSL 3");
	    break;
	case kTLSProtocol1:
	    vb_appendf(v, "TLS 1.0");
	    break;
	case kTLSProtocol11:
	    vb_appendf(v, "TLS 1.1");
	    break;
	case kTLSProtocol12:
	    vb_appendf(v, "TLS 1.2");
	    break;
	default:
	    vb_appendf(v, "0x%x", (unsigned)protocol);
	    break;
	}
	vb_appendf(v, "\n");
    }

    status = SSLGetNegotiatedCipher(s->context, &cipher_suite);
    if (status == errSecSuccess) {
	vb_appendf(v, "Cipher: %s\n", cipher_name(cipher_suite));
    }
}

/* Display server cert info. */
static void
display_server_cert(varbuf_t *v, stransport_sio_t *s)
{
    OSStatus status;
    SecTrustRef trust = NULL;

    status = SSLCopyPeerTrust(s->context, &trust);
    if (status == errSecSuccess && trust != NULL) {
	CFIndex count = SecTrustGetCertificateCount(trust);
	CFIndex i;

	for (i = 0L ; i < count ; i++) {
	    char *prefix = "";

	    if (i) {
		prefix = txAsprintf("CA %ld ", i);
	    }
	    display_cert(v, prefix, SecTrustGetCertificateAtIndex(trust, i));
	}
	CFRelease(trust);
    }
}

/* Display server subjects. */
static void
display_subjects(varbuf_t *v, stransport_sio_t *s)
{
    OSStatus status;
    SecTrustRef trust = NULL;
    char **subjects = NULL;

    status = SSLCopyPeerTrust(s->context, &trust);
    if (status == errSecSuccess && trust != NULL) {
	CFIndex count = SecTrustGetCertificateCount(trust);

	if (count > 0) {
	    SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, 0);
	    CFErrorRef error;
	    const void *keys[] = {
		kSecOIDX509V1SubjectName,
		kSecOIDSubjectAltName
	    };
	    CFArrayRef keySelection = CFArrayCreate(NULL, keys, ARRAY_SIZE(keys),
		&kCFTypeArrayCallBacks);
	    CFDictionaryRef certDict = SecCertificateCopyValues(cert,
		    keySelection, &error);

	    /* Get the subject name. */
	    CFDictionaryRef dict = CFDictionaryGetValue(certDict,
		    kSecOIDX509V1SubjectName);

	    if (dict != NULL) {
		CFArrayRef values = CFDictionaryGetValue(dict,
			kSecPropertyKeyValue);

		if (values != NULL) {
		    CFIndex n;

		    for (n = 0; n < CFArrayGetCount(values); n++) {
			CFTypeRef dictkey;
			CFStringRef str;
			char buf[1024];
			CFDictionaryRef dict2 = CFArrayGetValueAtIndex(values, n);

			if (CFGetTypeID(dict2) != CFDictionaryGetTypeID()) {
			    continue;
			}
			dictkey = CFDictionaryGetValue(dict2,
				kSecPropertyKeyLabel);
			if (!CFEqual(dictkey, kSecOIDCommonName)) {
			    continue;
			}
			str = (CFStringRef)CFDictionaryGetValue(dict2,
				kSecPropertyKeyValue);
			if (CFStringGetCString(str, buf, sizeof(buf),
				    kCFStringEncodingUTF8)) {
			    sioc_subject_add(&subjects, buf, (ssize_t)-1);
			}
		    }
		}
	    }

	    /* Get the alternate names. */
	    dict = CFDictionaryGetValue(certDict, kSecOIDSubjectAltName);
	    if (dict != NULL) {
		CFArrayRef values = CFDictionaryGetValue(dict,
			kSecPropertyKeyValue);
		if (values != NULL) {
		    CFIndex n;

		    for (n = 0; n < CFArrayGetCount(values); n++) {
			CFTypeRef dictkey;
			CFStringRef str;
			char buf[1024];
			CFDictionaryRef dict2 = CFArrayGetValueAtIndex(values,
				n);

			if (CFGetTypeID(dict2) != CFDictionaryGetTypeID()) {
			    continue;
			}
			dictkey = CFDictionaryGetValue(dict2,
				kSecPropertyKeyLabel);
			if (!CFEqual(dictkey, CFSTR("DNS Name"))) {
			    continue;
			}
			str = (CFStringRef)CFDictionaryGetValue(dict2,
				kSecPropertyKeyValue);
			if (CFStringGetCString(str, buf, sizeof(buf),
				    kCFStringEncodingUTF8)) {
			    sioc_subject_add(&subjects, buf, (ssize_t)-1);
			}
		    }
		}
	    }
	    CFRelease(certDict);
	}
	CFRelease(trust);
    }
    sioc_subject_print(v, &subjects);
}

/* Create a CFDataRef from the contents of a file. */
static CFDataRef
dataref_from_file(const char *path)
{
    char *accum = NULL;
    size_t n_accum = 0;
    CFDataRef dataref;

    accum = sioc_string_from_file(path, &n_accum);
    if (accum == NULL) {
	return NULL;
    }

    dataref = CFDataCreate(NULL, (UInt8 *)accum, n_accum);
    Free(accum);
    return dataref;
}

/* Copy the identity from a file. */
static OSStatus
identity_from_file(const char *path, const char *password,
	SecIdentityRef *identity_ret)
{
    CFDataRef pkcs_data = dataref_from_file(path);

    if (pkcs_data == NULL) {
	return errSecItemNotFound;
    } else {
	CFStringRef pass_string = (password != NULL)?
	    CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8) :
	    NULL;
	const void *keys[] = { kSecImportExportPassphrase };
	const void *values[] = { pass_string };
	CFDictionaryRef options = CFDictionaryCreate(NULL, keys, values,
		(pass_string != NULL)? 1L: 0L, NULL, NULL);
	CFArrayRef items = NULL;
	OSStatus status = SecPKCS12Import(pkcs_data, options, &items);

	if (status == errSecSuccess && items != NULL &&
		CFArrayGetCount(items)) {
	    CFDictionaryRef identity_and_trust = CFArrayGetValueAtIndex(items,
		    0L);
	    const void *identity = CFDictionaryGetValue(identity_and_trust,
		    kSecImportItemIdentity);

	    /* We only need the identity. */
	    CFRetain(identity);
	    *identity_ret = (SecIdentityRef)identity;
	}

	if (items != NULL) {
	    CFRelease(items);
	}
	CFRelease(options);
	CFRelease(pkcs_data);
	if (pass_string != NULL) {
	    CFRelease(pass_string);
	}
	return status;
    }
}

/*
 * Get an identity from a certificate in the keychain, based on the common
 * name.
 */
static OSStatus
identity_from_keychain(char *name, SecIdentityRef *identity_ret)
{
#   define KEY_ENTRIES 4
    OSStatus status;
    CFTypeRef keys[KEY_ENTRIES];
    CFTypeRef values[KEY_ENTRIES];
    CFDictionaryRef query_dict;
    CFArrayRef ids;

    /* Assume we will return nothing. */
    *identity_ret = NULL;

    /*
     * Set up search criteria.
     * The Apple docs imply that you can search for a match against the
     * common name, e.g. kSecMatchSubjectWholeString. It doesn't appear to
     * work; you get back all certificates.  So the result needs to be searched
     * manually for a common name match.
     */
    keys[0] = kSecClass;
    values[0] = kSecClassIdentity; 	/* want identity (cert and key) */
    keys[1] = kSecReturnRef;
    values[1] = kCFBooleanTrue;    	/* want a reference */
    keys[2] = kSecMatchLimit;
    values[2] = kSecMatchLimitAll;	/* all of them */
    keys[3] = kSecMatchPolicy;
    values[3] = SecPolicyCreateSSL(false, NULL); /* just SSL certs */
    query_dict = CFDictionaryCreate(NULL, (const void **)keys,
	    (const void **)values, KEY_ENTRIES,
	    &kCFCopyStringDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
    CFRelease(values[3]); /* the policy */
										    /* Search for a common name match. */
    status = SecItemCopyMatching(query_dict, (CFTypeRef *)&ids);
    CFRelease(query_dict);

    if (status == errSecSuccess) {
	CFIndex count = CFArrayGetCount(ids);
	CFIndex i;
	bool matched = false;

	/* TODO: Could do a case-independent match, or a substring match. */
	vtrace("identity_from_keychain: Got %d match%s\n", (int)count,
		((int)count == 1)? "": "es");
	for (i = 0; i < count && !matched; i++) {
	    SecIdentityRef identity =
		(SecIdentityRef)CFArrayGetValueAtIndex(ids, i);
	    SecCertificateRef cert = NULL;

	    if (SecIdentityCopyCertificate(identity, &cert) == errSecSuccess) {
		CFStringRef cf_common_name;
		char common_name[1024];

		if (SecCertificateCopyCommonName(cert, &cf_common_name) ==
			errSecSuccess) {
		    if (CFStringGetCString(cf_common_name, common_name,
				sizeof(common_name), kCFStringEncodingUTF8)
			    && !strcmp(name, common_name)) {
			CFRetain(identity);
			*identity_ret = identity;
			matched = true;
		    }
		    CFRelease(cf_common_name);
		}
		CFRelease(cert);
	    }
	}
	CFRelease(ids);
	return matched? errSecSuccess: errSecItemNotFound;
    }

    return status;
}

/* Set up the client certificate. */
static sio_init_ret_t
set_client_cert(stransport_sio_t *s)
{
    OSStatus status;
    SecIdentityRef identity = NULL;
    char *cert_name;

    if (config->cert_file != NULL) {
	char *password = NULL;
	bool need_free = false;

	if (interactive_password != NULL) {
	    password = interactive_password;
	} else if (config->key_passwd != NULL) {
	    password = sioc_parse_password_spec(config->key_passwd);
	    if (password == NULL) {
		return SI_FAILURE;
	    }
	    need_free = true;
	}

	cert_name = config->cert_file;
	status = identity_from_file(cert_name, password,
		&identity);
	if (need_free) {
	    Free(password);
	}
    } else if (config->client_cert != NULL) {
	cert_name = config->client_cert;
	status = identity_from_keychain(cert_name, &identity);
    } else {
	/* No client cert. */
	return SI_SUCCESS;
    }

    if (status == errSecSuccess && identity != NULL) {
	SecCertificateRef cert = NULL;
	CFTypeRef certs_array[1];
	CFArrayRef certs;

	/* Found it. */
	status = SecIdentityCopyCertificate(identity, &cert);
	if (status == errSecSuccess) {
	    varbuf_t v;

	    vb_init(&v);
	    display_cert(&v, "Client", cert);
	    vtrace("%s", vb_buf(&v));
	    vb_free(&v);
	    CFRelease(cert);
	}

	/* Set it. */
	certs_array[0] = identity;
	certs = CFArrayCreate(NULL, (const void **)certs_array, 1L,
		&kCFTypeArrayCallBacks);
	status = SSLSetCertificate(s->context, certs);
	if (certs != NULL) {
	    CFRelease(certs);
	}

	if (status != errSecSuccess) {
	    set_oserror(status, "SSLSetCertificate");
	    return SI_FAILURE;
	}
	CFRelease(identity);
	return SI_SUCCESS;
    }

    /* Failure. */
    switch (status) {
    case errSecAuthFailed:
    case errSecPkcs12VerifyFailure:
	sioc_set_error("Incorrect password for certificate \"%s\"", cert_name);
	return SI_WRONG_PASSWORD;
    case errSecDecode:
    case errSecUnknownFormat:
	sioc_set_error("Can't parse certificate certificate \"%s\"", cert_name);
	return SI_FAILURE;
    case errSecPassphraseRequired:
	sioc_set_error("Certificate \"%s\" requires a password", cert_name);
	return SI_NEED_PASSWORD;
    case errSecItemNotFound:
	sioc_set_error("Can't find certificate \"%s\"", cert_name);
	return SI_FAILURE;
    default:
	set_oserror(status, "Can't load certificate \"%s\"", cert_name);
	return SI_FAILURE;
    }
}

/* Free a TLS context. */
static void
sio_free(stransport_sio_t *s)
{
    s->sock = INVALID_SOCKET;
    SSLClose(s->context);
    CFRelease(s->context);
    s->context = NULL;
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
    Free(s);
}

/* Returns true if secure I/O is supported. */
bool
sio_supported(void)
{
    return true;
}

/*
 * Create a new connection.
 */
sio_init_ret_t
sio_init(tls_config_t *c, const char *password, sio_t *sio_ret)
{
    stransport_sio_t *s;
    OSStatus status;
    sio_init_ret_t ret = SI_SUCCESS;
    int min_protocol = -1;
    int max_protocol = -1;
    char *proto_error;

    sioc_error_reset();

    *sio_ret = NULL;

    config = c;
    s = (stransport_sio_t *)Malloc(sizeof(stransport_sio_t));
    memset(s, 0, sizeof(*s));

    s->sock = INVALID_SOCKET;
    s->context = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide,
	    kSSLStreamType);
    if (password != NULL) {
	Replace(interactive_password, NewString(password));
    }
    status = SSLSetIOFuncs(s->context, read_func, write_func);
    if (status != errSecSuccess) {
	set_oserror(status, "SSLSetIOFuncs");
	goto fail;
    }
    proto_error = sioc_parse_protocol_min_max(config->min_protocol, config->max_protocol, -1, SIP_TLS1_2, &min_protocol,
	    &max_protocol);
    if (proto_error != NULL) {
        sioc_set_error("%s", proto_error);
        Free(proto_error);
        goto fail;
    }
    if (min_protocol >= 0 && SSLSetProtocolVersionMin(s->context, proto_map[min_protocol]) != errSecSuccess) {
        sioc_set_error("SSLSetProtocolVersionMin failed");
        goto fail;
    }
    if (max_protocol >= 0 && SSLSetProtocolVersionMax(s->context, proto_map[max_protocol]) != errSecSuccess) {
        sioc_set_error("SSLSetProtocolVersionMax failed");
        goto fail;
    }

    status = SSLSetConnection(s->context, s);
    if (status != errSecSuccess) {
	set_oserror(status, "SSLSetConnection");
	goto fail;
    }

    if (!config->verify_host_cert) {
	status = SSLSetSessionOption(s->context,
		kSSLSessionOptionBreakOnServerAuth, true);
	if (status != errSecSuccess) {
	    set_oserror(status, "SSLSetSessionOption");
	    goto fail;
	}
    }

    /* Set the client certificate, which could require a password. */
    ret = set_client_cert(s);
    if (ret == SI_SUCCESS) {
	*sio_ret = (sio_t)s;
	return ret;
    }

fail:
    sio_free(s);
    *sio_ret = NULL;
    return SI_FAILURE;
}

/*
 * Negotiate a TLS connection.
 * Returns true for success, false for failure.
 * If it returns false, the socket should be disconnected.
 *
 * Returns 'data' true if there is already protocol data pending.
 */
sio_negotiate_ret_t
sio_negotiate(sio_t sio, socket_t sock, const char *hostname, bool *data)
{
    stransport_sio_t *s;
    const char *accept_hostname = hostname;
    OSStatus status;
    varbuf_t v;
    size_t sl;

    sioc_error_reset();

    *data = false;
    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIG_FAILURE;
    }
    s = (stransport_sio_t *)sio;
    if (s->negotiate_pending) {
	if (s->sock == INVALID_SOCKET) {
	    sioc_set_error("Invalid sio");
	    return SIG_FAILURE;
	}
    } else {
	if (s->sock != INVALID_SOCKET) {
	    sioc_set_error("Invalid sio");
	    return SIG_FAILURE;
	}

	s->sock = sock;
	s->hostname = hostname;

	if (config->accept_hostname != NULL) {
	    if (!strncasecmp(accept_hostname, "DNS:", 4)) {
		accept_hostname = config->accept_hostname + 4;
		sioc_set_error("Empty acceptHostname");
		goto fail;
	    } else if (!strncasecmp(config->accept_hostname, "IP:", 3)) {
		sioc_set_error("Cannot use 'IP:' acceptHostname");
		goto fail;
	    } else if (!strcasecmp(config->accept_hostname, "any")) {
		sioc_set_error("Cannot use 'any' acceptHostname");
		goto fail;
	    } else {
		accept_hostname = config->accept_hostname;
	    }
	}

	status = SSLSetPeerDomainName(s->context, accept_hostname,
		strlen(accept_hostname));
	if (status != errSecSuccess) {
	    set_oserror(status, "SSLSetPeerDomainName");
	    goto fail;
	}
    }

    /* Perform handshake. */
    status = SSLHandshake(s->context);
    if (status == errSSLWouldBlock) {
	s->negotiate_pending = true;
	return SIG_WANTMORE;
    }
    if (status != errSecSuccess && status != errSSLServerAuthCompleted) {
	set_oserror(status, "SSLHandshake");
	goto fail;
    }
    if (status == errSSLServerAuthCompleted) {
	/* Do it again, to complete the handshake. */
	status = SSLHandshake(s->context);
	if (status == errSSLWouldBlock) {
	    s->negotiate_pending = true;
	    return SIG_WANTMORE;
	}
	if (status != errSecSuccess) {
	    set_oserror(status, "SSLHandshake");
	    goto fail;
	}
    }

    /* Display connection info. */
    vb_init(&v);
    display_connection_info(&v, s);
    s->session_info = vb_consume(&v);
    sl = strlen(s->session_info);
    if (sl > 0 && s->session_info[sl - 1] == '\n') {
	s->session_info[sl - 1] = '\0';
    }

    /* Display server cert info. */
    vb_init(&v);
    display_server_cert(&v, s);
    s->server_cert_info = vb_consume(&v);
    sl = strlen(s->server_cert_info);
    if (sl > 0 && s->server_cert_info[sl - 1] == '\n') {
	s->server_cert_info[sl - 1] = '\0';
    }

    /* Display subject info. */
    vb_init(&v);
    display_subjects(&v, s);
    s->server_subjects = vb_consume(&v);
    sl = strlen(s->server_subjects);
    if (sl > 0 && s->server_subjects[sl - 1] == '\n') {
	s->server_subjects[sl - 1] = '\0';
    }

    /* Success. */
    s->secure_unverified = !config->verify_host_cert;
    return SIG_SUCCESS;

fail:
    return SIG_FAILURE;
}

/*
 * Read encrypted data from a socket.
 * Returns the data length, SIO_EOF for EOF, SIO_FATAL_ERROR for a fatal error,
 * SIO_EWOULDBLOCK for incomplete input.
 */
int
sio_read(sio_t sio, char *buf, size_t buflen)
{
    stransport_sio_t *s;
    OSStatus status;
    size_t n_read = 0;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (stransport_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	sioc_set_error("Invalid sio");
	return SIO_FATAL_ERROR;
    }

    status = SSLRead(s->context, buf, buflen, &n_read);
    if (status == errSSLClosedGraceful || status == errSSLClosedNoNotify) {
	vtrace("TLS: EOF\n");
	return 0;
    }
    if (status == errSSLWouldBlock) {
	vtrace("TLS: EWOULDBLOCK\n");
	return SIO_EWOULDBLOCK;
    }
    if (status != errSecSuccess) {
	set_oserror(status, "SSLRead %d", status);
	return SIO_FATAL_ERROR;
    }

    return (int)n_read;
}

/*
 * Write encrypted data on the socket.
 * Returns the data length or SIO_FATAL_ERROR.
 */
int
sio_write(sio_t sio, const char *buf, size_t buflen)
{
    stransport_sio_t *s;
    OSStatus status;
    size_t n_written = 0;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (stransport_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	sioc_set_error("Invalid sio");
	return SIO_FATAL_ERROR;
    }

    status = SSLWrite(s->context, buf, buflen, &n_written);
    if (status != errSecSuccess) {
	set_oserror(status, "SSLWrite");
	return SIO_FATAL_ERROR;
    }

    return (int)buflen;
}

/* Closes the TLS connection. */
void
sio_close(sio_t sio)
{
    stransport_sio_t *s;

    if (sio == NULL) {
	return;
    }
    s = (stransport_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	return;
    }

    sio_free(s);
}

/*
 * Returns true if the current connection is unverified.
 */
bool
sio_secure_unverified(sio_t sio)
{
    stransport_sio_t *s = (stransport_sio_t *)sio;
    return s? s->secure_unverified: false;
}

/*
 * Returns a bitmap of the supported options.
 */
unsigned
sio_options_supported(void)
{   
    return TLS_OPT_CERT_FILE | TLS_OPT_CLIENT_CERT | TLS_OPT_KEY_PASSWD | TLS_OPT_MIN_PROTOCOL | TLS_OPT_MAX_PROTOCOL;
}

const char *
sio_session_info(sio_t sio)
{
    stransport_sio_t *s = (stransport_sio_t *)sio;
    return (s != NULL)? s->session_info: NULL;
}

const char *
sio_server_cert_info(sio_t sio)
{
    stransport_sio_t *s = (stransport_sio_t *)sio;
    return (s != NULL)? s->server_cert_info: NULL;
}

const char *
sio_server_subject_names(sio_t sio)
{
    stransport_sio_t *s = (stransport_sio_t *)sio;
    return (s != NULL)? s->server_subjects: NULL;
}


const char *
sio_provider(void)
{
    return "Apple Secure Transport";
}
