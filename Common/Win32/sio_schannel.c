/*
 * Copyright (c) 2017-2025 Paul Mattes.
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
 *
 * This module borrows freely from "TLS with Schannel" posted on
 * http://www.coastrd.com/tls-with-schannel.
 */

/*
 *	sio_schannel.c
 *		Secure I/O via the Windows schannel facility.
 */

#include "globals.h"

#if defined(_MSC_VER) /*[*/
typedef struct {
    DWORD unused;
} UNICODE_STRING, *PUNICODE_STRING;
#endif /*]*/

#define SECURITY_WIN32
#define SCHANNEL_USE_BLACKLISTS
#include <wincrypt.h>
#include <wintrust.h>
#include <security.h>
#include <schannel.h>
#include <sspi.h>

#include "tls_config.h"

#include "indent_s.h"
#include "names.h"
#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "sioc.h"
#include "tls_passwd_gui.h"
#include "trace.h"
#include "utils.h"
#include "w3misc.h"
#include "winvers.h"

#if !defined(SP_PROT_TLS1_1_CLIENT)
# define SP_PROT_TLS1_1_CLIENT 0x200
#endif

#if !defined(SP_PROT_TLS1_2_CLIENT)
# define SP_PROT_TLS1_2_CLIENT 0x800
#endif

#if !defined(SP_PROT_TLS1_3_CLIENT)
# define SP_PROT_TLS1_3_CLIENT 0x2000
#endif

/* #define VERBOSE		1 */	/* dump protocol packets in hex */

#define MIN_READ	50		/* small amount to read from the
					   socket at a time, so we are in
					   no danger of reading more than one
					   record */
#define INBUF		(16 * 1024)	/* preliminary input buffer size */

#define CN_EQ			"CN="
#define CN_EQ_SIZE		strlen(CN_EQ)
#define DNS_NAME		"DNS Name="
#define DNS_NAME_SIZE		strlen(DNS_NAME)
#define COMMA_SPACE		", "
#define COMMA_SPACE_SIZE	strlen(COMMA_SPACE)

/* Globals */

/* Statics */
typedef struct {
    socket_t sock;			/* socket */
    const char *hostname;		/* server name */
    bool negotiate_pending;		/* true if negotiate pending */
    bool secure_unverified;		/* true if server cert not verified */
    bool negotiated;			/* true if session is negotiated */

    CredHandle client_creds;		/* client credentials */
    bool client_creds_set;		/* true if client_creds is valid */
    bool manual;			/* true if manual validation needed */

    CtxtHandle context;			/* security context */
    bool context_set;			/* true if context is valid */

    SecPkgContext_StreamSizes sizes;	/* stream sizes */

    char *session_info;			/* session information */
    char *server_cert_info;		/* server cert information */
    char *server_subjects;		/* server subject names */

    char *rcvbuf;			/* receive buffer */
    size_t rcvbuf_len;			/* receive buffer length */

    char *prbuf;			/* pending record buffer */
    size_t prbuf_len;			/* pending record buffer size */

    char *sendbuf;			/* send buffer */
} schannel_sio_t;

static tls_config_t *config;
static HCERTSTORE my_cert_store;

static DWORD proto_map[] = {
    0, /* We don't support SSL2 */
    SP_PROT_SSL3_CLIENT,
    SP_PROT_TLS1_CLIENT,
    SP_PROT_TLS1_1_CLIENT,
    SP_PROT_TLS1_2_CLIENT,
    SP_PROT_TLS1_3_CLIENT
};

/* SCH_CREDENTIALS state. */
static enum {
    USC_UNKNOWN = -1,	/* now known yet */
    USC_ON = 1,		/* known to be true */
    USC_OFF = 0,	/* known to be false */
} usc_state = USC_UNKNOWN;

/*
 * Indicates whether to use SCH_CREDENTIALS (returns true) or SCHANNEL_CRED (false).
 *
 * Includes logic to force 'true' for unit testing via the environment and to force 'false'
 * by manually setting usc_state.
 */
static bool
use_sch_credentials(void)
{
    if (usc_state == USC_UNKNOWN) {
	usc_state = ((getenv("FORCE_SCH") != NULL) || IsWindowsVersionOrGreater(10, 0, 0))? USC_ON: USC_OFF;
    }

    return usc_state == USC_ON;
}

/* Display the certificate chain. */
static void
display_cert_chain(varbuf_t *v, PCCERT_CONTEXT cert)
{
    CHAR name[1024];
    PCCERT_CONTEXT current_cert, issuer_cert;
    PCERT_EXTENSION ext;
    WCHAR *wcbuf = NULL;
    DWORD wcsize = 0;
    DWORD mbsize;
    char *mbbuf = NULL;
    int i;

    /* Display leaf name. */
    if (!CertNameToStr(cert->dwCertEncodingType,
		&cert->pCertInfo->Subject,
		CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
		name, sizeof(name))) {
	int err = GetLastError();
	vtrace("CertNameToStr(subject): error 0x%x (%s)\n", err,
		win32_strerror(err));
    } else {
	vb_appendf(v, "Subject: %s\n", name);
    }

    if (!CertNameToStr(cert->dwCertEncodingType,
		&cert->pCertInfo->Issuer,
		CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
		name, sizeof(name))) {
	int err = GetLastError();
	vtrace("CertNameToStr(issuer): error 0x%x (%s)\n", err,
		win32_strerror(err));
    } else {
	vb_appendf(v, "Issuer: %s\n", name);
    }

    /* Display the alternate name. */
    do {
	ext = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
		cert->pCertInfo->cExtension,
		cert->pCertInfo->rgExtension);
	if (ext == NULL) {
	    break;
	}
	if (!CryptFormatObject(X509_ASN_ENCODING, 0, 0, NULL,
		    szOID_SUBJECT_ALT_NAME2, ext->Value.pbData,
		    ext->Value.cbData, NULL, &wcsize)) {
	    break;
	}
	wcsize *= 4;
	wcbuf = (WCHAR *)Malloc(wcsize);
	if (!CryptFormatObject(X509_ASN_ENCODING, 0, 0, NULL,
		    szOID_SUBJECT_ALT_NAME2, ext->Value.pbData,
		    ext->Value.cbData, wcbuf, &wcsize)) {
	    break;
	}
	mbsize = WideCharToMultiByte(CP_ACP, 0, wcbuf, -1, NULL, 0, NULL, NULL);
	mbbuf = Malloc(mbsize);
	if (WideCharToMultiByte(CP_ACP, 0, wcbuf, -1, mbbuf, mbsize, NULL,
		    NULL) != mbsize) {
	    break;
	}
	vb_appendf(v, "Alternate names: %s\n", mbbuf);
    } while (false);
    if (wcbuf != NULL) {
	Free(wcbuf);
    }
    if (mbbuf != NULL) {
	Free(mbbuf);
    }

    /* Display certificate chain. */
    current_cert = cert;
    i = 0;
    while (current_cert != NULL) {
	DWORD verification_flags = 0;

	i++;
	issuer_cert = CertGetIssuerCertificateFromStore(cert->hCertStore,
		current_cert, NULL, &verification_flags);
	if (issuer_cert == NULL) {
            if (current_cert != cert) {
		CertFreeCertificateContext(current_cert);
	    }
	    break;
	}

	if (!CertNameToStr(issuer_cert->dwCertEncodingType,
		    &issuer_cert->pCertInfo->Subject,
		    CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
		    name, sizeof(name))) {
	    int err = GetLastError();
	    vtrace("CertNameToStr(subject): error 0x%x (%s)\n", err,
		    win32_strerror(err));
	} else {
	    vb_appendf(v, "CA %d Subject: %s\n", i, name);
	}

	if (!CertNameToStr(issuer_cert->dwCertEncodingType,
		    &issuer_cert->pCertInfo->Issuer,
		    CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
		    name, sizeof(name))) {
	    int err = GetLastError();
	    vtrace("CertNameToStr(issuer): error 0x%x (%s)\n", err,
		    win32_strerror(err));
	} else {
	    vb_appendf(v, "CA %d Issuer: %s\n", i, name);
	}

	if (current_cert != cert) {
	    CertFreeCertificateContext(current_cert);
	}
	current_cert = issuer_cert;
	issuer_cert = NULL;
    }
}

/* Display the certificate subjects. */
static void
display_cert_subjects(varbuf_t *v, PCCERT_CONTEXT cert)
{
    CHAR name[1024];
    WCHAR *wcbuf = NULL;
    char *mbbuf = NULL;
    char **subjects = NULL;

    /* Display leaf name. */
    if (!CertNameToStr(cert->dwCertEncodingType,
		&cert->pCertInfo->Subject,
		CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
		name, sizeof(name))) {
	int err = GetLastError();
	vtrace("CertNameToStr(subject): error 0x%x (%s)\n", err,
		win32_strerror(err));
    } else {
	char *cn = strstr(name, CN_EQ);
	if (cn != NULL) {
	    sioc_subject_add(&subjects, cn + CN_EQ_SIZE, (ssize_t)-1);
	}
    }

    /* Display the alternate names. */
    do {
	DWORD wcsize = 0;
	DWORD mbsize;
	PCERT_EXTENSION ext = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
		cert->pCertInfo->cExtension,
		cert->pCertInfo->rgExtension);

	if (ext == NULL) {
	    break;
	}
	if (!CryptFormatObject(X509_ASN_ENCODING, 0, 0, NULL,
		    szOID_SUBJECT_ALT_NAME2, ext->Value.pbData,
		    ext->Value.cbData, NULL, &wcsize)) {
	    break;
	}
	wcsize *= 4;
	wcbuf = (WCHAR *)Malloc(wcsize);
	if (!CryptFormatObject(X509_ASN_ENCODING, 0, 0, NULL,
		    szOID_SUBJECT_ALT_NAME2, ext->Value.pbData,
		    ext->Value.cbData, wcbuf, &wcsize)) {
	    break;
	}
	mbsize = WideCharToMultiByte(CP_ACP, 0, wcbuf, -1, NULL, 0, NULL, NULL);
	mbbuf = Malloc(mbsize);
	if (WideCharToMultiByte(CP_ACP, 0, wcbuf, -1, mbbuf, mbsize, NULL,
		    NULL) != mbsize) {
	    break;
	}
    } while (false);

    if (wcbuf != NULL) {
	Free(wcbuf);
    }
    if (mbbuf != NULL) {
	char *n;

	/*
	 * The string looks like:
	 *  XXX Name=nnn, ...
	 * XXX is DNS for DNS names; not sure about others.
	 */
	char *s = mbbuf;

	while ((n = strstr(s, DNS_NAME)) != NULL) {
	    char *comma = strstr(n, COMMA_SPACE);

	    if (comma != NULL) {
		sioc_subject_add(&subjects, n + DNS_NAME_SIZE,
			comma - (n + DNS_NAME_SIZE));
		s = comma + COMMA_SPACE_SIZE;
	    } else {
		sioc_subject_add(&subjects, n + DNS_NAME_SIZE, (ssize_t)-1);
		break;
	    }
	}

	Free(mbbuf);
    }
    sioc_subject_print(v, &subjects);
}

/* Create security credentials. */
static SECURITY_STATUS
create_credentials_single(LPSTR friendly_name, PCredHandle creds, bool *manual)
{
    TimeStamp ts_expiry;
    SECURITY_STATUS status;
    PCCERT_CONTEXT cert_context = NULL;
    SCHANNEL_CRED schannel_cred;
    SCH_CREDENTIALS sch_credentials;
    TLS_PARAMETERS tls_parameters;
    varbuf_t v;
    char *s, *t;
    int min_protocol = -1;
    int max_protocol = -1;
    char *proto_error;

    /* Parse the min/max protocol options. */
    /* Technically you can use SSL2 with schannel, but it is mutually exclusive with TLS, so we don't try. */
    proto_error = sioc_parse_protocol_min_max(config->min_protocol, config->max_protocol, SIP_SSL3, -1, &min_protocol,
            &max_protocol);
    if (proto_error != NULL) {
        sioc_set_error("%s", proto_error);
        Free(proto_error);
        return 1; /* which is not 0 */
    }

    *manual = false;

    /* Open the "MY" certificate store, where IE stores client certificates. */
    if (my_cert_store == NULL) {
	my_cert_store = CertOpenSystemStore(0, "MY");
	if (my_cert_store == NULL) {
	    int err = GetLastError();
	    sioc_set_error("CertOpenSystemStore: error 0x%x (%s)\n", err,
		    win32_strerror(err));
	    return err;
	}
    }

    /*
     * If a friendly name name is specified, then attempt to find a client
     * certificate. Otherwise, just create a NULL credential.
     */
    if (friendly_name != NULL) {
	for (;;) {
	    DWORD nbytes;
	    LPTSTR cert_friendly_name;

	    /* Find a client certificate with the given friendly name. */
	    cert_context = CertFindCertificateInStore(
		    my_cert_store,	/* hCertStore */
		    X509_ASN_ENCODING,	/* dwCertEncodingType */
		    0,			/* dwFindFlags */
		    CERT_FIND_ANY,	/* dwFindType */
		    NULL,		/* *pvFindPara */
		    cert_context);	/* pPrevCertContext */

	    if (cert_context == NULL) {
		int err = GetLastError();
		sioc_set_error("CertFindCertificateInStore: error 0x%x (%s)\n", err,
			win32_strerror(err));
		return err;
	    }

	    nbytes = CertGetNameString(cert_context,
		    CERT_NAME_FRIENDLY_DISPLAY_TYPE,
		    0,
		    NULL,
		    NULL,
		    0);
	    cert_friendly_name = Malloc(nbytes);
	    nbytes = CertGetNameString(cert_context,
		    CERT_NAME_FRIENDLY_DISPLAY_TYPE,
		    0,
		    NULL,
		    cert_friendly_name,
		    nbytes);
	    if (!strcasecmp(friendly_name, cert_friendly_name)) {
		Free(cert_friendly_name);
		break;
	    }

	    Free(cert_friendly_name);
	}

	/* Display it. */
	vtrace("Client certificate:\n");
	vb_init(&v);
	display_cert_chain(&v, cert_context);
	s = vb_consume(&v);
	t = indent_s(s);
	vtrace("%s", t);
	Free(t);
	Free(s);
    }

    /* Build Schannel credential structure. */
    if (use_sch_credentials()) {
	memset(&sch_credentials, 0, sizeof(sch_credentials));
	sch_credentials.dwVersion = SCH_CREDENTIALS_VERSION;
	if (cert_context != NULL) {
	    sch_credentials.cCreds = 1;
	    sch_credentials.paCred = &cert_context;
	}
    } else {
	memset(&schannel_cred, 0, sizeof(schannel_cred));
	schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
	if (cert_context != NULL) {
	    schannel_cred.cCreds = 1;
	    schannel_cred.paCred = &cert_context;
	}
    }

    /* If the user specified a range, or we're before Windows 10, specify the protocols explicitly. */
    if (min_protocol >= 0 || max_protocol >= 0 || !use_sch_credentials()) {
	DWORD protocols = 0;
	int i;

	if (min_protocol < 0) {
	    min_protocol = SIP_SSL3;
	}
	if (max_protocol < 0) {
	    max_protocol = SIP_TLS1_3;
	}

	if (use_sch_credentials()) {
	    /* With sch_credentials, we disable protocols. */
	    for (i = SIP_SSL2; i <= SIP_TLS1_3; i++) {
		if (i < min_protocol || i > max_protocol) {
		    protocols |= proto_map[i];
		}
	    }
	    memset(&tls_parameters, 0, sizeof(tls_parameters));
	    tls_parameters.grbitDisabledProtocols = protocols;
	    sch_credentials.cTlsParameters = 1;
	    sch_credentials.pTlsParameters = &tls_parameters;
	} else {
	    /* With schannel_cres, we enable protocols. */
	    for (i = SIP_SSL2; i <= SIP_TLS1_3; i++) {
		if (i >= min_protocol && i <= max_protocol) {
		    protocols |= proto_map[i];
		}
	    }
	    schannel_cred.grbitEnabledProtocols = protocols;
	}
    }

    if (use_sch_credentials()) {
	sch_credentials.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
    } else {
	schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
    }

    /* 
     * If they don't want the host certificate checked, specify manual
     * validation here and then don't validate.
     */
    if (!config->verify_host_cert || is_wine()) {
	if (use_sch_credentials()) {
	    sch_credentials.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
	} else {
	    schannel_cred.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
	}
	*manual = true;
    } else {
	if (use_sch_credentials()) {
	    sch_credentials.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION;
	} else {
	    schannel_cred.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION;
	}
    }

    /* Create an SSPI credential. */
    status = AcquireCredentialsHandle(
	    NULL,			/* Name of principal */
	    UNISP_NAME,			/* Name of package */
	    SECPKG_CRED_OUTBOUND,	/* Flags indicating use */
	    NULL,			/* Pointer to logon ID */
	    use_sch_credentials()? (PVOID)&sch_credentials: (PVOID)&schannel_cred, /* Package specific data */
	    NULL,			/* Pointer to GetKey() func */
	    NULL,			/* Value to pass to GetKey() */
	    creds,			/* (out) Cred Handle */
	    &ts_expiry);		/* (out) Lifetime (optional) */

    if (status != SEC_E_OK) {
	sioc_set_error("AcquireCredentialsHandle: error 0x%lx (%s)\n", status,
		win32_strerror(status));
    }

    /* Free the certificate context. Schannel has already made its own copy. */
    if (cert_context != NULL) {
	CertFreeCertificateContext(cert_context);
    }

    return status;
}

/* Create security credentials. */
static SECURITY_STATUS
create_credentials(LPSTR friendly_name, PCredHandle creds, bool *manual)
{
    /* Try with sch_credentials enabled. */
    SECURITY_STATUS status = create_credentials_single(friendly_name, creds, manual);

    if (status == SEC_E_UNKNOWN_CREDENTIALS && usc_state == USC_ON) {
	/*
	 * This can happen on Server 2016 and early versions of Windows 10, which do not
	 * support TLS 1.3 and the SCH_CREDENTIALS struct.
	 *
	 * Try again with SCH_CREDENTIALS explicitly disabled.
	 */
	vtrace("sio_schannel: Got SEC_E_UNKNOWN_CREDENTIALS, retrying credential creation using SSCHANNEL_CRED.\n");
	usc_state = USC_OFF;
	status = create_credentials_single(friendly_name, creds, manual);
    }

    return status;
}

/* Get new client credentials. */
static void
get_new_client_credentials(CredHandle *creds, CtxtHandle *context)
{
    CredHandle                        new_creds;
    SecPkgContext_IssuerListInfoEx    issuer_list_info;
    PCCERT_CHAIN_CONTEXT              chain_context;
    CERT_CHAIN_FIND_BY_ISSUER_PARA    find_by_issuer_params;
    PCCERT_CONTEXT                    cert_context;
    TimeStamp                         expiry;
    SECURITY_STATUS                   status;
    SCHANNEL_CRED                     schannel_cred;
    SCH_CREDENTIALS		      sch_credentials;

    /* Read the list of trusted issuers from schannel. */
    status = QueryContextAttributes(context, SECPKG_ATTR_ISSUER_LIST_EX,
	    (PVOID)&issuer_list_info);
    if (status != SEC_E_OK) {
	vtrace("QueryContextAttributes: error 0x%x (%s)\n",
		(unsigned)status, win32_strerror(status));
	return;
    }

    /* Enumerate the client certificates. */
    memset(&find_by_issuer_params, 0, sizeof(find_by_issuer_params));

    find_by_issuer_params.cbSize = sizeof(find_by_issuer_params);
    find_by_issuer_params.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
    find_by_issuer_params.dwKeySpec = 0;
    find_by_issuer_params.cIssuer   = issuer_list_info.cIssuers;
    find_by_issuer_params.rgIssuer  = issuer_list_info.aIssuers;

    chain_context = NULL;

    while (true) {
	/* Find a certificate chain. */
        chain_context = CertFindChainInStore(
		my_cert_store,
		X509_ASN_ENCODING,
		0,
		CERT_CHAIN_FIND_BY_ISSUER,
		&find_by_issuer_params,
		chain_context);
	if (chain_context == NULL) {
	    vtrace("CertFindChainInStore: error 0x%x (%s)\n",
		    (unsigned)GetLastError(),
		    win32_strerror(GetLastError()));
	    break;
	}

	/* Get pointer to leaf certificate context. */
	cert_context = chain_context->rgpChain[0]->rgpElement[0]->pCertContext;

	/* Create Schannel credential. */
	if (use_sch_credentials()) {
	    memset(&sch_credentials, 0, sizeof(sch_credentials));
	    sch_credentials.dwVersion = SCH_CREDENTIALS_VERSION;
	    sch_credentials.cCreds = 1;
	    sch_credentials.paCred = &cert_context;
	} else {
	    memset(&schannel_cred, 0, sizeof(schannel_cred));
	    schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
	    schannel_cred.cCreds = 1;
	    schannel_cred.paCred = &cert_context;
	}

	status = AcquireCredentialsHandle(
		NULL,                   /* Name of principal */
		UNISP_NAME_A,           /* Name of package */
		SECPKG_CRED_OUTBOUND,   /* Flags indicating use */
		NULL,                   /* Pointer to logon ID */
		use_sch_credentials()? (PVOID)&sch_credentials: (PVOID)&schannel_cred, /* Package specific data */
		NULL,                   /* Pointer to GetKey() func */
		NULL,                   /* Value to pass to GetKey() */
		&new_creds,             /* (out) Cred Handle */
		&expiry);               /* (out) Lifetime (optional) */
	if (status != SEC_E_OK) {
	    vtrace("AcquireCredentialsHandle: error 0x%x (%s)\n",
		    (unsigned)status, win32_strerror(status));
	    continue;
	}

	/* Destroy the old credentials. */
	FreeCredentialsHandle(creds);
	*creds = new_creds;
    }
}

#if defined(VERBOSE) /*[*/
/* Display a hex dump of a buffer. */
static void
print_hex_dump(const char *prefix, int length, unsigned char *buffer)
{
    int i, count, index;
    static char rgbDigits[] = "0123456789abcdef";
    char rgbLine[100];
    int cbLine;

    for (index = 0; length; length -= count, buffer += count, index += count) {
	count = (length > 16)? 16: length;
	sprintf(rgbLine, "%4.4x  ", index);
	cbLine = 6;

	for (i = 0; i < count; i++) {
	    rgbLine[cbLine++] = rgbDigits[buffer[i] >> 4];
	    rgbLine[cbLine++] = rgbDigits[buffer[i] & 0x0f];
	    if (i == 7) {
		rgbLine[cbLine++] = ':';
	    } else {
		rgbLine[cbLine++] = ' ';
	    }
	}
	for (; i < 16; i++) {
	    rgbLine[cbLine++] = ' ';
	    rgbLine[cbLine++] = ' ';
	    rgbLine[cbLine++] = ' ';
	}
	rgbLine[cbLine++] = ' ';

	for (i = 0; i < count; i++) {
	    if (buffer[i] < 32 || buffer[i] > 126 || buffer[i] == '%') {
		rgbLine[cbLine++] = '.';
	    } else {
		rgbLine[cbLine++] = buffer[i];
	    }
	}
	rgbLine[cbLine++] = 0;
	vtrace("%s %s\n", prefix, rgbLine);
    }
}
#endif /*]*/

/* Add some helpful info to a TLS failue. */
static const char *
explain_error(SECURITY_STATUS ret)
{
    switch (ret)
    {
	case CERT_E_CN_NO_MATCH:
	case SEC_E_WRONG_PRINCIPAL:
	    return "\nTry Y: to connect and " AnShow "(" KwTlsSubjectNames ") to display names";
	case SEC_E_UNSUPPORTED_FUNCTION:
	    return "\nHost may not support the requested TLS version";
	default:
	    return "";
    }
}

/* Client handshake, second phase. */
static SECURITY_STATUS
client_handshake_loop(
    schannel_sio_t *s,			/* in, out */
    bool            do_initial_read)	/* in */
{
    SecBufferDesc   out_buffer, in_buffer;
    SecBuffer       in_buffers[2], out_buffers[1];
    DWORD           ssp_i_flags, ssp_o_flags;
    int             nrw;
    TimeStamp       expiry;
    SECURITY_STATUS ret;
    bool            do_read;
    int             n2read = MIN_READ;

    ssp_i_flags =
	ISC_REQ_SEQUENCE_DETECT   |
	ISC_REQ_REPLAY_DETECT     |
	ISC_REQ_CONFIDENTIALITY   |
	ISC_RET_EXTENDED_ERROR    |
	ISC_REQ_ALLOCATE_MEMORY   |
	ISC_REQ_STREAM;

    do_read = do_initial_read;

    /* Loop until the handshake is finished or an error occurs. */
    ret = SEC_I_CONTINUE_NEEDED;

    while (ret == SEC_I_CONTINUE_NEEDED        ||
	   ret == SEC_E_INCOMPLETE_MESSAGE     ||
	   ret == SEC_I_INCOMPLETE_CREDENTIALS) {
	if (s->rcvbuf_len == 0 || ret == SEC_E_INCOMPLETE_MESSAGE) {
	    /* Read data from server. */
            if (do_read) {

		/* Read it. */
		nrw = recv(s->sock, s->rcvbuf + s->rcvbuf_len, n2read, 0);
		vtrace("TLS: %d/%d bytes of handshake data received\n", nrw,
			n2read);
		if (nrw == SOCKET_ERROR) {
		    ret = WSAGetLastError();
		    if (ret != WSAEWOULDBLOCK) {
			sioc_set_error("recv: error %d (%s)\n", (int)ret,
				win32_strerror(ret));
		    }
		    break;
		} else if (nrw == 0) {
		    sioc_set_error("server disconnected during TLS "
			    "negotiation");
		    ret = WSAECONNABORTED; /* XXX: synthetic error */
		    break;
	    }
#if defined(VERBOSE) /*[*/
		print_hex_dump("<enc", nrw,
			(unsigned char *)s->rcvbuf + s->rcvbuf_len);
#endif /*]*/
		s->rcvbuf_len += nrw;
	    } else {
	      do_read = true;
	    }
	}

	/*
	 * Set up the input buffers. Buffer 0 is used to pass in data
	 * received from the server. Schannel will consume some or all
	 * of this. Leftover data (if any) will be placed in buffer 1 and
	 * given a buffer type of SECBUFFER_EXTRA.
	 */
	in_buffers[0].pvBuffer   = s->rcvbuf;
	in_buffers[0].cbBuffer   = (DWORD)s->rcvbuf_len;
	in_buffers[0].BufferType = SECBUFFER_TOKEN;

	in_buffers[1].pvBuffer   = NULL;
	in_buffers[1].cbBuffer   = 0;
	in_buffers[1].BufferType = SECBUFFER_EMPTY;

	in_buffer.cBuffers       = 2;
	in_buffer.pBuffers       = in_buffers;
	in_buffer.ulVersion      = SECBUFFER_VERSION;

	/*
	 * Set up the output buffers. These are initialized to NULL
	 * so as to make it less likely we'll attempt to free random
	 * garbage later.
	 */
	out_buffers[0].pvBuffer  = NULL;
	out_buffers[0].BufferType= SECBUFFER_TOKEN;
	out_buffers[0].cbBuffer  = 0;

	out_buffer.cBuffers      = 1;
	out_buffer.pBuffers      = out_buffers;
	out_buffer.ulVersion     = SECBUFFER_VERSION;

	/* Call InitializeSecurityContext. */
	ret = InitializeSecurityContext(
		&s->client_creds,
		&s->context,
		NULL,
		ssp_i_flags,
		0,
		0,
		&in_buffer,
		0,
		NULL,
		&out_buffer,
		&ssp_o_flags,
		&expiry);

	vtrace("TLS: InitializeSecurityContext -> 0x%x (%s)\n", (unsigned)ret,
		win32_strerror(ret));

	/*
	 * If InitializeSecurityContext was successful (or if the error was
	 * one of the special extended ones), send the contends of the output
	 * buffer to the server.
	 */
	if (ret == SEC_E_OK                ||
	    ret == SEC_I_CONTINUE_NEEDED   ||
	    (FAILED(ret) && (ssp_o_flags & ISC_RET_EXTENDED_ERROR))) {
	    if (out_buffers[0].cbBuffer != 0 &&
		    out_buffers[0].pvBuffer != NULL) {
		nrw = send(s->sock, out_buffers[0].pvBuffer,
			out_buffers[0].cbBuffer, 0);
		if (nrw == SOCKET_ERROR) {
		    ret = WSAGetLastError();
		    sioc_set_error("send: error %d (%s)\n", (int)ret,
			    win32_strerror(ret));
		    FreeContextBuffer(out_buffers[0].pvBuffer);
		    break;
		}
		vtrace("TLS: %d bytes of handshake data sent\n", nrw);
#if defined(VERBOSE) /*[*/
		print_hex_dump(">enc", nrw, out_buffers[0].pvBuffer);
#endif /*]*/

		/* Free output buffer. */
		FreeContextBuffer(out_buffers[0].pvBuffer);
		out_buffers[0].pvBuffer = NULL;
	    }
	}

	/*
	 * If InitializeSecurityContext returned SEC_E_INCOMPLETE_MESSAGE,
	 * then we need to read more data from the server and try again.
	 */
	if (ret == SEC_E_INCOMPLETE_MESSAGE) {
	    if (in_buffers[1].BufferType == SECBUFFER_MISSING) {
		n2read = in_buffers[1].cbBuffer;
	    } else {
		n2read = MIN_READ;
	    }
	    continue;
	} else {
	    n2read = MIN_READ;
	}

	/*
	 * If InitializeSecurityContext returned SEC_E_OK, then the
	 * handshake completed successfully.
	 */
	if (ret == SEC_E_OK) {
	    /*
	     * If the "extra" buffer contains data, this is encrypted
	     * application protocol layer stuff. It needs to be saved. The
	     * application layer will later decrypt it with DecryptMessage.
	     */
	    vtrace("TLS: Handshake was successful\n");

	    if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
		/* Interestingly, in_buffers[1].pvBuffer is NULL here. */
		vtrace("TLS: %d bytes of encrypted data saved\n",
			(int)in_buffers[1].cbBuffer);
		memmove(s->rcvbuf,
			s->rcvbuf + s->rcvbuf_len - in_buffers[1].cbBuffer,
			in_buffers[1].cbBuffer);
		s->rcvbuf_len = in_buffers[1].cbBuffer;
	    } else {
		s->rcvbuf_len = 0;
	    }
	    break;
	}

	if (ret == SEC_E_UNSUPPORTED_FUNCTION) {
	    vtrace("TLS: SEC_E_UNSUPPORTED_FUNCTION from InitializeSecurityContext -- usually means requested TLS version not supported by server\n");
	}

	if (ret == SEC_E_WRONG_PRINCIPAL) {
	    vtrace("TLS: SEC_E_WRONG_PRINCIPAL from InitializeSecurityContext -- bad server certificate\n");
	}

	/* Check for fatal error. */
	if (FAILED(ret)) {
	    sioc_set_error("InitializeSecurityContext: error 0x%lx (%s)%s\n",
		    ret, win32_strerror(ret), explain_error(ret));
	    break;
	}

	/*
	 * If InitializeSecurityContext returned SEC_I_INCOMPLETE_CREDENTIALS,
	 * then the server just requested client authentication.
	 */
	if (ret == SEC_I_INCOMPLETE_CREDENTIALS) {
	    /*
	     * Busted. The server has requested client authentication and
	     * the credential we supplied didn't contain a client certificate.
	     * This function will read the list of trusted certificate
	     * authorities ("issuers") that was received from the server
	     * and attempt to find a suitable client certificate that
	     * was issued by one of these. If this function is successful,
	     * then we will connect using the new certificate. Otherwise,
	     * we will attempt to connect anonymously (using our current
	     * credentials).
	     */
	    get_new_client_credentials(&s->client_creds, &s->context);

	    /* Go around again. */
	    do_read = false;
	    ret = SEC_I_CONTINUE_NEEDED;
	    continue;
	}

	if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
	    /*
	     * Copy any leftover data from the "extra" buffer, and go around
	     * again.
	     */
	    vtrace("TLS: %lu bytes of extra data copied\n",
		    in_buffers[1].cbBuffer);
	    memmove(s->rcvbuf,
		    s->rcvbuf + s->rcvbuf_len - in_buffers[1].cbBuffer,
		    in_buffers[1].cbBuffer);
	    s->rcvbuf_len = in_buffers[1].cbBuffer;
	} else {
	    s->rcvbuf_len = 0;
	}
    }

    /* Delete the security context in the case of a fatal error. */
    if (ret != SEC_E_OK && ret != WSAEWOULDBLOCK) {
	DeleteSecurityContext(&s->context);
    } else {
	s->context_set = true;
    }

    return ret;
}

/* Client handshake, first phase. */
static SECURITY_STATUS
perform_client_handshake(
	schannel_sio_t *s,		/* in, out */
	LPSTR		server_name)	/* in */
{
    SecBufferDesc   out_buffer;
    SecBuffer       out_buffers[1];
    DWORD           ssp_i_flags, ssp_o_flags;
    int             data;
    TimeStamp       expiry;
    SECURITY_STATUS scRet;

    ssp_i_flags =
	ISC_REQ_SEQUENCE_DETECT   |
	ISC_REQ_REPLAY_DETECT     |
	ISC_REQ_CONFIDENTIALITY   |
	ISC_RET_EXTENDED_ERROR    |
	ISC_REQ_ALLOCATE_MEMORY   |
	ISC_REQ_STREAM;

    /* Initiate a ClientHello message and generate a token. */
    out_buffers[0].pvBuffer   = NULL;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;
    out_buffers[0].cbBuffer   = 0;

    out_buffer.cBuffers  = 1;
    out_buffer.pBuffers  = out_buffers;
    out_buffer.ulVersion = SECBUFFER_VERSION;

    scRet = InitializeSecurityContext(
	    &s->client_creds,
	    NULL,
	    server_name,
	    ssp_i_flags,
	    0,
	    0,
	    NULL,
	    0,
	    &s->context,
	    &out_buffer,
	    &ssp_o_flags,
	    &expiry);

    if (scRet != SEC_I_CONTINUE_NEEDED) {
	sioc_set_error("InitializeSecurityContext: error %lx (%s)%s\n", scRet,
		win32_strerror(scRet), explain_error(scRet));
	return scRet;
    }

    /* Send response to server, if there is one. */
    if (out_buffers[0].cbBuffer != 0 && out_buffers[0].pvBuffer != NULL) {
	data = send(s->sock, out_buffers[0].pvBuffer, out_buffers[0].cbBuffer,
		0);
	if (data == SOCKET_ERROR) {
	    int err = WSAGetLastError();
	    sioc_set_error("send: error %d (%s)\n", err, win32_strerror(err));
	    FreeContextBuffer(out_buffers[0].pvBuffer);
	    DeleteSecurityContext(&s->context);
	    return err;
	}
	vtrace("TLS: %d bytes of handshake data sent\n", data);
	FreeContextBuffer(out_buffers[0].pvBuffer);
	out_buffers[0].pvBuffer = NULL;
    }

    return client_handshake_loop(s, true);
}

/* Manually verify a server certificate. */
static DWORD
verify_server_certificate(
	PCCERT_CONTEXT server_cert,
	PSTR server_name,
	DWORD cert_flags)
{
    HTTPSPolicyCallbackData  policy_https;
    CERT_CHAIN_POLICY_PARA   policy_params;
    CERT_CHAIN_POLICY_STATUS policy_status;
    CERT_CHAIN_PARA          chain_params;
    PCCERT_CHAIN_CONTEXT     chain_context = NULL;
    DWORD                    server_name_size, status;
    LPSTR rgszUsages[]     = { szOID_PKIX_KP_SERVER_AUTH,
                               szOID_SERVER_GATED_CRYPTO,
                               szOID_SGC_NETSCAPE };
    DWORD usages_count     = sizeof(rgszUsages) / sizeof(LPSTR);
    PWSTR server_name_wide = NULL;

    vtrace("TLS: Verifying server certificate manually\n");

    /* Convert server name to Unicode. */
    server_name_size = MultiByteToWideChar(CP_ACP, 0, server_name, -1, NULL, 0);
    server_name_wide = Malloc(server_name_size * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, server_name, -1, server_name_wide,
	    server_name_size);

    /* Build certificate chain. */
    memset(&chain_params, 0, sizeof(chain_params));
    chain_params.cbSize = sizeof(chain_params);
    chain_params.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
    chain_params.RequestedUsage.Usage.cUsageIdentifier = usages_count;
    chain_params.RequestedUsage.Usage.rgpszUsageIdentifier = rgszUsages;

    if (!CertGetCertificateChain(
		NULL,
		server_cert,
		NULL,
		server_cert->hCertStore,
		&chain_params,
		0,
		NULL,
		&chain_context)) {
	status = GetLastError();
	sioc_set_error("CertGetCertificateChain: error 0x%lx (%s)\n", status,
		win32_strerror(status));
	goto done;
    }

    /* Validate certificate chain. */
    ZeroMemory(&policy_https, sizeof(HTTPSPolicyCallbackData));
    policy_https.cbStruct       = sizeof(HTTPSPolicyCallbackData);
    policy_https.dwAuthType     = AUTHTYPE_SERVER;
    policy_https.fdwChecks      = cert_flags;
    policy_https.pwszServerName = server_name_wide;

    memset(&policy_params, 0, sizeof(policy_params));
    policy_params.cbSize = sizeof(policy_params);
    policy_params.pvExtraPolicyPara = &policy_https;

    memset(&policy_status, 0, sizeof(policy_status));
    policy_status.cbSize = sizeof(policy_status);

    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain_context,
		&policy_params, &policy_status)) {
	status = GetLastError();
	sioc_set_error("CertVerifyCertificateChainPolicy: error 0x%lx (%s)%s\n",
		status, win32_strerror(status),
		explain_error(status));
	goto done;
    }

    if (policy_status.dwError) {
	status = policy_status.dwError;
	sioc_set_error("CertVerifyCertificateChainPolicy: error 0x%lx (%s)%s\n",
		status, win32_strerror(status),
		explain_error(status));
	goto done;
    }

    status = SEC_E_OK;

done:
    if (chain_context != NULL) {
	CertFreeCertificateChain(chain_context);
    }
    if (server_name_wide != NULL) {
	Free(server_name_wide);
    }

    return status;
}

/* Display a connection. */
static void
display_connection_info(varbuf_t *v, CtxtHandle *context)
{
    SECURITY_STATUS status;
    SecPkgContext_ConnectionInfo connection_info;

    status = QueryContextAttributes(context, SECPKG_ATTR_CONNECTION_INFO,
	    (PVOID)&connection_info);
    if (status != SEC_E_OK) {
	vtrace("QueryContextAttributes: error 0x%x (%s)\n", (unsigned)status,
		win32_strerror(status));
	return;
    }

    vb_appendf(v, "Protocol: ");
    switch (connection_info.dwProtocol) {
    case SP_PROT_SSL2_CLIENT:
	vb_appendf(v, "SSL 2.0\n");
	break;
    case SP_PROT_SSL3_CLIENT:
	vb_appendf(v, "SSL 3.0\n");
	break;
    case SP_PROT_TLS1_CLIENT:
	vb_appendf(v, "TLS 1.0\n");
	break;
    case SP_PROT_TLS1_1_CLIENT:
	vb_appendf(v, "TLS 1.1\n");
	break;
    case SP_PROT_TLS1_2_CLIENT:
	vb_appendf(v, "TLS 1.2\n");
	break;
    case SP_PROT_TLS1_3_CLIENT:
	vb_appendf(v, "TLS 1.3\n");
	break;
    default:
	vb_appendf(v, "0x%x\n", (unsigned)connection_info.dwProtocol);
	break;
    }

    vb_appendf(v, "Cipher: ");
    switch (connection_info.aiCipher) {
    case CALG_3DES:
	vb_appendf(v, "Triple DES\n");
	break;
    case CALG_AES:
	vb_appendf(v, "AES\n");
	break;
    case CALG_AES_128:
	vb_appendf(v, "AES 128\n");
	break;
    case CALG_AES_256:
	vb_appendf(v, "AES 256\n");
	break;
    case CALG_DES:
	vb_appendf(v, "DES\n");
	break;
    case CALG_RC2:
	vb_appendf(v, "RC2\n");
	break;
    case CALG_RC4:
	vb_appendf(v, "RC4\n");
	break;
    default:
	vb_appendf(v, "0x%x\n", connection_info.aiCipher);
	break;
    }

    vb_appendf(v, "Cipher strength: %d\n",
	    (int)connection_info.dwCipherStrength);

    vb_appendf(v, "Hash: ");
    switch (connection_info.aiHash) {
    case CALG_MD5:
	vb_appendf(v, "MD5\n");
	break;
    case CALG_SHA:
	vb_appendf(v, "SHA\n");
	break;
    default:
	vb_appendf(v, "0x%x\n", connection_info.aiHash);
	break;
    }

    vb_appendf(v, "Hash strength: %d\n", (int)connection_info.dwHashStrength);

    vb_appendf(v, "Key exchange: ");
    switch (connection_info.aiExch) {
    case CALG_RSA_KEYX:
    case CALG_RSA_SIGN:
	vb_appendf(v, "RSA\n");
	break;
    case CALG_KEA_KEYX:
	vb_appendf(v, "KEA\n");
	break;
    case CALG_DH_EPHEM:
	vb_appendf(v, "DH Ephemeral\n");
	break;
    default:
	vb_appendf(v, "0x%x\n", connection_info.aiExch);
	break;
    }

    vb_appendf(v, "Key exchange strength: %d\n",
	    (int)connection_info.dwExchStrength);
}

/* Free an sio context. */
static void
sio_free(schannel_sio_t *s)
{
    s->sock = INVALID_SOCKET;

    /* Free the SSPI context handle. */
    if (s->context_set) {
        DeleteSecurityContext(&s->context);
	memset(&s->context, 0, sizeof(s->context));
        s->context_set = false;
    }

    /* Free the SSPI credentials handle. */
    if (s->client_creds_set) {
        FreeCredentialsHandle(&s->client_creds);
	memset(&s->client_creds, 0, sizeof(s->client_creds));
        s->client_creds_set = false;
    }

    /* Free the receive buffer. */
    if (s->rcvbuf != NULL) {
	Free(s->rcvbuf);
	s->rcvbuf = NULL;
    }

    /* Free the record buffer. */
    if (s->prbuf != NULL) {
	Free(s->prbuf);
	s->prbuf = NULL;
    }

    /* Free the send buffer. */
    if (s->sendbuf != NULL) {
	Free(s->sendbuf);
	s->sendbuf = NULL;
    }

    /* Free the session info. */
    if (s->session_info != NULL) {
	Free(s->session_info);
	s->session_info = NULL;
    }

    /* Free the server cert info. */
    if (s->server_cert_info != NULL) {
	Free(s->server_cert_info);
	s->server_cert_info = NULL;
    }

    /* Free the server subject. */
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
 * Create a new context.
 */
sio_init_ret_t
sio_init(tls_config_t *c, const char *password, sio_t *sio_ret)
{
    schannel_sio_t *s;

    sioc_error_reset();

    config = c;

    s = (schannel_sio_t *)Malloc(sizeof(schannel_sio_t));
    memset(s, 0, sizeof(*s));
    s->sock = INVALID_SOCKET;

    /* Create credentials. */
    if (create_credentials(config->client_cert, &s->client_creds, &s->manual)) {
	vtrace("TLS: Error creating credentials\n");
	goto fail;
    }
    s->client_creds_set = true;

    *sio_ret = (sio_t)s;
    return SI_SUCCESS;

fail:
    sio_free(s);
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
    schannel_sio_t *s;
    const char *accept_hostname = hostname;
    SECURITY_STATUS status;
    PCCERT_CONTEXT remote_cert_context = NULL;
    size_t recsz;
    varbuf_t v;
    char *cert_desc = NULL;
    char *cert_subjects = NULL;
    size_t sl;

    sioc_error_reset();

    *data = false;
    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIG_FAILURE;
    }
    s = (schannel_sio_t *)sio;
    if (s->negotiate_pending) {
	if (s->sock == INVALID_SOCKET) {
	    sioc_set_error("Invalid sio (missing socket)");
	    return SIG_FAILURE;
	}

	/* Continue handshake. */
	status = client_handshake_loop(s, true);
    } else {
	if (s->sock != INVALID_SOCKET) {
	    sioc_set_error("Invalid sio (already negotiated)");
	    return SIG_FAILURE;
	}
	s->sock = sock;
	s->hostname = hostname;

	/*
	 * Allocate the initial receive buffer.
	 * This is temporary, because we can't learn the receive stream sizes
	 * until we have finished negotiating, but we need a receive buffer to
	 * negotiate in the first place.
	 */
	s->rcvbuf = Malloc(INBUF);

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

	/* Perform handshake. */
	status = perform_client_handshake(s, (LPSTR)accept_hostname);
    }

    if (status == WSAEWOULDBLOCK) {
	s->negotiate_pending = true;
	return SIG_WANTMORE;
    } else if (status != 0) {
	vtrace("TLS: Error performing handshake\n");
	goto fail;
    }

    /* Get the server's certificate. */
    status = QueryContextAttributes(&s->context,
	    SECPKG_ATTR_REMOTE_CERT_CONTEXT, (PVOID)&remote_cert_context);
    if (status != SEC_E_OK) {
	sioc_set_error("QueryContextAttributes: error 0x%x (%s)",
		(unsigned)status, win32_strerror(status));
	goto fail;
    }

    /*
     * Get the description of the server certificate chain.
     */
    vb_init(&v);
    display_cert_chain(&v, remote_cert_context);
    cert_desc = vb_consume(&v);

    vb_init(&v);
    display_cert_subjects(&v, remote_cert_context);
    cert_subjects = vb_consume(&v);

    /* Attempt to validate the server certificate. */
    if (s->manual && config->verify_host_cert) {
	status = verify_server_certificate(remote_cert_context,
		(LPSTR)accept_hostname, 0);
	if (status) {
	    vtrace("TLS: Error 0x%x authenticating server credentials\n",
		    (unsigned)status);
	    goto fail;
	}
    }

    /* Free the server certificate context. */
    CertFreeCertificateContext(remote_cert_context);
    remote_cert_context = NULL;

    /* Read stream encryption properties. */
    status = QueryContextAttributes(&s->context, SECPKG_ATTR_STREAM_SIZES,
	    &s->sizes);
    if (status != SEC_E_OK) {
	sioc_set_error("QueryContextAttributes: error 0x%x (%s)",
		(unsigned)status, win32_strerror(status));
	goto fail;
    }

    /* Display connection info. */
    vb_init(&v);
    display_connection_info(&v, &s->context);
    s->session_info = vb_consume(&v);
    sl = strlen(s->session_info);
    if (sl > 0 && s->session_info[sl - 1] == '\n') {
	s->session_info[sl - 1] = '\0';
    }

    /* Display server_cert info. */
    s->server_cert_info = cert_desc;
    cert_desc = NULL;
    sl = strlen(s->server_cert_info);
    if (sl > 0 && s->server_cert_info[sl - 1] == '\n') {
	s->server_cert_info[sl - 1] = '\0';
    }

    /* Display server subject. */
    s->server_subjects = cert_subjects;
    cert_subjects = NULL;
    sl = strlen(s->server_subjects);
    if (sl > 0 && s->server_subjects[sl - 1] == '\n') {
	s->server_subjects[sl - 1] = '\0';
    }

    /* Account for any extra data. */
    if (s->rcvbuf_len > 0) {
	*data = true;
    }

    /* Reallocate the receive buffer. */
    vtrace("TLS: Sizes: header %d, trailer %d, max message %d\n",
	    (int)s->sizes.cbHeader, (int)s->sizes.cbTrailer,
	    (int)s->sizes.cbMaximumMessage);
    recsz = s->sizes.cbHeader + s->sizes.cbTrailer + s->sizes.cbMaximumMessage;
    if (recsz > INBUF) {
	s->rcvbuf = Realloc(s->rcvbuf, recsz);
    }
    s->prbuf = Malloc(recsz);
    s->sendbuf = Malloc(recsz);

    /* Success. */
    s->secure_unverified = !config->verify_host_cert;
    s->negotiated = true;
    return SIG_SUCCESS;

fail:
    /* Free the server certificate context. */
    if (remote_cert_context != NULL) {
        CertFreeCertificateContext(remote_cert_context);
        remote_cert_context = NULL;
    }

    /* Free the SSPI context handle. */
    if (s->context_set) {
        DeleteSecurityContext(&s->context);
	memset(&s->context, 0, sizeof(s->context));
        s->context_set = false;
    }

    /* Free the SSPI credentials handle. */
    if (s->client_creds_set) {
        FreeCredentialsHandle(&s->client_creds);
	memset(&s->client_creds, 0, sizeof(s->client_creds));
        s->client_creds_set = false;
    }

    if (cert_desc != NULL) {
	Free(cert_desc);
    }
    if (cert_subjects != NULL) {
	Free(cert_subjects);
    }

    return SIG_FAILURE;
}

/*
 * Read and decrypt data.
 */
static SECURITY_STATUS
read_decrypt(
	schannel_sio_t *s,	/* in */
	CtxtHandle *context,	/* in */
	bool *renegotiated)	/* out */
{
    SecBuffer          *data_buffer_ptr, *extra_buffer_ptr;

    SECURITY_STATUS    ret;
    SecBufferDesc      message;
    SecBuffer          buffers[4];

    int                nr;
    int                i;
    int                n2read = s->sizes.cbHeader;

    *renegotiated = false;

    /* Read data from server until done. */
    ret = SEC_E_OK;
    while (true) {
	data_buffer_ptr = NULL;
	extra_buffer_ptr = NULL;

	/* Read some data. */
	if (s->rcvbuf_len == 0 || ret == SEC_E_INCOMPLETE_MESSAGE) {
	    /* Get the data */
            nr = recv(s->sock, s->rcvbuf + s->rcvbuf_len, n2read, 0);
	    vtrace("TLS: %d/%d bytes of encrypted application data received\n",
		    nr, n2read);
            if (nr == SOCKET_ERROR) {
		ret = WSAGetLastError();
		sioc_set_error("recv: error %d (%s)", (int)ret,
			win32_strerror(ret));
		break;
            } else if (nr == 0) {
		/* Server disconnected. */
		vtrace("TLS: Server disconnected.\n");
		s->negotiated = false;
		ret = SEC_E_OK;
		break;
            } else {
		/* Success. */
#if defined(VERBOSE) /*[*/
		print_hex_dump("<enc", nr,
			(unsigned char *)s->rcvbuf + s->rcvbuf_len);
#endif /*]*/
		s->rcvbuf_len += nr;
            }
        }

        /* Try to decrypt it. */
	buffers[0].pvBuffer     = s->rcvbuf;
	buffers[0].cbBuffer     = (DWORD)s->rcvbuf_len;
	buffers[0].BufferType   = SECBUFFER_DATA;
	buffers[1].BufferType   = SECBUFFER_EMPTY;
	buffers[2].BufferType   = SECBUFFER_EMPTY;
	buffers[3].BufferType   = SECBUFFER_EMPTY;

	message.ulVersion       = SECBUFFER_VERSION;
	message.cBuffers        = 4;
	message.pBuffers        = buffers;
	ret = DecryptMessage(context, &message, 0, NULL);
	if (ret == SEC_I_CONTEXT_EXPIRED) {
	    /* Server signalled end of session. Treat it like EOF. */
	    vtrace("TLS: Server signaled end of session.\n");
	    s->negotiated = false;
	    ret = SEC_E_OK;
	    break;
	}
        if (ret != SEC_E_OK &&
	    ret != SEC_I_RENEGOTIATE &&
	    ret != SEC_I_CONTEXT_EXPIRED &&
	    ret != SEC_E_INCOMPLETE_MESSAGE) {
	    sioc_set_error("DecryptMessage: error 0x%x (%s)\n", (unsigned)ret,
		    win32_strerror(ret));
	    return ret;
	}

	if (ret == SEC_E_INCOMPLETE_MESSAGE) {
	    /* Nibble some more. */
	    if (buffers[0].BufferType == SECBUFFER_MISSING) {
		n2read = buffers[0].cbBuffer;
	    } else {
		n2read = s->sizes.cbHeader;
	    }
	    continue;
	} else {
	    n2read = s->sizes.cbHeader;
	}

	/* Locate data and (optional) extra buffers. */
	data_buffer_ptr  = NULL;
	extra_buffer_ptr = NULL;
	for (i = 1; i < 4; i++) {
	    if (data_buffer_ptr == NULL &&
		    buffers[i].BufferType == SECBUFFER_DATA) {
		data_buffer_ptr  = &buffers[i];
	    }
	    if (extra_buffer_ptr == NULL &&
		    buffers[i].BufferType == SECBUFFER_EXTRA) {
		extra_buffer_ptr = &buffers[i];
	    }
	}

	/* Check for completion. */
        if (data_buffer_ptr != NULL && data_buffer_ptr->cbBuffer) {
	    /* Copy decrypted data to the record buffer. */
	    memcpy(s->prbuf, data_buffer_ptr->pvBuffer,
		    data_buffer_ptr->cbBuffer);
	    s->prbuf_len = data_buffer_ptr->cbBuffer;
	    s->rcvbuf_len = 0;
	    vtrace("TLS: Got %lu decrypted bytes\n", data_buffer_ptr->cbBuffer);
	}

	/* Move any "extra" data to the receive buffer for next time. */
	if (extra_buffer_ptr != NULL) {
	    vtrace("TLS: %d bytes extra after decryption\n",
		    (int)extra_buffer_ptr->cbBuffer);
	    memmove(s->rcvbuf, extra_buffer_ptr->pvBuffer,
		    extra_buffer_ptr->cbBuffer);
	    s->rcvbuf_len = extra_buffer_ptr->cbBuffer;
	}

	/*
	 * Check for renegotiation.
	 * It's not clear to me if we can get data back *and* this return code,
	 * of if it's one or the other.
	 */
	if (ret == SEC_I_RENEGOTIATE) {
	    /* The server wants to perform another handshake sequence. */
	    vtrace("TLS: Server requested renegotiate\n");
	    ret = client_handshake_loop(s, false);
	    if (ret != SEC_E_OK) {
		s->negotiated = false;
		return ret;
	    }
	    *renegotiated = true; /* so we try to read more */
	}

	if (ret == SEC_E_OK) {
	    break;
	}
    }

    return ret;
}

/* Send an encrypted message. */
static SECURITY_STATUS
encrypt_send(
	schannel_sio_t *s,
	const char *buf,
	size_t len)
{
    SECURITY_STATUS    ret;
    SecBufferDesc      message;
    SecBuffer          buffers[4];
    int                nw;

    /* Copy the data. */
    memcpy(s->sendbuf + s->sizes.cbHeader, buf, len);

    /* Encrypt the data. */
    buffers[0].pvBuffer     = s->sendbuf;
    buffers[0].cbBuffer     = s->sizes.cbHeader;
    buffers[0].BufferType   = SECBUFFER_STREAM_HEADER;

    buffers[1].pvBuffer     = s->sendbuf + s->sizes.cbHeader;
    buffers[1].cbBuffer     = (DWORD)len;
    buffers[1].BufferType   = SECBUFFER_DATA;

    buffers[2].pvBuffer     = s->sendbuf + s->sizes.cbHeader + len;
    buffers[2].cbBuffer     = s->sizes.cbTrailer;
    buffers[2].BufferType   = SECBUFFER_STREAM_TRAILER;

    buffers[3].pvBuffer     = SECBUFFER_EMPTY;
    buffers[3].cbBuffer     = SECBUFFER_EMPTY;
    buffers[3].BufferType   = SECBUFFER_EMPTY;

    message.ulVersion       = SECBUFFER_VERSION;
    message.cBuffers        = 4;
    message.pBuffers        = buffers;
    ret = EncryptMessage(&s->context, 0, &message, 0);
    if (FAILED(ret)) {
	sioc_set_error("EncryptMessage: error 0x%x (%s)", (unsigned)ret,
		win32_strerror(ret));
	return ret;
    }

    /* Send the encrypted data to the server. */
    nw = send(s->sock, s->sendbuf,
	    buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer, 0);
	vtrace("TLS: %d bytes of encrypted data sent\n", nw);
    if (nw < 0) {
	ret = WSAGetLastError();
	sioc_set_error("send: error %d (%s)", (int)ret, win32_strerror(ret));
    } else {
#if defined(VERBOSE) /*[*/
	print_hex_dump(">enc", nw, (PBYTE)s->sendbuf);
#endif /*]*/
    }

    return ret;
}

/* Disconnect from the server. */
static SECURITY_STATUS
disconnect_from_server(schannel_sio_t *s)
{
    PBYTE         outbuf;
    DWORD         type, flags, out_flags;
    int		  n2w;
    int           nw;
    SECURITY_STATUS status;
    SecBufferDesc out_buffer;
    SecBuffer     out_buffers[1];
    TimeStamp     expiry;

    /* Notify schannel that we are about to close the connection. */
    type = SCHANNEL_SHUTDOWN;

    out_buffers[0].pvBuffer   = &type;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;
    out_buffers[0].cbBuffer   = sizeof(type);

    out_buffer.cBuffers  = 1;
    out_buffer.pBuffers  = out_buffers;
    out_buffer.ulVersion = SECBUFFER_VERSION;

    status = ApplyControlToken(&s->context, &out_buffer);
    if (FAILED(status)) {
	vtrace("TLS: ApplyControlToken: error 0x%x (%s)\n", (unsigned)status,
		win32_strerror(status));
	return status;
    }

    /* Build a TLS close notify message. */
    flags = ISC_REQ_SEQUENCE_DETECT   |
		  ISC_REQ_REPLAY_DETECT     |
		  ISC_REQ_CONFIDENTIALITY   |
		  ISC_RET_EXTENDED_ERROR    |
		  ISC_REQ_ALLOCATE_MEMORY   |
		  ISC_REQ_STREAM;

    out_buffers[0].pvBuffer   = NULL;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;
    out_buffers[0].cbBuffer   = 0;

    out_buffer.cBuffers  = 1;
    out_buffer.pBuffers  = out_buffers;
    out_buffer.ulVersion = SECBUFFER_VERSION;

    status = InitializeSecurityContext(&s->client_creds,
	    &s->context,
	    NULL,
	    flags,
	    0,
	    0,
	    NULL,
	    0,
	    &s->context,
	    &out_buffer,
	    &out_flags,
	    &expiry);

    if (FAILED(status)) {
	vtrace("TLS: InitializeSecurityContext: error 0x%x (%s)%s\n",
		(unsigned)status, win32_strerror(status),
		explain_error(status));
	return status;
    }

    outbuf = out_buffers[0].pvBuffer;
    n2w = out_buffers[0].cbBuffer;

    /* Send the close notify message to the server. */
    if (outbuf != NULL && n2w != 0) {
	nw = send(s->sock, (char *)outbuf, n2w, 0);
	if (nw == SOCKET_ERROR) {
	    status = WSAGetLastError();
	    vtrace("TLS: send: error %d (%s)\n", (int)status,
		    win32_strerror(status));
	} else {
	    vtrace("TLS: %d bytes of handshake data sent\n", nw);
#if defined(VERBOSE) /*[*/
	    print_hex_dump(">enc", nw, outbuf);
#endif /*]*/
	}
	FreeContextBuffer(outbuf);
    }
    vtrace("TLS: Sent TLS disconnect\n");

    return status;
}

/*
 * Read encrypted data from a socket.
 * Returns the data length, SIO_EOF for EOF, SIO_FATAL_ERROR for a fatal error,
 * SIO_EWOULDBLOCK for incomplete input.
 */
int
sio_read(sio_t sio, char *buf, size_t buflen)
{
    schannel_sio_t *s;
    SECURITY_STATUS ret;
    bool renegotiated;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (schannel_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	sioc_set_error("Invalid sio (not negotiated)");
	return SIO_FATAL_ERROR;
    }

    if (!s->negotiated) {
	return SIO_EOF;
    }

    if (s->prbuf_len > 0) {
	size_t copy_len = s->prbuf_len;

	/* Record already buffered. */
	if (copy_len > buflen) {
	    copy_len = buflen;
	}
	memcpy(buf, s->prbuf, copy_len);
	s->prbuf_len -= copy_len;
	return (int)copy_len;
    }

    ret = read_decrypt(s, &s->context, &renegotiated);
    if (ret != SEC_E_OK) {
	if (ret == WSAEWOULDBLOCK) {
	    return SIO_EWOULDBLOCK;
	}
	s->negotiated = false;
	vtrace("TLS: sio_read: fatal error, ret = 0x%x\n", (unsigned)ret);
	return SIO_FATAL_ERROR;
    }

    if (!renegotiated && s->prbuf_len == 0) {
	/* End of file. */
	s->negotiated = false;
	vtrace("TLS: sio_read: EOF\n");
	return SIO_EOF;
    }

    /* Got a complete record. */
    return sio_read(sio, buf, buflen);
}

/*
 * Write encrypted data on the socket.
 * Returns the data length or SIO_FATAL_ERROR.
 */
int
sio_write(sio_t sio, const char *buf, size_t buflen)
{
    schannel_sio_t *s;
    size_t len_left = buflen;

    sioc_error_reset();

    if (sio == NULL) {
	sioc_set_error("NULL sio");
	return SIO_FATAL_ERROR;
    }
    s = (schannel_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	sioc_set_error("Invalid sio (not negotiated)");
	return SIO_FATAL_ERROR;
    }

    do {
	size_t n2w = len_left;
	SECURITY_STATUS ret;

	if (n2w > s->sizes.cbMaximumMessage) {
	    n2w = s->sizes.cbMaximumMessage;
	}
	ret = encrypt_send(s, buf, n2w);
	if (ret != SEC_E_OK) {
	    s->negotiated = false;
	    return SIO_FATAL_ERROR;
	}
	len_left -= n2w;
	buf += n2w;
    } while (len_left > 0);

    return (int)buflen;
}

/* Closes the TLS connection. */
void
sio_close(sio_t sio)
{
    schannel_sio_t *s;

    if (sio == NULL) {
	return;
    }
    s = (schannel_sio_t *)sio;
    if (s->sock == INVALID_SOCKET) {
	return;
    }

    if (s->negotiated) {
	disconnect_from_server(s);
    }
    sio_free(s);
}

/*
 * Returns true if the current connection is unverified.
 */
bool
sio_secure_unverified(sio_t sio)
{
    schannel_sio_t *s = (schannel_sio_t *)sio;
    return s? s->secure_unverified: false;
}

/*
 * Returns a bitmap of the supported options.
 */
unsigned
sio_options_supported(void)
{ 
    return TLS_OPT_CLIENT_CERT | TLS_OPT_MIN_PROTOCOL | TLS_OPT_MAX_PROTOCOL;
}

/*
 * Returns session information.
 */
const char *
sio_session_info(sio_t sio)
{
    schannel_sio_t *s = (schannel_sio_t *)sio;
    return s? s->session_info: NULL;
}

/*
 * Returns server cert information.
 */
const char *
sio_server_cert_info(sio_t sio)
{
    schannel_sio_t *s = (schannel_sio_t *)sio;
    return s? s->server_cert_info: NULL;
}

/*
 * Returns server subject names.
 */
const char *
sio_server_subject_names(sio_t sio)
{
    schannel_sio_t *s = (schannel_sio_t *)sio;
    return s? s->server_subjects: NULL;
}

/*
 * Returns the provider name.
 */
const char *
sio_provider(void)
{
    return "Windows Schannel";
}
