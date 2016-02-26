/*
 * Copyright (c) 1993-2016 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 * DON RUSSELL, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	telnet.c
 *		This module initializes and manages a telnet socket to
 *		the given IBM host.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
# include <sys/ioctl.h>
# include <netinet/in.h>
#endif /*]*/
#define TELCMDS 1
#define TELOPTS 1
#include "arpa_telnet.h"
#if !defined(_WIN32) /*[*/
# include <arpa/inet.h>
#endif /*]*/
#include <errno.h>
#include <fcntl.h>
#if !defined(_WIN32) /*[*/
# include <netdb.h>
#endif /*]*/
#include <stdint.h>
#if defined(HAVE_LIBSSL) /*[*/
# if defined(_WIN32) /*[*/
#  include "ssl_dll.h"
# endif /*]*/
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/conf.h>
# include <openssl/x509v3.h>
#endif /*]*/
#include "tn3270e.h"
#include "3270ds.h"

#include "appres.h"

#include "actions.h"
#include "b8.h"
#include "ctlrc.h"
#include "host.h"
#include "kybd.h"
#include "lazya.h"
#include "linemode.h"
#include "macros.h"
#include "nvt.h"
#include "popups.h"
#include "proxy.h"
#include "resolver.h"
#include "ssl_passwd_gui.h"
#include "status.h"
#include "telnet.h"
#include "telnet_core.h"
#include "telnet_gui.h"
#include "telnet_private.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "w3misc.h"
#include "xio.h"

#if defined(_WIN32) && defined(HAVE_LIBSSL) /*[*/
# define ROOT_CERTS	"root_certs.txt"
#endif /*]*/

#if !defined(TELOPT_NAWS) /*[*/
# define TELOPT_NAWS	31
#endif /*]*/

#if !defined(TELOPT_STARTTLS) /*[*/
# define TELOPT_STARTTLS	46
#endif /*]*/
#define TLS_FOLLOWS	1

#define BUFSZ		16384
#define TRACELINE	72

#define N_OPTS		256

/* Globals */
char    	*hostname = NULL;
time_t          ns_time;
int             ns_brcvd;
int             ns_rrcvd;
int             ns_bsent;
int             ns_rsent;
unsigned char  *obuf;		/* 3270 output buffer */
unsigned char  *obptr = (unsigned char *) NULL;
int             linemode = 1;
#if defined(LOCAL_PROCESS) /*[*/
bool		local_process = false;
#endif /*]*/
char           *termtype;

/* Statics */
static socket_t sock = INVALID_SOCKET;	/* active socket */
#if defined(_WIN32) /*[*/
static HANDLE	sock_handle = INVALID_HANDLE_VALUE;
#endif /*]*/
static unsigned char myopts[N_OPTS], hisopts[N_OPTS];
			/* telnet option flags */
static bool did_ne_send;
static bool deferred_will_ttype;
static unsigned char *ibuf = (unsigned char *) NULL;
			/* 3270 input buffer */
static unsigned char *ibptr;
static int      ibuf_size = 0;	/* size of ibuf */
static unsigned char *obuf_base = NULL;
static int	obuf_size = 0;
static unsigned char *netrbuf = NULL;
			/* network input buffer */
static unsigned char *sbbuf = NULL;
			/* telnet sub-option buffer */
static unsigned char *sbptr;
static unsigned char telnet_state;
static int      syncing;
#if !defined(_WIN32) /*[*/
static ioid_t output_id = NULL_IOID;
#endif /*]*/
static ioid_t	connect_timeout_id = NULL_IOID;	/* explicit Connect timeout */
static char     ttype_tmpval[13];

static unsigned short e_xmit_seq; /* transmit sequence number */
static int response_required;

static size_t   nvt_data = 0;
static int	tn3270e_negotiated = 0;
static enum { E_UNBOUND, E_3270, E_NVT, E_SSCP } tn3270e_submode = E_UNBOUND;
static int	tn3270e_bound = 0;
static unsigned char *bind_image = NULL;
static size_t	bind_image_len = 0;
static char	*plu_name = NULL;
static int	maxru_sec = 0;
static int	maxru_pri = 0;
static int	bind_rd = 0;
static int	bind_cd = 0;
static int	bind_ra = 0;
static int	bind_ca = 0;
#define BIND_DIMS_PRESENT	0x1	/* BIND included screen dimensions */
#define BIND_DIMS_ALT		0x2	/* BIND included alternate size */
#define BIND_DIMS_VALID		0x4	/* BIND screen sizes were valid */
static unsigned	bind_state = 0;
static char	**lus = NULL;
static char	**curr_lu = NULL;
static char	*try_lu = NULL;

static int	proxy_type = 0;
static char	*proxy_host = NULL;
static char	*proxy_portname = NULL;
static unsigned short proxy_port = 0;

static b8_t e_funcs;		/* negotiated TN3270E functions */

static bool telnet_fsm(unsigned char c);
static void net_rawout(unsigned const char *buf, size_t len);
static void check_in3270(void);
static void store3270in(unsigned char c);
static void check_linemode(bool init);
static int non_blocking(bool on);
static void net_connected(void);
static void connection_complete(void);
static int tn3270e_negotiate(void);
static int process_eor(void);
static const char *tn3270e_function_names(const unsigned char *, int);
static void tn3270e_subneg_send(unsigned char, b8_t *);
static void tn3270e_fdecode(const unsigned char *, int, b8_t *);
static void tn3270e_ack(void);
static void tn3270e_nak(enum pds);

static const char *cmd(int c);
static const char *opt(unsigned char c);
static const char *nnn(int c);

/* telnet states */
#define TNS_DATA	0	/* receiving data */
#define TNS_IAC		1	/* got an IAC */
#define TNS_WILL	2	/* got an IAC WILL */
#define TNS_WONT	3	/* got an IAC WONT */
#define TNS_DO		4	/* got an IAC DO */
#define TNS_DONT	5	/* got an IAC DONT */
#define TNS_SB		6	/* got an IAC SB */
#define TNS_SB_IAC	7	/* got an IAC after an IAC SB */

/* telnet predefined messages */
static unsigned char	do_opt[]	= { 
	IAC, DO, '_' };
static unsigned char	dont_opt[]	= { 
	IAC, DONT, '_' };
static unsigned char	will_opt[]	= { 
	IAC, WILL, '_' };
static unsigned char	wont_opt[]	= { 
	IAC, WONT, '_' };
static unsigned char	functions_req[] = {
	IAC, SB, TELOPT_TN3270E, TN3270E_OP_FUNCTIONS };

static const char *telquals[3] = { "IS", "SEND", "INFO" };
static const char *telobjs[4] = { "VAR", "VALUE", "ESC", "USERVAR" };
static const char *reason_code[8] = { "CONN-PARTNER", "DEVICE-IN-USE",
	"INV-ASSOCIATE", "INV-NAME", "INV-DEVICE-TYPE", "TYPE-NAME-ERROR",
	"UNKNOWN-ERROR", "UNSUPPORTED-REQ" };
#  define rsn(n)	(((n) <= TN3270E_REASON_UNSUPPORTED_REQ)? \
			reason_code[(n)]: "??")
static const char *function_name[5] = { "BIND-IMAGE", "DATA-STREAM-CTL",
	"RESPONSES", "SCS-CTL-CODES", "SYSREQ" };
# define fnn(n)	(((n) <= TN3270E_FUNC_SYSREQ)? \
			function_name[(n)]: "??")
static const char *data_type[9] = { "3270-DATA", "SCS-DATA", "RESPONSE",
	"BIND-IMAGE", "UNBIND", "NVT-DATA", "REQUEST", "SSCP-LU-DATA",
	"PRINT-EOJ" };
# define e_dt(n)	(((n) <= TN3270E_DT_PRINT_EOJ)? \
			data_type[(n)]: "??")
static const char *req_flag[1] = { " ERR-COND-CLEARED" };
# define e_rq(fn, n) (((fn) == TN3270E_DT_REQUEST)? \
			(((n) <= TN3270E_RQF_ERR_COND_CLEARED)? \
			req_flag[(n)]: " ??"): "")
static const char *hrsp_flag[3] = { "NO-RESPONSE", "ERROR-RESPONSE",
	"ALWAYS-RESPONSE" };
# define e_hrsp(n) (((n) <= TN3270E_RSF_ALWAYS_RESPONSE)? \
			hrsp_flag[(n)]: "??")
static const char *trsp_flag[2] = { "POSITIVE-RESPONSE", "NEGATIVE-RESPONSE" };
# define e_trsp(n) (((n) <= TN3270E_RSF_NEGATIVE_RESPONSE)? \
			trsp_flag[(n)]: "??")
# define e_rsp(fn, n) (((fn) == TN3270E_DT_RESPONSE)? e_trsp(n): e_hrsp(n))

#if !defined(_WIN32) /*[*/
# define XMIT_ROWS	((appres.c3270.altscreen)? MODEL_2_ROWS: maxROWS)
# define XMIT_COLS	((appres.c3270.altscreen)? MODEL_2_COLS: maxCOLS)
#else /*][*/
# define XMIT_ROWS	maxROWS
# define XMIT_COLS	maxCOLS
#endif /*]*/

static int ssl_init(void);

#if defined(HAVE_LIBSSL) /*[*/
bool ssl_supported = true;
bool secure_connection = false;
bool secure_unverified = false;
char **unverified_reasons = NULL;
static int n_unverified_reasons = 0;
SSL_CTX *ssl_ctx;
static SSL *ssl_con;
static bool need_tls_follows = false;
char *ssl_cl_hostname;
bool *ssl_pending;
static bool accept_specified_host;
static char *accept_dnsname;
struct in_addr host_inaddr;
static bool host_inaddr_valid;
# if defined(X3270_IPV6) /*[*/
struct in6_addr host_in6addr;
static bool host_in6addr_valid;
# endif /*]*/
# if OPENSSL_VERSION_NUMBER >= 0x00907000L /*[*/
#  define INFO_CONST const
# else /*][*/
#  define INFO_CONST
# endif /*]*/
static void client_info_callback(INFO_CONST SSL *s, int where, int ret);
static void continue_tls(unsigned char *sbbuf, int len);
static char *spc_verify_cert_hostname(X509 *cert, char *hostname,
	unsigned char *v4addr, unsigned char *v6addr);
#endif /*]*/
static bool refused_tls = false;
static bool any_host_data = false;

#if !defined(_WIN32) /*[*/
static void output_possible(iosrc_t fd, ioid_t id);
#endif /*]*/

#if defined(_WIN32) /*[*/
# define socket_errno()	WSAGetLastError()
# define socket_strerror(n) win32_strerror(n)
# define SE_EWOULDBLOCK	WSAEWOULDBLOCK
# define SE_ECONNRESET	WSAECONNRESET
# define SE_EINTR	WSAEINTR
# define SE_EAGAIN	WSAEINPROGRESS
# define SE_EPIPE	WSAECONNABORTED
# define SE_EINPROGRESS	WSAEINPROGRESS
# define SOCK_IOCTL(s, f, v)	ioctlsocket(s, f, (DWORD *)v)
# define IOCTL_T	u_long
#else /*][*/
# define socket_errno()	errno
# define socket_strerror(n) strerror(n)
# define SE_EWOULDBLOCK	EWOULDBLOCK
# define SE_ECONNRESET	ECONNRESET
# define SE_EINTR	EINTR
# define SE_EAGAIN	EAGAIN
# define SE_EPIPE	EPIPE
# if defined(EINPROGRESS) /*[*/
#  define SE_EINPROGRESS	EINPROGRESS
# endif /*]*/
# define SOCK_IOCTL	ioctl
# define IOCTL_T	int
#endif /*]*/

#if defined(SE_EINPROGRESS) /*[*/
# define IS_EINPROGRESS(e)	((e) == SE_EINPROGRESS)
#else /*][*/
# define IS_EINPROGRESS(e)	false
#endif /*]*/


typedef union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if defined(X3270_IPV6) /*[*/
	struct sockaddr_in6 sin6;
#endif /*]*/
} sockaddr_46_t;

#define NUM_HA	4
static sockaddr_46_t haddr[NUM_HA];
static socklen_t ha_len[NUM_HA] = {
    sizeof(haddr[0]), sizeof(haddr[0]), sizeof(haddr[0]), sizeof(haddr[0])
};
#if defined(HAVE_LIBSSL) /*[*/
static bool hin[NUM_HA];
#endif /*]*/
static int num_ha = 0;
static int ha_ix = 0;

#if defined(_WIN32) /*[*/
void
popup_a_sockerr(const char *fmt, ...)
{
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = vlazyaf(fmt, args);
    va_end(args);
    popup_an_error("%s: %s", buffer, win32_strerror(socket_errno()));
}
#else /*][*/
void
popup_a_sockerr(const char *fmt, ...)
{
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = vlazyaf(fmt, args);
    va_end(args);
    popup_an_errno(errno, "%s", buffer);
}
#endif /*]*/

/* The host connection timed out. */
static void
connect_timed_out(ioid_t id _is_unused)
{
    popup_an_error("Host connection timed out");
    connect_timeout_id = NULL_IOID;
    host_disconnect(true);
}

/* Connect to one of the addresses in haddr[]. */
static iosrc_t
connect_to(int ix, bool noisy, bool *pending)
{
    int			on = 1;
    char		hn[256];
    char		pn[256];
    char		*errmsg;
#if defined(OMTU) /*[*/
    int			mtu = OMTU;
#endif /*]*/
#   define close_fail	{ (void) SOCK_CLOSE(sock); \
    			  sock = INVALID_SOCKET; \
    			  return INVALID_IOSRC; \
			}

#if defined(HAVE_LIBSSL) /*[*/
    /* Set host_inaddr and host_in6addr for IP address validation. */
    if (!accept_specified_host && hin[ix]) {
	if (haddr[ix].sa.sa_family == AF_INET) {
	    memcpy(&host_inaddr, &haddr[ix].sin.sin_addr,
		    sizeof(struct in_addr));
	    host_inaddr_valid = true;
# if defined(X3270_IPV6) /*[*/
	    host_in6addr_valid = false;
# endif /*]*/
	}
#if defined(X3270_IPV6) /*[*/
	if (haddr[ix].sa.sa_family == AF_INET6) {
	    memcpy(&host_in6addr, &haddr[ix].sin6.sin6_addr,
		    sizeof(struct in6_addr));
	    host_in6addr_valid = true;
	    host_inaddr_valid = false;
	}
#endif /*]*/
    }
#endif /*]*/

    /* create the socket */
    if ((sock = socket(haddr[ix].sa.sa_family, SOCK_STREAM, 0)) ==
	    INVALID_SOCKET) {
	popup_a_sockerr("socket");
	return INVALID_IOSRC;
    }

    /* set options for inline out-of-band data and keepalives */
    if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *)&on,
		sizeof(on)) < 0) {
	popup_a_sockerr("setsockopt(SO_OOBINLINE)");
	close_fail;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
		sizeof(on)) < 0) {
	popup_a_sockerr("setsockopt(SO_KEEPALIVE)");
	close_fail;
    }
#if defined(OMTU) /*[*/
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&mtu,
		sizeof(mtu)) < 0) {
	popup_a_sockerr("setsockopt(SO_SNDBUF)");
	close_fail;
    }
#endif /*]*/

	/* set the socket to be non-delaying */
	if (non_blocking(true) < 0) {
		close_fail;
	}

#if !defined(_WIN32) /*[*/
    /* don't share the socket with our children */
    (void) fcntl(sock, F_SETFD, 1);
#endif /*]*/

    /* init ssl */
    if (HOST_FLAG(SSL_HOST)) {
	if (ssl_init() < 0) {
	    close_fail;
	}
    }

    if (numeric_host_and_port(&haddr[ix].sa, ha_len[ix], hn, sizeof(hn), pn,
		sizeof(pn), &errmsg)) {
	vtrace("Trying %s, port %s...\n", hn, pn);
	telnet_gui_connecting(hn, pn);
    }

    /* Set an explicit timeout, if configured. */
    if (appres.connect_timeout) {
	connect_timeout_id = AddTimeOut(appres.connect_timeout * 1000,
		connect_timed_out);
    }

    /* connect */
    if (connect(sock, &haddr[ix].sa, ha_len[ix]) == -1) {
	if (socket_errno() == SE_EWOULDBLOCK ||
		IS_EINPROGRESS(socket_errno())) {
	    vtrace("TCP connection pending.\n");
	    *pending = true;
#if !defined(_WIN32) /*[*/
	    output_id = AddOutput(sock, output_possible);
#endif /*]*/
	} else {
	    if (noisy) {
		popup_a_sockerr("Connect to %s, port %d", hostname,
			current_port);
	    }
	    close_fail;
	}
    } else {
	if (non_blocking(false) < 0) {
	    close_fail;
	}
	net_connected();

	/* net_connected() can cause the connection to fail. */
	if (sock == INVALID_SOCKET) {
	    close_fail;
	}
    }

    /* all done */
#if defined(_WIN32) /*[*/
    sock_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (sock_handle == NULL) {
	fprintf(stderr, "Cannot create socket handle: %s\n",
		win32_strerror(GetLastError()));
	x3270_exit(1);
    }
    if (WSAEventSelect(sock, sock_handle, FD_READ | FD_CONNECT | FD_CLOSE)
	    != 0) {
	fprintf(stderr, "WSAEventSelect failed: %s\n",
		win32_strerror(GetLastError()));
	x3270_exit(1);
    }

    return sock_handle;
#else /*][*/
    return sock;
#endif /*]*/
}

#if defined(HAVE_LIBSSL) /*[*/
static bool
is_numeric_host(const char *host)
{
	/* Is it an IPv4 address? */
	if (inet_addr(host) != (INET_ADDR_T)-1)
		return true;

# if defined(X3270_IPV6) /*[*/
	/*
	 * Is it an IPv6 address?
	 *
	 * The test here is imperfect, but a DNS name can't contain a colon,
	 * so if the name contains one, and getaddrinfo() succeeds, we can
	 * assume it is a numeric IPv6 address.  We add an extra level of
	 * insurance, that it only contains characters that are valid in an
	 * IPv6 numeric address (hex digits, colons and periods).
	 */
	if (strchr(host, ':') &&
	    strspn(host, ":.0123456789abcdefABCDEF") == strlen(host))
		return true;
# endif /*]*/

	return false;
}
#endif /*]*/

/*
 * net_connect
 *	Establish a telnet socket to the given host passed as an argument.
 *	Called only once and is responsible for setting up the telnet
 *	variables.  Returns the file descriptor of the connected socket.
 */
iosrc_t
net_connect(const char *host, char *portname, bool ls, bool *resolving,
    bool *pending)
{
    struct servent       *sp;
    struct hostent       *hp;
    char	       	passthru_haddr[8];
    int			passthru_len = 0;
    unsigned short	passthru_port = 0;
    char		*errmsg;
    iosrc_t		s;
#if defined(HAVE_LIBSSL) /*[*/
    bool		inh;
#endif /*]*/

    if (netrbuf == NULL) {
	netrbuf = (unsigned char *)Malloc(BUFSZ);
    }

    linemode_init();

    *resolving = false;
    *pending = false;

    Replace(hostname, NewString(host));
#if defined(HAVE_LIBSSL) /*[*/
    if (!accept_specified_host) {
	host_inaddr_valid = false;
# if defined(X3270_IPV6) /*[*/
	host_in6addr_valid = false;
# endif /*]*/
	inh = is_numeric_host(host);
    } else {
	inh = false;
    }
#endif /*]*/

    /* set up temporary termtype */
    if (appres.termname == NULL) {
	if (appres.nvt_mode) {
	    termtype = "xterm";
	} else if (ov_rows || ov_cols) {
	    termtype = "IBM-DYNAMIC";
	} else if (HOST_FLAG(STD_DS_HOST)) {
	    (void) snprintf(ttype_tmpval, sizeof(ttype_tmpval), "IBM-327%c-%d",
		    appres.m3279? '9': '8', model_num);
	    termtype = ttype_tmpval;
	} else {
	    termtype = full_model_name;
	}
    }

    /* get the passthru host and port number */
    if (HOST_FLAG(PASSTHRU_HOST)) {
	const char *hn;

	hn = getenv("INTERNET_HOST");
	if (hn == NULL) {
	    hn = "internet-gateway";
	}

	hp = gethostbyname(hn);
	if (hp == (struct hostent *) 0) {
	    popup_an_error("Unknown passthru host: %s", hn);
	    return INVALID_IOSRC;
	}
	(void) memmove(passthru_haddr, hp->h_addr, hp->h_length);
	passthru_len = hp->h_length;

	sp = getservbyname("telnet-passthru","tcp");
	if (sp != NULL) {
	    passthru_port = sp->s_port;
	} else {
	    passthru_port = htons(3514);
	}
    } else if (appres.proxy != NULL && !proxy_type) {
	proxy_type = proxy_setup(appres.proxy, &proxy_host, &proxy_portname);
	if (proxy_type > 0) {
	    unsigned long lport;
	    char *ptr;
	    struct servent *sp;

	    lport = strtoul(portname, &ptr, 0);
	    if (ptr == portname || *ptr != '\0' || lport == 0L ||
		    lport & ~0xffff) {
		if (!(sp = getservbyname(portname, "tcp"))) {
		    popup_an_error("Unknown port number or service: %s",
			    portname);
		    return INVALID_IOSRC;
		}
		current_port = ntohs(sp->s_port);
	    } else {
		current_port = (unsigned short)lport;
	    }
	}
	if (proxy_type < 0) {
	    return INVALID_IOSRC;
	}
    }

    /* fill in the socket address of the given host */
    (void) memset((char *) &haddr, 0, sizeof(haddr));
    if (HOST_FLAG(PASSTHRU_HOST)) {
	/*
	 * XXX: We don't try multiple addresses for the passthru
	 * host.
	 */
	haddr[0].sin.sin_family = AF_INET;
	(void) memmove(&haddr[0].sin.sin_addr, passthru_haddr, passthru_len);
	haddr[0].sin.sin_port = passthru_port;
	ha_len[0] = sizeof(struct sockaddr_in);
#if defined(HAVE_LIBSSL) /*[*/
	hin[0] = false;
#endif /*]*/
	num_ha = 1;
	ha_ix = 0;
    } else if (proxy_type > 0) {
	/*
	 * XXX: We don't try multiple addresses for a proxy
	 * host.
	 */
	rhp_t rv;

	rv = resolve_host_and_port(proxy_host, proxy_portname, 0, &proxy_port,
		&haddr[0].sa, &ha_len[0], &errmsg, NULL);
	if (RHP_IS_ERROR(rv)) {
	    popup_an_error("%s", errmsg);
	    return INVALID_IOSRC;
	}
#if defined(HAVE_LIBSSL) /*[*/
	hin[0] = false;
#endif /*]*/
	num_ha = 1;
	ha_ix = 0;
    } else {
#if defined(LOCAL_PROCESS) /*[*/
	if (ls) {
	    local_process = true;
	} else {
#endif /*]*/
	    int i;
	    int last = false;
	    rhp_t rv;

#if defined(LOCAL_PROCESS) /*[*/
	    local_process = false;
#endif /*]*/
	    num_ha = 0;
	    for (i = 0; i < NUM_HA && !last; i++) {
		rv = resolve_host_and_port(host, portname, i, &current_port,
			&haddr[i].sa, &ha_len[i], &errmsg, &last);
		if (RHP_IS_ERROR(rv)) {
		    popup_an_error("%s", errmsg);
		    return INVALID_IOSRC;
		}
#if defined(HAVE_LIBSSL) /*[*/
		hin[i] = inh;
#endif /*]*/
		num_ha++;
	    }
	    ha_ix = 0;
#if defined(LOCAL_PROCESS) /*[*/
	}
#endif /*]*/
    }

#if defined(LOCAL_PROCESS) /*[*/
    if (local_process) {
	int amaster;
	struct winsize w;

	w.ws_row = XMIT_ROWS;
	w.ws_col = XMIT_COLS;
	w.ws_xpixel = 0;
	w.ws_ypixel = 0;

	switch (forkpty(&amaster, NULL, NULL, &w)) {
	case -1:	/* failed */
	    popup_an_errno(errno, "forkpty");
	    close_fail;
	case 0:	/* child */
	    putenv("TERM=xterm");
	    if (strchr(host, ' ') != NULL) {
		(void) execlp("/bin/sh", "sh", "-c", host, NULL);
	    } else {
		char *arg1;

		arg1 = strrchr(host, '/');
		(void) execlp(host, (arg1 == NULL)? host: arg1 + 1, NULL);
	    }
	    perror(host);
	    _exit(1);
	    break;
	default:	/* parent */
	    sock = amaster;
	    (void) fcntl(sock, F_SETFD, 1);
	    connection_complete();
	    host_in3270(CONNECTED_NVT);
	    break;
	}
	return sock;
    }
#endif /*]*/

    /* Try each of the haddrs. */
    while (ha_ix < num_ha) {
	if ((s = connect_to(ha_ix, (ha_ix == num_ha - 1),
			pending)) != INVALID_IOSRC) {
	    return s;
	}
	ha_ix++;
    }

    /* Ran out. */
    return INVALID_IOSRC;
}
#undef close_fail

/* Set up the LU list. */
static void
setup_lus(void)
{
    char *lu;
    char *comma;
    int n_lus = 1;
    int i;

    connected_lu = NULL;
    connected_type = NULL;

    if (!luname[0]) {
	Replace(lus, NULL);
	curr_lu = NULL;
	try_lu = NULL;
	return;
    }

    /*
     * Count the commas in the LU name.  That plus one is the
     * number of LUs to try. 
     */
    lu = luname;
    while ((comma = strchr(lu, ',')) != NULL) {
	n_lus++;
	lu++;
    }

    /*
     * Allocate enough memory to construct an argv[] array for
     * the LUs.
     */
    Replace(lus,
	    (char **)Malloc((n_lus+1) * sizeof(char *) + strlen(luname) + 1));

    /* Copy each LU into the array. */
    lu = (char *)(lus + n_lus + 1);
    (void) strcpy(lu, luname);
    i = 0;
    do {
	lus[i++] = lu;
	comma = strchr(lu, ',');
	if (comma != NULL) {
	    *comma = '\0';
	    lu = comma + 1;
	}
    } while (comma != NULL);
    lus[i] = NULL;
    curr_lu = lus;
    try_lu = *curr_lu;
}

#if defined(HAVE_LIBSSL) /*[*/
/*
 * Add a reason that the host certificate is unverified.
 */
static void
add_unverified_reason(const char *reason)
{
    char **s;
    int i;

    s = unverified_reasons;
    unverified_reasons = (char **)Malloc(
	   (n_unverified_reasons + 2) * sizeof(char *));
    for (i = 0; i < n_unverified_reasons; i++) {
	unverified_reasons[i] = s[i];
    }
    unverified_reasons[n_unverified_reasons++] = NewString(reason);
    unverified_reasons[n_unverified_reasons] = NULL;
    Free(s);
}

/*
 * Clear the list of reasons the host certificate is unverified.
 */
static void
free_unverified_reasons(void)
{
    if (unverified_reasons != NULL) {
	int i;

	for (i = 0; unverified_reasons[i]; i++) {
	    Free(unverified_reasons[i]);
	}
	Replace(unverified_reasons, NULL);
    }
    n_unverified_reasons = 0;
}

/*
 * Check the name in the host certificate.
 *
 * Returns true if the certificate is okay (or doesn't need to be), false if
 * the connection should fail because of a bad certificate.
 */
static bool
check_cert_name(void)
{
    X509 *cert;
    char *unmatched_names;

    cert = SSL_get_peer_certificate(ssl_con);
    if (cert == NULL) {
	if (appres.ssl.verify_host_cert) {
	    popup_an_error("No host certificate");
	    return false;
	} else {
	    secure_unverified = true;
	    vtrace("No host certificate.\n");
	    add_unverified_reason("No host certificate");
	    return true;
	}
    }

    unmatched_names = spc_verify_cert_hostname(cert,
	    accept_specified_host? accept_dnsname: hostname,
	    host_inaddr_valid? (unsigned char *)(void *)&host_inaddr: NULL,
#if defined(X3270_IPV6) /*[*/
	    host_in6addr_valid? (unsigned char *)(void *)&host_in6addr: NULL
#else /*][*/
	    NULL
#endif /*]*/
	    );
    if (unmatched_names != NULL) {
	X509_free(cert);
	if (appres.ssl.verify_host_cert) {
	    popup_an_error("Host certificate name(s) do not match '%s':\n%s",
		    hostname, unmatched_names);
	    return false;
	} else {
	    char *reason;

	    secure_unverified = true;
	    vtrace("Host certificate name(s) do not match hostname.\n");
	    reason = xs_buffer("Host certificate name(s) do not match '%s': "
		    "%s", hostname, unmatched_names);
	    add_unverified_reason(reason);
	    Free(reason);
	    return true;
	}
	Free(unmatched_names);
    }
    X509_free(cert);
    return true;
}
#endif /*]*/

static void
net_connected(void)
{
    /* Cancel the timeout. */
    if (connect_timeout_id != NULL_IOID) {
	RemoveTimeOut(connect_timeout_id);
	connect_timeout_id = NULL_IOID;
    }

    /*
     * If the connection went through on the first connect() call, then
     * our state is NOT_CONNECTED, so host_disconnect() will not call back
     * net_disconnect(). That would be bad. So set the state to something
     * non-zero.
     */
    cstate = NEGOTIATING;

    if (proxy_type > 0) {

	/* Negotiate with the proxy. */
	vtrace("Connected to proxy server %s, port %u.\n", proxy_host,
		proxy_port);

	if (!proxy_negotiate(proxy_type, sock, hostname, current_port)) {
	    host_disconnect(true);
	    return;
	}
    }

    vtrace("Connected to %s, port %u%s.\n", hostname, current_port,
	    HOST_FLAG(SSL_HOST)? " via SSL": "");

#if defined(HAVE_LIBSSL) /*[*/
    /* Set up SSL. */
    if (HOST_FLAG(SSL_HOST) && !secure_connection) {
	int rv;

	if (SSL_set_fd(ssl_con, (int)sock) != 1) {
	    vtrace("Can't set fd!\n");
	}
#if defined(_WIN32) /*[*/
	/* Make the socket blocking for SSL_connect. */
	(void) WSAEventSelect(sock, sock_handle, 0);
	(void) non_blocking(false);
#endif /*]*/
	rv = SSL_connect(ssl_con);
#if defined(_WIN32) /*[*/
	/* Make the socket non-blocking again for event processing. */
	(void) WSAEventSelect(sock, sock_handle, FD_READ | FD_CONNECT | FD_CLOSE);
#endif /*]*/
	if (rv != 1) {
	    long v;

	    v = SSL_get_verify_result(ssl_con);
	    if (v != X509_V_OK) {
		popup_an_error("Host certificate verification failed:\n"
			"%s (%ld)%s", X509_verify_cert_error_string(v), v,
			(v == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)?
	       "\nCA certificate needs to be added to the local database": "");
	    }

	    /* No need to trace the error, it was already displayed. */
	    host_disconnect(true);
	    return;
	}

	/* Check the host certificate. */
	if (!check_cert_name()) {
	    host_disconnect(true);
	    return;
	}

	secure_connection = true;

	vtrace("TLS/SSL tunneled connection complete. Connection is now "
		"secure.\n");

	/* Tell everyone else again. */
	host_connected();
    }
#endif /*]*/

    /* Done with SSL or proxy. */
    if (appres.nvt_mode) {
	host_in3270(CONNECTED_NVT);
    } else {
	cstate = CONNECTED_INITIAL;
    }

    /* set up telnet options */
    memset((char *)myopts, 0, sizeof(myopts));
    memset((char *)hisopts, 0, sizeof(hisopts));
    did_ne_send = false;
    deferred_will_ttype = false;
    b8_zero(&e_funcs);
    b8_set_bit(&e_funcs, TN3270E_FUNC_BIND_IMAGE);
    b8_set_bit(&e_funcs, TN3270E_FUNC_RESPONSES);
    b8_set_bit(&e_funcs, TN3270E_FUNC_SYSREQ);
    e_xmit_seq = 0;
    response_required = TN3270E_RSF_NO_RESPONSE;
#if defined(HAVE_LIBSSL) /*[*/
    need_tls_follows = false;
#endif /*]*/
    telnet_state = TNS_DATA;
    ibptr = ibuf;

    /* clear statistics and flags */
    (void) time(&ns_time);
    ns_brcvd = 0;
    ns_rrcvd = 0;
    ns_bsent = 0;
    ns_rsent = 0;
    syncing = 0;
    tn3270e_negotiated = 0;
    tn3270e_submode = E_UNBOUND;
    tn3270e_bound = 0;

    setup_lus();

    check_linemode(true);

    /* write out the passthru hostname and port nubmer */
    if (HOST_FLAG(PASSTHRU_HOST)) {
	char *buf;

	buf = xs_buffer("%s %d\r\n", hostname, current_port);
	(void) send(sock, buf, (int)strlen(buf), 0);
	Free(buf);
    }
}

/*
 * remove_output
 * 	Cancel the callback for output available.
 */
static void
remove_output(void)
{
#if !defined(_WIN32) /*[*/
    if (output_id != NULL_IOID) {
	RemoveInput(output_id);
	output_id = NULL_IOID;
    }
#endif /*]*/
}

/*
 * connection_complete
 *	The connection appears to be complete (output is possible or input
 *	appeared ready but recv() returned EWOULDBLOCK).  Complete the
 *	connection-completion processing.
 */
static void
connection_complete(void)
{
#if !defined(_WIN32) /*[*/
    if (non_blocking(false) < 0) {
	host_disconnect(true);
	return;
    }
#endif /*]*/
    host_connected();
    net_connected();
    remove_output();
}

#if !defined(_WIN32) /*[*/
/*
 * output_possible
 *	Output is possible on the socket.  Used only when a connection is
 *	pending, to determine that the connection is complete.
 */
static void
output_possible(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
#if defined(CONNECT_GETPEERNAME) /*[*/
    sockaddr_46_t sa;
    socklen_t len = sizeof(sa);
# define COMPLETE_CONNECT(s)	getpeername(s, &sa.sa, &len)
# else /*][*/
# define COMPLETE_CONNECT(s)	connect(s, &haddr[ha_ix].sa, sizeof(haddr[0]))
#endif /*]*/

    vtrace("Output possible\n");

    /*
     * Try a connect() again to see if the connection completed sucessfully.
     * On some systems, such as Linux, this is harmless and succeeds.
     * On others, such as MacOS, this is mostly harmless and fails
     * with EISCONN.
     *
     * On Solaris, we do a getpeername() instead of a connect(). The second
     * connect() would fail with EINVAL there.
     */
    if (COMPLETE_CONNECT(sock) < 0) {
	if (errno != EISCONN) {
	    vtrace("RCVD socket error %d (%s)\n", socket_errno(),
		    strerror(errno));
	    popup_a_sockerr("Connection failed");
	    host_disconnect(true);
	    return;
	}
    }

    if (HALF_CONNECTED) {
	connection_complete();
    }
    remove_output();
}
#endif /*]*/


/*
 * net_disconnect
 *	Shut down the socket.
 */
void
net_disconnect(void)
{
#if defined(HAVE_LIBSSL) /*[*/
    if (ssl_con != NULL) {
	SSL_shutdown(ssl_con);
	SSL_free(ssl_con);
	ssl_con = NULL;
    }
    secure_connection = false;
    secure_unverified = false;
    free_unverified_reasons();
#endif /*]*/
    if (CONNECTED) {
	(void) shutdown(sock, 2);
    }
    (void) SOCK_CLOSE(sock);
    sock = INVALID_SOCKET;
#if defined(_WIN32) /*[*/
    CloseHandle(sock_handle);
    sock_handle = INVALID_HANDLE_VALUE;
#endif /*]*/
    vtrace("SENT disconnect\n");

    /* Cancel the timeout. */
    if (connect_timeout_id != NULL_IOID) {
	RemoveTimeOut(connect_timeout_id);
	connect_timeout_id = NULL_IOID;
    }

    /* We're not connected to an LU any more. */
    status_lu(NULL);

    /* We have no more interest in output buffer space. */
    remove_output();

    /* If we refused TLS and never entered 3270 mode, say so. */
    if (refused_tls && !any_host_data) {
#if defined(HAVE_LIBSSL) /*[*/
	if (!appres.ssl.tls) {
	    popup_an_error("Connection failed:\n"
		    "Host requested TLS but SSL disabled");
	} else {
	    popup_an_error("Connection failed:\n"
		    "Host requested TLS but SSL DLLs not found");
	}
#else /*][*/
	popup_an_error("Connection failed:\n"
		"Host requested TLS but SSL not supported");
#endif /*]*/
    }
    refused_tls = false;
    any_host_data = false;

    net_set_default_termtype();
}


/*
 * net_input
 *	Called by the toolkit whenever there is input available on the
 *	socket.  Reads the data, processes the special telnet commands
 *	and calls process_ds to process the 3270 data stream.
 */
void
net_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
	register unsigned char	*cp;
	int	nr;
#if defined(HAVE_LIBSSL) /*[*/
	bool	ignore_ssl = false;
#endif /*]*/

#if defined(_WIN32) /*[*/
	/*
	 * Make the socket non-blocking.
	 * Note that WSAEventSelect does this automatically (and won't allow
	 * us to change it back to blocking), except on Wine.
	 */
	if (sock != INVALID_SOCKET && non_blocking(true) < 0) {
		    host_disconnect(true);
		    return;
	}
	for (;;) {
#endif /*]*/
	if (sock < 0)
		return;

#if defined(_WIN32) /*[*/
	if (HALF_CONNECTED) {
		if (connect(sock, &haddr[ha_ix].sa, sizeof(haddr[0])) < 0) {
			int err = GetLastError();

			switch (err) {
			case WSAEISCONN:
				connection_complete();
				/* and go get data...? */
				break;
			case WSAEALREADY:
			case WSAEWOULDBLOCK:
			case WSAEINVAL:
				return;
			default:
				fprintf(stderr,
				    "second connect() failed: %s\n",
				    win32_strerror(err));
				x3270_exit(1);
			}
		}
	}
#endif /*]*/

	nvt_data = 0;

	vtrace("Reading host socket\n");

#if defined(HAVE_LIBSSL) /*[*/
	if (ssl_con != NULL) {
		/*
		 * OpenSSL does not like getting refused connections
		 * when it hasn't done any I/O yet.  So peek ahead to
		 * see if it's worth getting it involved at all.
		 */
		if (HALF_CONNECTED &&
		    (nr = recv(sock, (char *) netrbuf, 1,
			       MSG_PEEK)) <= 0)
			ignore_ssl = true;
		else
			nr = SSL_read(ssl_con, (char *) netrbuf, BUFSZ);
	} else
#else /*][*/
#endif /*]*/
#if defined(LOCAL_PROCESS) /*[*/
	if (local_process)
		nr = read(sock, (char *) netrbuf, BUFSZ);
	else
#endif /*]*/
		nr = recv(sock, (char *) netrbuf, BUFSZ, 0);
	vtrace("Host socket read complete nr=%d\n", nr);
	if (nr < 0) {
		if (socket_errno() == SE_EWOULDBLOCK) {
			vtrace("EWOULDBLOCK\n");
			return;
		}
#if defined(HAVE_LIBSSL) /*[*/
		if (ssl_con != NULL && !ignore_ssl) {
			unsigned long e;
			char err_buf[120];

			e = ERR_get_error();
			if (e != 0)
				(void) ERR_error_string(e, err_buf);
			else
				strcpy(err_buf, "unknown error");
			vtrace("RCVD SSL_read error %ld (%s)\n", e,
			    err_buf);
			popup_an_error("SSL_read:\n%s", err_buf);
			host_disconnect(true);
			return;
		}
#endif /*]*/
		if (HALF_CONNECTED && socket_errno() == SE_EAGAIN) {
			connection_complete();
			return;
		}
#if defined(LOCAL_PROCESS) /*[*/
		if (errno == EIO && local_process) {
			vtrace("RCVD local process disconnect\n");
			host_disconnect(false);
			return;
		}
#endif /*]*/
		vtrace("RCVD socket error %d (%s)\n",
			socket_errno(), socket_strerror(socket_errno()));
		if (HALF_CONNECTED) {
			if (ha_ix == num_ha - 1) {
				popup_a_sockerr("Connect to %s, "
				    "port %d", hostname, current_port);
			} else {
				bool dummy;
				iosrc_t s;

				net_disconnect();
				if (HOST_FLAG(SSL_HOST)) {
					if (ssl_init() < 0) {
						host_disconnect(true);
						return;
					}
				}
				while (++ha_ix < num_ha) {
					s = connect_to(ha_ix,
						(ha_ix == num_ha - 1),
						&dummy);
					if (s != INVALID_IOSRC) {
						host_newfd(s);
						return;
					}
				}
			}
		} else if (socket_errno() != SE_ECONNRESET) {
			popup_a_sockerr("Socket read");
		}
		host_disconnect(true);
		return;
	} else if (nr == 0) {
		/* Host disconnected. */
		vtrace("RCVD disconnect\n");
		host_disconnect(false);
		return;
	}

	/* Process the data. */

	if (HALF_CONNECTED) {
		if (non_blocking(false) < 0) {
			host_disconnect(true);
			return;
		}
		host_connected();
		net_connected();
		remove_output();
	}

	trace_netdata('<', netrbuf, nr);

	ns_brcvd += nr;
	for (cp = netrbuf; cp < (netrbuf + nr); cp++) {
#if defined(LOCAL_PROCESS) /*[*/
		if (local_process) {
			/* More to do here, probably. */
			if (IN_NEITHER) {	/* now can assume NVT mode */
				host_in3270(CONNECTED_NVT);
				hisopts[TELOPT_ECHO] = 1;
				check_linemode(false);
				kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
				status_reset();
				ps_process();
			}
			nvt_process((unsigned int) *cp);
		} else {
#endif /*]*/
			if (!telnet_fsm(*cp)) {
				(void) ctlr_dbcs_postprocess();
				host_disconnect(true);
				return;
			}
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/
	}

	if (IN_NVT) {
		(void) ctlr_dbcs_postprocess();
	}
	if (nvt_data) {
		vtrace("\n");
		nvt_data = 0;
	}

	/* See if it's time to roll over the trace file. */
	trace_rollover_check();

#if defined(_WIN32) /*[*/
	}
#endif /*]*/
}


/*
 * set16
 *	Put a 16-bit value in a buffer.
 *	Returns the number of bytes required.
 */
static size_t
set16(char *buf, int n)
{
	char *b0 = buf;

	n %= 256 * 256;
	if ((n / 256) == IAC)
		*(unsigned char *)buf++ = IAC;
	*buf++ = (n / 256);
	n %= 256;
	if (n == IAC)
		*(unsigned char *)buf++ = IAC;
	*buf++ = n;
	return buf - b0;
}

/*
 * send_naws
 *	Send a Telnet window size sub-option negotation.
 */
static void
send_naws(void)
{
	char naws_msg[14];
	size_t naws_len = 0;

	(void) snprintf(naws_msg, sizeof(naws_msg), "%c%c%c",
		IAC, SB, TELOPT_NAWS);
	naws_len += 3;
	naws_len += set16(naws_msg + naws_len, XMIT_COLS);
	naws_len += set16(naws_msg + naws_len, XMIT_ROWS);
	(void) sprintf(naws_msg + naws_len, "%c%c", IAC, SE);
	naws_len += 2;
	net_rawout((unsigned char *)naws_msg, naws_len);
	vtrace("SENT %s NAWS %d %d %s\n", cmd(SB), XMIT_COLS,
	    XMIT_ROWS, cmd(SE));
}



/* Advance 'try_lu' to the next desired LU name. */
static void
next_lu(void)
{
	if (curr_lu != NULL && (try_lu = *++curr_lu) == NULL)
		curr_lu = NULL;
}

#if defined(EBCDIC_HOST) /*[*/
/*
 * force_ascii
 * 	Force the argument string to ASCII.  On ASCII (or ASCII-derived) hosts,
 * 	this is a no-op.  On EBCDIC-based hosts, translation is necessary.
 */
static const char *
force_ascii(const char *s)
{
    	static char buf[256];
	unsigned char c, e;
	int i;

	i = 0;
	while ((c = *s++) && i < sizeof(buf) - 1) {
		e = ebc2asc0[c];
		if (e)
			buf[i++] = e;
		else
			buf[i++] = 0x3f; /* '?' */
	}
	buf[i] = '\0';
	return buf;
}
#else /*][*/
#define force_ascii(s) (s)
#endif /*]*/

#if defined(EBCDIC_HOST) /*[*/
/*
 * force_local
 * 	Force the argument string from ASCII to the local character set.  On
 * 	ASCII (or ASCII-derived) hosts, this is a no-op.  On EBCDIC-based
 * 	hosts, translation is necessary.
 *
 * 	Does the translation in-place.
 */
void
force_local(char *s)
{
    unsigned char c, e;

    while ((c = *s) != '\0') {
	e = asc2ebc0[c];
	if (e) {
	    *s = e;
	} else {
	    *s = '?';
	}
	s++;
    }
}
#else /*][*/
#define force_local(s)
#endif /*]*/

/*
 * telnet_fsm
 *	Telnet finite-state machine.
 *	Returns true for okay, false for errors.
 */
static bool
telnet_fsm(unsigned char c)
{
    char *see_chr;
    size_t sl;

    switch (telnet_state) {
    case TNS_DATA:	/* normal data processing */
	if (c == IAC) {	/* got a telnet command */
	    telnet_state = TNS_IAC;
	    if (nvt_data) {
		vtrace("\n");
		nvt_data = 0;
	    }
	    break;
	}
	if (IN_NEITHER) {	/* now can assume NVT mode */
	    if (linemode) {
		linemode_buf_init();
	    }
	    host_in3270(CONNECTED_NVT);
	    kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
	    status_reset();
	    ps_process();
	}
	if (IN_NVT && !IN_E) {
	    if (!nvt_data) {
		vtrace("<.. ");
		nvt_data = 4;
	    }
	    see_chr = ctl_see((int) c);
	    nvt_data += (sl = strlen(see_chr));
	    if (nvt_data >= TRACELINE) {
		vtrace(" ...\n... ");
		nvt_data = 4 + sl;
	    }
	    vtrace("%s", see_chr);
	    if (!syncing) {
		if (linemode && appres.linemode.onlcr && c == '\n') {
		    nvt_process((unsigned int) '\r');
		}
		nvt_process((unsigned int) c);
		sms_store(c);
	    }
	} else {
	    store3270in(c);
	}
	break;
    case TNS_IAC:	/* process a telnet command */
	if (c != EOR && c != IAC) {
	    vtrace("RCVD %s ", cmd(c));
	}
	switch (c) {
	case IAC:	/* escaped IAC, insert it */
	    if (IN_NVT && !IN_E) {
		if (!nvt_data) {
		    vtrace("<.. ");
		    nvt_data = 4;
		}
		see_chr = ctl_see((int) c);
		nvt_data += (sl = strlen(see_chr));
		if (nvt_data >= TRACELINE) {
		    vtrace(" ...\n ...");
		    nvt_data = 4 + sl;
		}
		vtrace("%s", see_chr);
		nvt_process((unsigned int) c);
		sms_store(c);
	    } else {
		store3270in(c);
	    }
	    telnet_state = TNS_DATA;
	    break;
	case EOR:	/* eor, process accumulated input */
	    if (IN_3270 || (IN_E && tn3270e_negotiated)) {
		ns_rrcvd++;
		if (process_eor()) {
		    return false;
		}
	    } else {
		Warning("EOR received when not in 3270 mode, ignored.");
	    }
	    vtrace("RCVD EOR\n");
	    ibptr = ibuf;
	    telnet_state = TNS_DATA;
	    break;
	case WILL:
	    telnet_state = TNS_WILL;
	    break;
	case WONT:
	    telnet_state = TNS_WONT;
	    break;
	case DO:
	    telnet_state = TNS_DO;
	    break;
	case DONT:
	    telnet_state = TNS_DONT;
	    break;
	case SB:
	    telnet_state = TNS_SB;
	    if (sbbuf == NULL) {
		sbbuf = (unsigned char *)Malloc(1024);
	    }
	    sbptr = sbbuf;
	    break;
	case DM:
	    vtrace("\n");
	    if (syncing) {
		syncing = 0;
#if !defined(_WIN32) /*[*/
		x_except_on(sock);
#else /*][*/
		x_except_on(sock_handle);
#endif /*]*/
	    }
	    telnet_state = TNS_DATA;
	    break;
	case GA:
	case NOP:
	    vtrace("\n");
	    telnet_state = TNS_DATA;
	    break;
	default:
	    vtrace("???\n");
	    telnet_state = TNS_DATA;
	    break;
	}
	break;
    case TNS_WILL:	/* telnet WILL DO OPTION command */
	vtrace("%s\n", opt(c));
	switch (c) {
	case TELOPT_SGA:
	case TELOPT_BINARY:
	case TELOPT_EOR:
	case TELOPT_TTYPE:
	case TELOPT_ECHO:
	case TELOPT_TN3270E:
	    if (c != TELOPT_TN3270E || !HOST_FLAG(NON_TN3270E_HOST)) {
		if (!hisopts[c]) {
		    hisopts[c] = 1;
		    do_opt[2] = c;
		    net_rawout(do_opt, sizeof(do_opt));
		    vtrace("SENT %s %s\n", cmd(DO), opt(c));

		    /* For UTS, volunteer to do EOR when they do. */
		    if (c == TELOPT_EOR && !myopts[c]) {
			myopts[c] = 1;
			will_opt[2] = c;
			net_rawout(will_opt, sizeof(will_opt));
			vtrace("SENT %s %s\n", cmd(WILL), opt(c));
		    }

		    check_in3270();
		    check_linemode(false);
		}
		break;
	    }
	default:
	    dont_opt[2] = c;
	    net_rawout(dont_opt, sizeof(dont_opt));
	    vtrace("SENT %s %s\n", cmd(DONT), opt(c));
	    break;
	}
	telnet_state = TNS_DATA;
	break;
    case TNS_WONT:	/* telnet WONT DO OPTION command */
	vtrace("%s\n", opt(c));
	if (hisopts[c]) {
	    hisopts[c] = 0;
	    dont_opt[2] = c;
	    net_rawout(dont_opt, sizeof(dont_opt));
	    vtrace("SENT %s %s\n", cmd(DONT), opt(c));
	    check_in3270();
	    check_linemode(false);
	}
	telnet_state = TNS_DATA;
	break;
    case TNS_DO:	/* telnet PLEASE DO OPTION command */
	vtrace("%s\n", opt(c));
	switch (c) {
	case TELOPT_BINARY:
	case TELOPT_EOR:
	case TELOPT_TTYPE:
	case TELOPT_SGA:
	case TELOPT_NAWS:
	case TELOPT_TM:
	case TELOPT_TN3270E:
	case TELOPT_STARTTLS:
#if defined(HAVE_LIBSSL) /*[*/
	    if (c == TELOPT_STARTTLS && (!ssl_supported || !appres.ssl.tls)) {
		refused_tls = true;
		goto wont;
	    }
#else /*][*/
	    if (c == TELOPT_STARTTLS) {
		refused_tls = true;
		goto wont;
	    }
#endif /*]*/
	case TELOPT_NEW_ENVIRON:
	    if (c == TELOPT_TN3270E && HOST_FLAG(NON_TN3270E_HOST)) {
		goto wont;
	    }
	    if (c == TELOPT_TM && !appres.bsd_tm) {
		goto wont;
	    }
	    if (c == TELOPT_NEW_ENVIRON && !appres.new_environ) {
		goto wont;
	    }
	    if (c == TELOPT_TTYPE && myopts[TELOPT_NEW_ENVIRON] &&
		    !did_ne_send) {
		/*
		 * Defer sending WILL TTYPE until after the host asks for SB
		 * NEW_ENVIRON SEND.
		 */
		myopts[c] = 1;
		deferred_will_ttype = true;
		break;
	    }

	    if (!myopts[c]) {
		if (c != TELOPT_TM) {
		    myopts[c] = 1;
		}
		will_opt[2] = c;
		net_rawout(will_opt, sizeof(will_opt));
		vtrace("SENT %s %s\n", cmd(WILL), opt(c));
		check_in3270();
		check_linemode(false);
	    }
	    if (c == TELOPT_NAWS) {
		send_naws();
	    }
#if defined(HAVE_LIBSSL) /*[*/
	    if (c == TELOPT_STARTTLS) {
		static unsigned char follows_msg[] = {
		    IAC, SB, TELOPT_STARTTLS, TLS_FOLLOWS, IAC, SE
		};

		/*
		 * Send IAC SB STARTTLS FOLLOWS IAC SE to announce that what
		 * follows is TLS.
		 */
		net_rawout(follows_msg, sizeof(follows_msg));
		vtrace("SENT %s %s FOLLOWS %s\n", cmd(SB),
			opt(TELOPT_STARTTLS), cmd(SE));
		need_tls_follows = true;
	    }
#endif /*]*/
	    break;
	default:
	wont:
	    wont_opt[2] = c;
	    net_rawout(wont_opt, sizeof(wont_opt));
	    vtrace("SENT %s %s\n", cmd(WONT), opt(c));
	    break;
	}
	telnet_state = TNS_DATA;
	break;
    case TNS_DONT:	/* telnet PLEASE DON'T DO OPTION command */
	vtrace("%s\n", opt(c));
	if (myopts[c]) {
	    myopts[c] = 0;
	    wont_opt[2] = c;
	    net_rawout(wont_opt, sizeof(wont_opt));
	    vtrace("SENT %s %s\n", cmd(WONT), opt(c));
	    check_in3270();
	    check_linemode(false);
	}
	if (c == TELOPT_TTYPE && deferred_will_ttype) {
	    deferred_will_ttype = false;
	}
	telnet_state = TNS_DATA;
	break;
    case TNS_SB:	/* telnet sub-option string command */
	if (c == IAC) {
	    telnet_state = TNS_SB_IAC;
	} else {
	    *sbptr++ = c;
	}
	break;
    case TNS_SB_IAC:	/* telnet sub-option string command */
	*sbptr++ = c;
	if (c == SE) {
	    telnet_state = TNS_DATA;
	    if (sbbuf[0] == TELOPT_TTYPE && sbbuf[1] == TELQUAL_SEND) {
		size_t tt_len, tb_len;
		char *tt_out;

		vtrace("%s %s\n", opt(sbbuf[0]), telquals[sbbuf[1]]);
		if (lus != NULL && try_lu == NULL) {
		    /* None of the LUs worked. */
		    popup_an_error("Cannot connect to specified LU");
		    return false;
		}

		tt_len = strlen(termtype);
		if (try_lu != NULL && *try_lu) {
		    tt_len += strlen(try_lu) + 1;
		    connected_lu = try_lu;
		} else {
		    connected_lu = NULL;
		}
		status_lu(connected_lu);

		tb_len = 4 + tt_len + 2;
		tt_out = Malloc(tb_len + 1);
		(void) sprintf(tt_out, "%c%c%c%c%s%s%s%c%c",
			IAC, SB, TELOPT_TTYPE, TELQUAL_IS,
			force_ascii(termtype),
			(try_lu != NULL && *try_lu)? "@": "",
			(try_lu != NULL && *try_lu)?  force_ascii(try_lu) : "",
			IAC, SE);
		net_rawout((unsigned char *)tt_out, tb_len);
		Free(tt_out);

		vtrace("SENT %s %s %s %s%s%s %s\n", cmd(SB), opt(TELOPT_TTYPE),
			telquals[TELQUAL_IS], termtype,
			(try_lu != NULL && *try_lu)? "@": "",
			(try_lu != NULL && *try_lu)? try_lu: "",
			cmd(SE));

		/* Advance to the next LU name. */
		next_lu();
	    } else if (myopts[TELOPT_TN3270E] && sbbuf[0] == TELOPT_TN3270E) {
		if (tn3270e_negotiate()) {
		    return false;
		}
	    }
#if defined(HAVE_LIBSSL) /*[*/
	    else if (need_tls_follows && myopts[TELOPT_STARTTLS] &&
		    sbbuf[0] == TELOPT_STARTTLS) {
		continue_tls(sbbuf, (int)(sbptr - sbbuf));
	    }
#endif /*]*/
	    else if (sbbuf[0] == TELOPT_NEW_ENVIRON &&
		    sbbuf[1] == TELQUAL_SEND && appres.new_environ) {
		size_t tb_len;
		char *tt_out;
		char *user;

		vtrace("%s %s %s\n", opt(sbbuf[0]), telquals[sbbuf[1]],
			telobjs[sbbuf[2]]);

		/* Send out NEW-ENVIRON. */
		user = appres.user? appres.user: getenv("USER");
		if (user == NULL) {
		    user = "unknown";
		}
		tb_len = 21 + strlen(user) + strlen(appres.devname);
		tt_out = Malloc(tb_len + 1);
		(void) sprintf(tt_out, "%c%c%c%c%c%s%c%s%c%s%c%s%c%c",
			IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_IS, TELOBJ_VAR,
			force_ascii("USER"), TELOBJ_VALUE, force_ascii(user),
			TELOBJ_USERVAR, force_ascii("DEVNAME"), TELOBJ_VALUE,
			force_ascii(appres.devname), IAC, SE);
		net_rawout((unsigned char *)tt_out, tb_len);
		Free(tt_out);
		vtrace("SENT %s %s %s %s \"%s\" %s \"%s\" %s \"%s\" %s \"%s\""
			"\n", cmd(SB), opt(TELOPT_NEW_ENVIRON),
			telquals[TELQUAL_IS], telobjs[TELOBJ_VAR], "USER",
			telobjs[TELOBJ_VALUE], user, telobjs[TELOBJ_USERVAR],
			"DEVNAME", telobjs[TELOBJ_VALUE], appres.devname);

		/*
		 * Remember that we did a NEW_ENVIRON SEND, so we won't defer a
		 * future DO TTYPE.
		 */
		did_ne_send = true;

		/* Now respond to DO TTYPE. */
		if (deferred_will_ttype && myopts[TELOPT_TTYPE]) {
		    will_opt[2] = TELOPT_TTYPE;
		    net_rawout(will_opt, sizeof(will_opt));
		    vtrace("SENT %s %s\n", cmd(WILL), opt(TELOPT_TTYPE));
		    check_in3270();
		    check_linemode(false);
		    deferred_will_ttype = false;
		}
	    }

	} else {
	    telnet_state = TNS_SB;
	}
	break;
    }
    return true;
}

/* Send a TN3270E terminal type request. */
static void
tn3270e_request(void)
{
	size_t tt_len, tb_len;
	char *tt_out;
	char *t;
	char *xtn;

	/* Convert 3279 to 3278, per the RFC. */
	xtn = NewString(termtype);
	if (!strncmp(xtn, "IBM-3279", 8))
	    	xtn[7] = '8';

	tt_len = strlen(termtype);
	if (try_lu != NULL && *try_lu)
		tt_len += strlen(try_lu) + 1;

	tb_len = 5 + tt_len + 2;
	tt_out = Malloc(tb_len + 1);
	t = tt_out;
	t += sprintf(tt_out, "%c%c%c%c%c%s",
	    IAC, SB, TELOPT_TN3270E, TN3270E_OP_DEVICE_TYPE,
	    TN3270E_OP_REQUEST, force_ascii(xtn));

	if (try_lu != NULL && *try_lu)
		t += sprintf(t, "%c%s", TN3270E_OP_CONNECT,
			force_ascii(try_lu));

	(void) sprintf(t, "%c%c", IAC, SE);

	net_rawout((unsigned char *)tt_out, tb_len);
	Free(tt_out);

	vtrace("SENT %s %s DEVICE-TYPE REQUEST %s%s%s "
		   "%s\n",
	    cmd(SB), opt(TELOPT_TN3270E), xtn,
	    (try_lu != NULL && *try_lu)? " CONNECT ": "",
	    (try_lu != NULL && *try_lu)? try_lu: "",
	    cmd(SE));

	Free(xtn);
}

/*
 * Back off of TN3270E.
 */
static void
backoff_tn3270e(const char *why)
{
	vtrace("Aborting TN3270E: %s\n", why);

	/* Tell the host 'no'. */
	wont_opt[2] = TELOPT_TN3270E;
	net_rawout(wont_opt, sizeof(wont_opt));
	vtrace("SENT %s %s\n", cmd(WONT), opt(TELOPT_TN3270E));

	/* Restore the LU list; we may need to run it again in TN3270 mode. */
	setup_lus();

	/* Reset our internal state. */
	myopts[TELOPT_TN3270E] = 0;
	check_in3270();
}

/*
 * Negotiation of TN3270E options.
 * Returns 0 if okay, -1 if we have to give up altogether.
 */
static int
tn3270e_negotiate(void)
{
#define LU_MAX	32
	static char reported_lu[LU_MAX+1];
	static char reported_type[LU_MAX+1];
	int sblen;
	b8_t e_rcvd;

	/* Find out how long the subnegotiation buffer is. */
	for (sblen = 0; ; sblen++) {
		if (sbbuf[sblen] == SE)
			break;
	}

	vtrace("TN3270E ");

	switch (sbbuf[1]) {

	case TN3270E_OP_SEND:

		if (sbbuf[2] == TN3270E_OP_DEVICE_TYPE) {

			/* Host wants us to send our device type. */
			vtrace("SEND DEVICE-TYPE SE\n");

			tn3270e_request();
		} else {
			vtrace("SEND ??%u SE\n", sbbuf[2]);
		}
		break;

	case TN3270E_OP_DEVICE_TYPE:

		/* Device type negotiation. */
		vtrace("DEVICE-TYPE ");

		switch (sbbuf[2]) {
		case TN3270E_OP_IS: {
			int tnlen, snlen;

			/* Device type success. */

			/* Isolate the terminal type and session. */
			tnlen = 0;
			while (sbbuf[3+tnlen] != SE &&
			       sbbuf[3+tnlen] != TN3270E_OP_CONNECT)
				tnlen++;
			snlen = 0;
			if (sbbuf[3+tnlen] == TN3270E_OP_CONNECT) {
				while(sbbuf[3+tnlen+1+snlen] != SE)
					snlen++;
			}

			/* Remember the LU. */
			if (tnlen) {
				if (tnlen > LU_MAX)
					tnlen = LU_MAX;
				(void)strncpy(reported_type,
				    (char *)&sbbuf[3], tnlen);
				reported_type[tnlen] = '\0';
				force_local(reported_type);
				connected_type = reported_type;
			}
			if (snlen) {
				if (snlen > LU_MAX)
					snlen = LU_MAX;
				(void)strncpy(reported_lu,
				    (char *)&sbbuf[3+tnlen+1], snlen);
				reported_lu[snlen] = '\0';
				force_local(reported_lu);
				connected_lu = reported_lu;
				status_lu(connected_lu);
			}

			vtrace("IS %s CONNECT %s SE\n",
				tnlen? connected_type: "",
				snlen? connected_lu: "");

			/* Tell them what we can do. */
			tn3270e_subneg_send(TN3270E_OP_REQUEST, &e_funcs);
			break;
		}
		case TN3270E_OP_REJECT:

			/* Device type failure. */

			vtrace("REJECT REASON %s SE\n", rsn(sbbuf[4]));
			if (sbbuf[4] == TN3270E_REASON_UNSUPPORTED_REQ) {
				backoff_tn3270e("Host rejected request type");
				break;
			}

			next_lu();
			if (try_lu != NULL) {
				/* Try the next LU. */
				tn3270e_request();
			} else if (lus != NULL) {
				/* No more LUs to try.  Give up. */
				backoff_tn3270e("Host rejected resource(s)");
			} else {
				backoff_tn3270e("Device type rejected");
			}

			break;
		default:
			vtrace("??%u SE\n", sbbuf[2]);
			break;
		}
		break;

	case TN3270E_OP_FUNCTIONS:

		/* Functions negotiation. */
		vtrace("FUNCTIONS ");

		switch (sbbuf[2]) {

		case TN3270E_OP_REQUEST:
			/* Host is telling us what functions they want. */
			vtrace("REQUEST %s SE\n",
			    tn3270e_function_names(sbbuf+3, sblen-3));

			tn3270e_fdecode(sbbuf+3, sblen-3, &e_rcvd);
			if (b8_none_added(&e_funcs, &e_rcvd)) {
				/* They want what we want, or less.  Done. */
				b8_copy(&e_funcs, &e_rcvd);
				tn3270e_subneg_send(TN3270E_OP_IS, &e_funcs);
				tn3270e_negotiated = 1;
				vtrace("TN3270E option negotiation "
				    "complete.\n");
				check_in3270();
			} else {
				/*
				 * They want us to do something we can't.
				 * Request the common subset.
				 */
				b8_and(&e_funcs, &e_funcs, &e_rcvd);
				tn3270e_subneg_send(TN3270E_OP_REQUEST,
				    &e_funcs);
			}
			break;

		case TN3270E_OP_IS:
			/* They accept our last request, or a subset thereof. */
			vtrace("IS %s SE\n",
			    tn3270e_function_names(sbbuf+3, sblen-3));
			tn3270e_fdecode(sbbuf+3, sblen-3, &e_rcvd);
			if (b8_none_added(&e_funcs, &e_rcvd)) {
				/* They want what we want, or less.  Done. */
				b8_copy(&e_funcs, &e_rcvd);
			} else {
				/*
				 * They've added something. Abandon TN3270E,
				 * they're brain dead.
				 */
				backoff_tn3270e("Host illegally added "
					"function(s)");
				break;
			}
			tn3270e_negotiated = 1;
			vtrace("TN3270E option negotiation complete.\n");

			/*
			 * If the host does not support BIND_IMAGE, then we
			 * must go straight to 3270 mode. We do not implicitly
			 * unlock the keyboard, though -- that requires a
			 * Write command from the host.
			 */
			if (!b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE)) {
				tn3270e_submode = E_3270;
			}

			check_in3270();
			break;

		default:
			vtrace("??%u SE\n", sbbuf[2]);
			break;
		}
		break;

	default:
		vtrace("??%u SE\n", sbbuf[1]);
	}

	/* Good enough for now. */
	return 0;
}

/* Expand a string of TN3270E function codes into text. */
static const char *
tn3270e_function_names(const unsigned char *buf, int len)
{
	int i;
	static char text_buf[1024];
	char *s = text_buf;

	if (!len)
		return("(null)");
	for (i = 0; i < len; i++) {
		s += sprintf(s, "%s%s", (s == text_buf)? "": " ",
		    fnn(buf[i]));
	}
	return text_buf;
}

/* Expand the current TN3270E function codes into text. */
const char *
tn3270e_current_opts(void)
{
	int i;
	static char text_buf[1024];
	char *s = text_buf;

	if (b8_is_zero(&e_funcs) || !IN_E)
		return NULL;
	for (i = 0; i < MX8; i++) {
		if (b8_bit_is_set(&e_funcs, i)) {
			s += sprintf(s, "%s%s", (s == text_buf)? "": " ",
				fnn(i));
		}
	}
	return text_buf;
}

/* Transmit a TN3270E FUNCTIONS REQUEST or FUNCTIONS IS message. */
static void
tn3270e_subneg_send(unsigned char op, b8_t *funcs)
{
	unsigned char proto_buf[7 + MX8];
	int proto_len;
	int i;

	/* Construct the buffers. */
	(void) memcpy(proto_buf, functions_req, 4);
	proto_buf[4] = op;
	proto_len = 5;
	for (i = 0; i < MX8; i++) {
		if (b8_bit_is_set(funcs, (i))) {
			proto_buf[proto_len++] = i;
		}
	}

	/* Complete and send out the protocol message. */
	proto_buf[proto_len++] = IAC;
	proto_buf[proto_len++] = SE;
	net_rawout(proto_buf, proto_len);

	/* Complete and send out the trace text. */
	vtrace("SENT %s %s FUNCTIONS %s %s %s\n",
	    cmd(SB), opt(TELOPT_TN3270E),
	    (op == TN3270E_OP_REQUEST)? "REQUEST": "IS",
	    tn3270e_function_names(proto_buf + 5, proto_len - 7),
	    cmd(SE));
}

/* Translate a string of TN3270E functions into a bitmap. */
static void
tn3270e_fdecode(const unsigned char *buf, int len, b8_t *r)
{
	int i;

	b8_zero(r);
	for (i = 0; i < len; i++) {
		b8_set_bit(r, buf[i]);
	}
}

static int
maxru(unsigned char c)
{
    	if (!(c & 0x80))
	    	return 0;
	return ((c >> 4) & 0x0f) * (1 << (c & 0xf));
}

static void
process_bind(unsigned char *buf, size_t buflen)
{
	size_t namelen;
	size_t dest_ix = 0;

	/* Save the raw image. */
	Replace(bind_image, (unsigned char *)Malloc(buflen));
	memcpy(bind_image, buf, buflen);
	bind_image_len = buflen;

	/* Clean up the derived state. */
	if (plu_name == NULL)
	    	plu_name = Malloc(mb_max_len(BIND_PLU_NAME_MAX + 1));
	(void) memset(plu_name, '\0', mb_max_len(BIND_PLU_NAME_MAX + 1));
	maxru_sec = 0;
	maxru_pri = 0;
	bind_rd = 0;
	bind_cd = 0;
	bind_ra = 0;
	bind_ca = 0;
	bind_state = 0;

	/* Make sure it's a BIND. */
	if (buflen < 1 || buf[0] != BIND_RU) {
		return;
	}

	/* Extract the maximum RUs. */
	if (buflen > BIND_OFF_MAXRU_SEC)
		maxru_sec = maxru(buf[BIND_OFF_MAXRU_SEC]);
	if (buflen > BIND_OFF_MAXRU_PRI)
		maxru_pri = maxru(buf[BIND_OFF_MAXRU_PRI]);

	/* Extract the screen size. */
	if (buflen > BIND_OFF_SSIZE) {
	    	int bind_ss = buf[BIND_OFF_SSIZE];

	    	switch (bind_ss) {
		case 0x00:
		case 0x02:
			bind_rd = MODEL_2_ROWS;
			bind_cd = MODEL_2_COLS;
			bind_ra = MODEL_2_ROWS;
			bind_ca = MODEL_2_COLS;
			bind_state =
			    BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
			break;
		case 0x03:
			bind_rd = MODEL_2_ROWS;
			bind_cd = MODEL_2_COLS;
			bind_ra = maxROWS;
			bind_ca = maxCOLS;
			bind_state =
			    BIND_DIMS_PRESENT | BIND_DIMS_VALID;
			break;
		case 0x7e:
			bind_rd = buf[BIND_OFF_RD];
			bind_cd = buf[BIND_OFF_CD];
			bind_ra = buf[BIND_OFF_RD];
			bind_ca = buf[BIND_OFF_CD];
			bind_state =
			    BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
			break;
		case 0x7f:
			bind_rd = buf[BIND_OFF_RD];
			bind_cd = buf[BIND_OFF_CD];
			bind_ra = buf[BIND_OFF_RA];
			bind_ca = buf[BIND_OFF_CA];
			bind_state =
			    BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
			break;
		default:
			bind_state = 0;
			break;
		}
	}

	/* Validate and implement the screen size. */
	if (appres.bind_limit && (bind_state & BIND_DIMS_PRESENT)) {
		if (bind_rd > maxROWS ||
		    bind_cd > maxCOLS) {
			popup_an_error("Ignoring invalid BIND image screen "
				"size parameters:\n"
				" BIND Default Rows-Cols %ux%u > Maximum "
				"%ux%u",
				bind_rd, bind_cd, maxROWS, maxCOLS);
			bind_state &= ~BIND_DIMS_VALID;
		} else if (bind_rd < MODEL_2_ROWS ||
			   bind_cd < MODEL_2_COLS) {
			popup_an_error("Ignoring invalid BIND image screen "
				"size parameters:\n"
				" BIND Default Rows-Cols %ux%u < Minimum %ux%u",
				bind_rd, bind_cd, MODEL_2_ROWS, MODEL_2_COLS);
			bind_state &= ~BIND_DIMS_VALID;
		} else if (bind_ra > maxROWS ||
			   bind_ca > maxCOLS) {
			popup_an_error("Ignoring invalid BIND image screen "
				"size parameters:\n"
				" BIND Alternate Rows-Cols %ux%u > Maximum "
				"%ux%u",
				bind_ra, bind_ca, maxROWS, maxCOLS);
			bind_state &= ~BIND_DIMS_VALID;
		} else if (bind_ra < MODEL_2_ROWS ||
			   bind_ca < MODEL_2_COLS) {
			popup_an_error("Ignoring invalid BIND image screen "
				"size parameters:\n"
				" BIND Alternate Rows-Cols %ux%u < Minimum "
				"%ux%u",
				bind_ra, bind_ca, MODEL_2_ROWS, MODEL_2_COLS);
			bind_state &= ~BIND_DIMS_VALID;
		} else {
			defROWS = bind_rd;
			defCOLS = bind_cd;
			altROWS = bind_ra;
			altCOLS = bind_ca;
		}
	}

	ctlr_erase(false);

	/* Extract the PLU name. */
	if (buflen > BIND_OFF_PLU_NAME_LEN) {
		namelen = buf[BIND_OFF_PLU_NAME_LEN];
		if (namelen > BIND_PLU_NAME_MAX)
			namelen = BIND_PLU_NAME_MAX;
		if ((namelen > 0) && (buflen > BIND_OFF_PLU_NAME + namelen)) {
#if defined(EBCDIC_HOST) /*[*/
			memcpy(plu_name, &buf[BIND_OFF_PLU_NAME], namelen);
			plu_name[namelen] = '\0';
#else /*][*/
		    	size_t i;

			for (i = 0; i < namelen; i++) {
				size_t nx;

				nx = ebcdic_to_multibyte(
					buf[BIND_OFF_PLU_NAME + i],
					plu_name + dest_ix, mb_max_len(1));
				if (nx > 1)
					dest_ix += nx - 1;
			}
#endif /*]*/
		}
	}

	/* A BIND implicitly puts us in 3270 mode. */
	tn3270e_submode = E_3270;
}

/* Decode an UNBIND reason. */
static const char *
unbind_reason (unsigned char r)
{
    switch (r) {
    case TN3270E_UNBIND_NORMAL:
	return "normal";
    case TN3270E_UNBIND_BIND_FORTHCOMING:
	return "BIND forthcoming";
    case TN3270E_UNBIND_VR_INOPERATIVE:
	return "virtual route inoperative";
    case TN3270E_UNBIND_RX_INOPERATIVE:
	return "route extension inoperative";
    case TN3270E_UNBIND_HRESET:
	return "hierarchical reset";
    case TN3270E_UNBIND_SSCP_GONE:
	return "SSCP gone";
    case TN3270E_UNBIND_VR_DEACTIVATED:
	return "virtual route deactivated";
    case TN3270E_UNBIND_LU_FAILURE_PERM:
	return "unrecoverable LU failure";
    case TN3270E_UNBIND_LU_FAILURE_TEMP:
	return "recoverable LU failure";
    case TN3270E_UNBIND_CLEANUP:
	return "cleanup";
    case TN3270E_UNBIND_BAD_SENSE:
	return "bad sense code or user-supplied sense code";
    default:
	return lazyaf("unknown X'%02x'", r);
    }
}

static int
process_eor(void)
{
	if (syncing || !(ibptr - ibuf))
		return(0);

	if (IN_E) {
		tn3270e_header *h = (tn3270e_header *)ibuf;
		unsigned char *s;
		enum pds rv;

		vtrace("RCVD TN3270E(%s%s %s %u)\n",
		    e_dt(h->data_type),
		    e_rq(h->data_type, h->request_flag),
		    e_rsp(h->data_type, h->response_flag),
		    h->seq_number[0] << 8 | h->seq_number[1]);

		switch (h->data_type) {
		case TN3270E_DT_3270_DATA:
			if (b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE) &&
			    !tn3270e_bound) {
				return 0;
			}
			tn3270e_submode = E_3270;
			check_in3270();
			response_required = h->response_flag;
			rv = process_ds(ibuf + EH_SIZE,
			    (ibptr - ibuf) - EH_SIZE);
			if (rv < 0 &&
			    response_required != TN3270E_RSF_NO_RESPONSE)
				tn3270e_nak(rv);
			else if (rv == PDS_OKAY_NO_OUTPUT &&
			    response_required == TN3270E_RSF_ALWAYS_RESPONSE)
				tn3270e_ack();
			response_required = TN3270E_RSF_NO_RESPONSE;
			return 0;
		case TN3270E_DT_BIND_IMAGE:
			if (!b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE)) {
				return 0;
			}
			process_bind(ibuf + EH_SIZE, (ibptr - ibuf) - EH_SIZE);
			if (bind_state & BIND_DIMS_PRESENT) {
				if (bind_state & BIND_DIMS_ALT) {
					trace_ds("< BIND PLU-name '%s' "
						"MaxSec-RU %d MaxPri-RU %d "
						"Rows-Cols Default %dx%d "
						"Alternate %dx%d%s%s\n",
						plu_name, maxru_sec, maxru_pri,
						bind_rd, bind_cd,
						bind_ra, bind_ca,
						(bind_state & BIND_DIMS_VALID)?
						    "": " (invalid)",
						appres.bind_limit?
						    "": " (ignored)");
				} else {
					trace_ds("< BIND PLU-name '%s' "
						"MaxSec-RU %d MaxPri-RU %d "
						"Rows-Cols Default %dx%d%s%s\n",
						plu_name, maxru_sec, maxru_pri,
						bind_rd, bind_cd,
						(bind_state & BIND_DIMS_VALID)?
						    "": " (invalid)",
						appres.bind_limit?
						    "": " (ignored)");
				}
			} else {
				trace_ds("< BIND PLU-name '%s' "
					"MaxSec-RU %d MaxPri-RU %d\n",
					plu_name, maxru_sec, maxru_pri);
			}
			tn3270e_bound = 1;
			check_in3270();
			return 0;
		case TN3270E_DT_UNBIND:
			if (!b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE)) {
				return 0;
			}
			if ((ibptr - ibuf) > EH_SIZE) {
				trace_ds("< UNBIND %s\n",
					unbind_reason(ibuf[EH_SIZE]));
			}
			tn3270e_bound = 0;
			/*
			 * Undo any screen-sizing effects from a previous BIND.
			 */
			defROWS = MODEL_2_ROWS;
			defCOLS = MODEL_2_COLS;
			altROWS = maxROWS;
			altCOLS = maxCOLS;
			ctlr_erase(false);
			tn3270e_submode = E_UNBOUND;
			check_in3270();
			return 0;
		case TN3270E_DT_NVT_DATA:
			/* In tn3270e NVT mode */
			tn3270e_submode = E_NVT;
			check_in3270();
			for (s = ibuf; s < ibptr; s++) {
				nvt_process(*s++);
			}
			return 0;
		case TN3270E_DT_SSCP_LU_DATA:
			if (!b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE)) {
				return 0;
			}
			tn3270e_submode = E_SSCP;
			check_in3270();
			ctlr_write_sscp_lu(ibuf + EH_SIZE,
			                   (ibptr - ibuf) - EH_SIZE);
			return 0;
		default:
			/* Should do something more extraordinary here. */
			return 0;
		}
	} else {
		(void) process_ds(ibuf, ibptr - ibuf);
	}
	return 0;
}


/*
 * net_exception
 *	Called when there is an exceptional condition on the socket.
 */
void
net_exception(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
#if defined(LOCAL_PROCESS) /*[*/
    if (local_process) {
	vtrace("RCVD exception\n");
    } else
#endif /*[*/
    {
	vtrace("RCVD urgent data indication\n");
	if (!syncing) {
	    syncing = 1;
	    x_except_off();
	}
    }
}

/*
 * Flavors of Network Output:
 *
 *   3270 mode
 *	net_output	send a 3270 record
 *
 *   NVT mode; call each other in turn
 *	net_sendc	net_cookout for 1 byte
 *	net_sends	net_cookout for a null-terminated string
 *	net_cookout	send user data with cooked-mode processing, NVT mode
 *	net_cookedout	send user data, NVT mode, already cooked
 *	net_rawout	send telnet protocol data, NVT mode
 *
 */

/*
 * net_cookedout
 *      Send user data out in NVT mode, without cooked-mode processing.
 */
void
net_cookedout(const char *buf, size_t len)
{
    if (toggled(TRACING)) {
	size_t i;

	vtrace(">");
	for (i = 0; i < len; i++) {
	    vtrace(" %s", ctl_see((int)*(buf+i)));
	}
	vtrace("\n");
    }
    net_rawout((unsigned const char *)buf, len);
}

/*
 * net_cookout
 *      Send output in NVT mode, including cooked-mode processing if
 *      appropriate.
 */
void
net_cookout(const char *buf, size_t len)
{
    if (!IN_NVT || (kybdlock & KL_AWAITING_FIRST)) {
	return;
    }

    if (linemode) {
	linemode_out(buf, len);
    } else {
	net_cookedout(buf, len);
    }
}




/*
 * net_rawout
 *	Send out raw telnet data.  We assume that there will always be enough
 *	space to buffer what we want to transmit, so we don't handle EAGAIN or
 *	EWOULDBLOCK.
 */
static void
net_rawout(unsigned const char *buf, size_t len)
{
	int	nw;

	trace_netdata('>', buf, len);

	while (len) {
#if defined(OMTU) /*[*/
		size_t n2w = len;
		int pause = 0;

		if (n2w > OMTU) {
			n2w = OMTU;
			pause = 1;
		}
#else
#		define n2w len
#endif
#if defined(HAVE_LIBSSL) /*[*/
		if (ssl_con != NULL)
			nw = SSL_write(ssl_con, (const char *) buf, (int)n2w);
		else
#endif /*]*/
#if defined(LOCAL_PROCESS) /*[*/
		if (local_process)
			nw = write(sock, (const char *) buf, (int)n2w);
		else
#endif /*]*/
			nw = send(sock, (const char *) buf, (int)n2w, 0);
		if (nw < 0) {
#if defined(HAVE_LIBSSL) /*[*/
			if (ssl_con != NULL) {
				unsigned long e;
				char err_buf[120];

				e = ERR_get_error();
				(void) ERR_error_string(e, err_buf);
				vtrace("RCVD SSL_write error %ld (%s)\n", e,
				    err_buf);
				popup_an_error("SSL_write:\n%s", err_buf);
				host_disconnect(false);
				return;
			}
#endif /*]*/
			vtrace("RCVD socket error %d (%s)\n",
				socket_errno(),
				socket_strerror(socket_errno()));
			if (socket_errno() == SE_EPIPE ||
			    socket_errno() == SE_ECONNRESET) {
				host_disconnect(false);
				return;
			} else if (socket_errno() == SE_EINTR) {
				goto bot;
			} else {
				popup_a_sockerr("Socket write");
				host_disconnect(true);
				return;
			}
		}
		ns_bsent += nw;
		len -= nw;
		buf += nw;
	    bot:
#if defined(OMTU) /*[*/
		if (pause)
			sleep(1);
#endif /*]*/
		;
	}
}


/*
 * net_hexnvt_out
 *	Send uncontrolled user data to the host in NVT mode, performing IAC
 *	and CR quoting as necessary.
 */
void
net_hexnvt_out(unsigned char *buf, int len)
{
	unsigned char *tbuf;
	unsigned char *xbuf;

	if (!len)
		return;

	/* Trace the data. */
	if (toggled(TRACING)) {
		int i;

		vtrace(">");
		for (i = 0; i < len; i++)
			vtrace(" %s", ctl_see((int) *(buf+i)));
		vtrace("\n");
	}

	/* Expand it. */
	tbuf = xbuf = (unsigned char *)Malloc(2*len);
	while (len) {
		unsigned char c = *buf++;

		*tbuf++ = c;
		len--;
		if (c == IAC)
			*tbuf++ = IAC;
		else if (c == '\r' && (!len || *buf != '\n'))
			*tbuf++ = '\0';
	}

	/* Send it to the host. */
	net_rawout(xbuf, tbuf - xbuf);
	Free(xbuf);
}


/*
 * check_in3270
 *	Check for switches between NVT, SSCP-LU and 3270 modes.
 */
static void
check_in3270(void)
{
	enum cstate new_cstate = NOT_CONNECTED;
	static const char *state_name[] = {
		"unconnected",				/* NOT_CONNECTED */
		"resolving hostname",			/* RESOLVING */
		"TCP connection pending",		/* PENDING */
		"negotiating SSL or proxy",		/* NEGOTIATING */
		"connected; 3270 state unknown",	/* CONNECTED_INITIAL */
		"TN3270 NVT",				/* CONNECTED_NVT */
		"TN3270 3270",				/* CONNECTED_3270 */
		"TN3270E unbound",			/* CONNECTED_UNBOUND */
		"TN3270E NVT",				/* CONNECTED_E_NVT */
		"TN3270E SSCP-LU",			/* CONNECTED_SSCP */
		"TN3270E 3270"				/* CONNECTED_TN3270E */
	};

	if (myopts[TELOPT_TN3270E]) {
		if (!tn3270e_negotiated)
			new_cstate = CONNECTED_UNBOUND;
		else switch (tn3270e_submode) {
		case E_UNBOUND:
			new_cstate = CONNECTED_UNBOUND;
			break;
		case E_NVT:
			new_cstate = CONNECTED_E_NVT;
			break;
		case E_3270:
			new_cstate = CONNECTED_TN3270E;
			break;
		case E_SSCP:
			new_cstate = CONNECTED_SSCP;
			break;
		}
	} else if (myopts[TELOPT_BINARY] &&
	           myopts[TELOPT_EOR] &&
	           myopts[TELOPT_TTYPE] &&
	           hisopts[TELOPT_BINARY] &&
	           hisopts[TELOPT_EOR]) {
		new_cstate = CONNECTED_3270;
	} else if (cstate == CONNECTED_INITIAL) {
		/* Nothing has happened, yet. */
		return;
	} else if (appres.nvt_mode) {
		new_cstate = CONNECTED_NVT;
	} else {
		new_cstate = CONNECTED_INITIAL;
	}

	if (new_cstate != cstate) {
		int was_in_e = IN_E;

		/*
		 * If we've now switched between non-TN3270E mode and
		 * TN3270E mode, reset the LU list so we can try again
		 * in the new mode.
		 */
		if (lus != NULL && was_in_e != IN_E) {
			curr_lu = lus;
			try_lu = *curr_lu;
		}

		/* Allocate the initial 3270 input buffer. */
		if (new_cstate >= CONNECTED_INITIAL && !ibuf_size) {
			ibuf = (unsigned char *)Malloc(BUFSIZ);
			ibuf_size = BUFSIZ;
			ibptr = ibuf;
		}

		/* Reinitialize line mode. */
		if ((new_cstate == CONNECTED_NVT && linemode) ||
		    new_cstate == CONNECTED_E_NVT) {
			linemode_buf_init();
		}

		/* If we fell out of TN3270E, remove the state. */
		if (!myopts[TELOPT_TN3270E]) {
			tn3270e_negotiated = 0;
			tn3270e_submode = E_UNBOUND;
			tn3270e_bound = 0;
		}
		vtrace("Now operating in %s mode.\n",
			state_name[new_cstate]);
		if (IN_3270 || IN_NVT || IN_SSCP) {
			any_host_data = true;
		}
		host_in3270(new_cstate);
	}
}

/*
 * store3270in
 *	Store a character in the 3270 input buffer, checking for buffer
 *	overflow and reallocating ibuf if necessary.
 */
static void
store3270in(unsigned char c)
{
	if (ibptr - ibuf >= ibuf_size) {
		ibuf_size += BUFSIZ;
		ibuf = (unsigned char *)Realloc((char *)ibuf, ibuf_size);
		ibptr = ibuf + ibuf_size - BUFSIZ;
	}
	*ibptr++ = c;
}

/*
 * space3270out
 *	Ensure that <n> more characters will fit in the 3270 output buffer.
 *	Allocates the buffer in BUFSIZ chunks.
 *	Allocates hidden space at the front of the buffer for TN3270E.
 */
void
space3270out(size_t n)
{
	size_t nc = 0;	/* amount of data currently in obuf */
	unsigned more = 0;

	if (obuf_size)
		nc = obptr - obuf;

	while ((nc + n + EH_SIZE) > (obuf_size + more)) {
		more += BUFSIZ;
	}

	if (more) {
		obuf_size += more;
		obuf_base = (unsigned char *)Realloc((char *)obuf_base,
			obuf_size);
		obuf = obuf_base + EH_SIZE;
		obptr = obuf + nc;
	}
}


/*
 * check_linemode
 *	Set the global variable 'linemode', which says whether we are in
 *	character-by-character mode or line mode.
 */
static void
check_linemode(bool init)
{
    int wasline = linemode;

    /*
     * The next line is a deliberate kluge to effectively ignore the SGA
     * option.  If the host will echo for us, we assume
     * character-at-a-time; otherwise we assume fully cooked by us.
     *
     * This allows certain IBM hosts which volunteer SGA but refuse
     * ECHO to operate more-or-less normally, at the expense of
     * implementing the (hopefully useless) "character-at-a-time, local
     * echo" mode.
     *
     * We still implement "switch to line mode" and "switch to character
     * mode" properly by asking for both SGA and ECHO to be off or on, but
     * we basically ignore the reply for SGA.
     */
    linemode = !hisopts[TELOPT_ECHO] /* && !hisopts[TELOPT_SGA] */;

    if (init || linemode != wasline) {
	st_changed(ST_LINE_MODE, linemode);
	if (!init) {
	    vtrace("Operating in %s mode.\n",
		    linemode? "line": "character-at-a-time");
	}
	if (IN_NVT) {
	    if (linemode) {
		linemode_buf_init();
	    } else {
		linemode_dump();
	    }
	}
    }
}


/*
 * nnn
 *	Expands a number to a character string, for displaying unknown telnet
 *	commands and options.
 */
static const char *
nnn(int c)
{
    return lazyaf("%d", c);
}

/*
 * cmd
 *	Expands a TELNET command into a character string.
 */
static const char *
cmd(int c)
{
    if (TELCMD_OK(c)) {
	return TELCMD(c);
    } else {
	return nnn(c);
    }
}

/*
 * opt
 *	Expands a TELNET option into a character string.
 */
static const char *
opt(unsigned char c)
{
    if (TELOPT_OK(c)) {
	return TELOPT(c);
    } else if (c == TELOPT_TN3270E) {
	return "TN3270E";
    } else if (c == TELOPT_STARTTLS) {
	return "START-TLS";
    } else {
	return nnn((int)c);
    }
}


#define LINEDUMP_MAX	32

void
trace_netdata(char direction, unsigned const char *buf, size_t len)
{
	size_t offset;

	if (!toggled(TRACING))
		return;
	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			ntvtrace("%s%c 0x%-3x ",
			    (offset? "\n": ""), direction, (unsigned)offset);
		ntvtrace("%02x", buf[offset]);
	}
	ntvtrace("\n");
}


/*
 * net_output
 *	Send 3270 output over the network:
 *	- Prepend TN3270E header
 *	- Expand IAC to IAC IAC
 *	- Append IAC EOR
 */
void
net_output(void)
{
	static unsigned char *xobuf = NULL;
	static int xobuf_len = 0;
	int need_resize = 0;
	unsigned char *nxoptr, *xoptr;

#define BSTART	((IN_TN3270E || IN_SSCP)? obuf_base: obuf)

	/* Set the TN3720E header. */
	if (IN_TN3270E || IN_SSCP) {
		tn3270e_header *h = (tn3270e_header *)obuf_base;

		/* Check for sending a TN3270E response. */
		if (response_required == TN3270E_RSF_ALWAYS_RESPONSE) {
			tn3270e_ack();
			response_required = TN3270E_RSF_NO_RESPONSE;
		}

		/* Set the outbound TN3270E header. */
		h->data_type = IN_TN3270E?
			TN3270E_DT_3270_DATA: TN3270E_DT_SSCP_LU_DATA;
		h->request_flag = 0;
		h->response_flag = 0;
		h->seq_number[0] = (e_xmit_seq >> 8) & 0xff;
		h->seq_number[1] = e_xmit_seq & 0xff;

		vtrace("SENT TN3270E(%s NO-RESPONSE %u)\n",
			IN_TN3270E? "3270-DATA": "SSCP-LU-DATA", e_xmit_seq);
		if (b8_bit_is_set(&e_funcs, TN3270E_FUNC_RESPONSES)) {
			e_xmit_seq = (e_xmit_seq + 1) & 0x7fff;
		}
	}

	/* Reallocate the expanded output buffer. */
	while (xobuf_len <  (obptr - BSTART + 1) * 2) {
		xobuf_len += BUFSZ;
		need_resize++;
	}
	if (need_resize) {
		Replace(xobuf, (unsigned char *)Malloc(xobuf_len));
	}

	/* Copy and expand IACs. */
	xoptr = xobuf;
	nxoptr = BSTART;
	while (nxoptr < obptr) {
		if ((*xoptr++ = *nxoptr++) == IAC) {
			*xoptr++ = IAC;
		}
	}

	/* Append the IAC EOR and transmit. */
	*xoptr++ = IAC;
	*xoptr++ = EOR;
	net_rawout(xobuf, xoptr - xobuf);

	vtrace("SENT EOR\n");
	ns_rsent++;
#undef BSTART
}

/* Send a TN3270E positive response to the server. */
static void
tn3270e_ack(void)
{
	unsigned char rsp_buf[10];
	tn3270e_header *h_in = (tn3270e_header *)ibuf;
	int rsp_len = 0;

	rsp_len = 0;
	rsp_buf[rsp_len++] = TN3270E_DT_RESPONSE;	    /* data_type */
	rsp_buf[rsp_len++] = 0;				    /* request_flag */
	rsp_buf[rsp_len++] = TN3270E_RSF_POSITIVE_RESPONSE; /* response_flag */	
	rsp_buf[rsp_len++] = h_in->seq_number[0];	    /* seq_number[0] */
	if (h_in->seq_number[0] == IAC)
		rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = h_in->seq_number[1];	    /* seq_number[1] */
	if (h_in->seq_number[1] == IAC)
		rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = TN3270E_POS_DEVICE_END;
	rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = EOR;
	vtrace("SENT TN3270E(RESPONSE POSITIVE-RESPONSE %u) DEVICE-END\n",
		h_in->seq_number[0] << 8 | h_in->seq_number[1]);
	net_rawout(rsp_buf, rsp_len);
}

/* Send a TN3270E negative response to the server. */
static void
tn3270e_nak(enum pds rv)
{
	unsigned char rsp_buf[10];
	tn3270e_header *h_in = (tn3270e_header *)ibuf;
	int rsp_len = 0;
	char *neg = NULL;

	rsp_buf[rsp_len++] = TN3270E_DT_RESPONSE;	    /* data_type */
	rsp_buf[rsp_len++] = 0;				    /* request_flag */
	rsp_buf[rsp_len++] = TN3270E_RSF_NEGATIVE_RESPONSE; /* response_flag */
	rsp_buf[rsp_len++] = h_in->seq_number[0];	    /* seq_number[0] */
	if (h_in->seq_number[0] == IAC)
		rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = h_in->seq_number[1];	    /* seq_number[1] */
	if (h_in->seq_number[1] == IAC)
		rsp_buf[rsp_len++] = IAC;
	switch (rv) {
	default:
	case PDS_BAD_CMD:
		rsp_buf[rsp_len++] = TN3270E_NEG_COMMAND_REJECT;
		neg = "COMMAND-REJECT";
		break;
	case PDS_BAD_ADDR:
		rsp_buf[rsp_len++] = TN3270E_NEG_OPERATION_CHECK;
		neg = "OPERATION-CHECK";
		break;
	}
	rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = EOR;
	vtrace("SENT TN3270E(RESPONSE NEGATIVE-RESPONSE %u) %s\n",
		h_in->seq_number[0] << 8 | h_in->seq_number[1], neg);
	net_rawout(rsp_buf, rsp_len);
}

/* Add a dummy TN3270E header to the output buffer. */
bool
net_add_dummy_tn3270e(void)
{
	tn3270e_header *h;

	if (!IN_E || tn3270e_submode == E_UNBOUND)
		return false;

	space3270out(EH_SIZE);
	h = (tn3270e_header *)obptr;

	switch (tn3270e_submode) {
	case E_UNBOUND:
		break;
	case E_NVT:
		h->data_type = TN3270E_DT_NVT_DATA;
		break;
	case E_SSCP:
		h->data_type = TN3270E_DT_SSCP_LU_DATA;
		break;
	case E_3270:
		h->data_type = TN3270E_DT_3270_DATA;
		break;
	}
	h->request_flag = 0;
	h->response_flag = TN3270E_RSF_NO_RESPONSE;
	h->seq_number[0] = 0;
	h->seq_number[1] = 0;
	obptr += EH_SIZE;
	return true;
}

/*
 * Add IAC EOR to a buffer.
 */
void
net_add_eor(unsigned char *buf, size_t len)
{
	buf[len++] = IAC;
	buf[len++] = EOR;
}


/*
 * net_sendc
 *	Send a character of user data over the network in NVT mode.
 */
void
net_sendc(char c)
{
	if (c == '\r' && !linemode
#if defined(LOCAL_PROCESS) /*[*/
				   && !local_process
#endif /*]*/
						    ) {
		/* CR must be quoted */
		net_cookout("\r\0", 2);
	} else {
		net_cookout(&c, 1);
	}
}


/*
 * net_sends
 *	Send a null-terminated string of user data in NVT mode.
 */
void
net_sends(const char *s)
{
	net_cookout(s, strlen(s));
}


/*
 * External entry points to negotiate line or character mode.
 */
void
net_linemode(void)
{
	if (!CONNECTED)
		return;
	if (hisopts[TELOPT_ECHO]) {
		dont_opt[2] = TELOPT_ECHO;
		net_rawout(dont_opt, sizeof(dont_opt));
		vtrace("SENT %s %s\n", cmd(DONT), opt(TELOPT_ECHO));
	}
	if (hisopts[TELOPT_SGA]) {
		dont_opt[2] = TELOPT_SGA;
		net_rawout(dont_opt, sizeof(dont_opt));
		vtrace("SENT %s %s\n", cmd(DONT), opt(TELOPT_SGA));
	}
}

void
net_charmode(void)
{
	if (!CONNECTED)
		return;
	if (!hisopts[TELOPT_ECHO]) {
		do_opt[2] = TELOPT_ECHO;
		net_rawout(do_opt, sizeof(do_opt));
		vtrace("SENT %s %s\n", cmd(DO), opt(TELOPT_ECHO));
	}
	if (!hisopts[TELOPT_SGA]) {
		do_opt[2] = TELOPT_SGA;
		net_rawout(do_opt, sizeof(do_opt));
		vtrace("SENT %s %s\n", cmd(DO), opt(TELOPT_SGA));
	}
}


/*
 * net_break
 *	Send telnet break, which is used to implement 3270 ATTN.
 *
 */
void
net_break(void)
{
	static unsigned char buf[] = { IAC, BREAK };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	vtrace("SENT BREAK\n");
}

/*
 * net_interrupt
 *	Send telnet IP.
 *
 */
void
net_interrupt(void)
{
	static unsigned char buf[] = { IAC, IP };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	vtrace("SENT IP\n");
}

/*
 * net_abort
 *	Send telnet AO.
 *
 */
void
net_abort(void)
{
	static unsigned char buf[] = { IAC, AO };

	if (b8_bit_is_set(&e_funcs, TN3270E_FUNC_SYSREQ)) {
		/*
		 * I'm not sure yet what to do here.  Should the host respond
		 * to the AO by sending us SSCP-LU data (and putting us into
		 * SSCP-LU mode), or should we put ourselves in it?
		 * Time, and testers, will tell.
		 */
		switch (tn3270e_submode) {
		case E_UNBOUND:
		case E_NVT:
			break;
		case E_SSCP:
			net_rawout(buf, sizeof(buf));
			vtrace("SENT AO\n");
			if (tn3270e_bound || !b8_bit_is_set(&e_funcs,
						    TN3270E_FUNC_BIND_IMAGE)) {
				tn3270e_submode = E_3270;
				check_in3270();
			}
			break;
		case E_3270:
			net_rawout(buf, sizeof(buf));
			vtrace("SENT AO\n");
			tn3270e_submode = E_SSCP;
			check_in3270();
			break;
		}
	}
}

/*
 * Construct a string to reproduce the current TELNET options.
 * Returns a bool indicating whether it is necessary.
 */
bool
net_snap_options(void)
{
	bool any = false;
	int i;
	static unsigned char ttype_str[] = {
		IAC, DO, TELOPT_TTYPE,
		IAC, SB, TELOPT_TTYPE, TELQUAL_SEND, IAC, SE
	};

	if (!CONNECTED)
		return false;

	obptr = obuf;

	/* Do TTYPE first. */
	if (myopts[TELOPT_TTYPE]) {
		unsigned j;

		space3270out(sizeof(ttype_str));
		for (j = 0; j < sizeof(ttype_str); j++)
			*obptr++ = ttype_str[j];
	}

	/* Do the other options. */
	for (i = 0; i < N_OPTS; i++) {
		space3270out(6);
		if (i == TELOPT_TTYPE)
			continue;
		if (hisopts[i]) {
			*obptr++ = IAC;
			*obptr++ = WILL;
			*obptr++ = (unsigned char)i;
			any = true;
		}
		if (myopts[i]) {
			*obptr++ = IAC;
			*obptr++ = DO;
			*obptr++ = (unsigned char)i;
			any = true;
		}
	}

	/* If we're in TN3270E mode, snap the subnegotations as well. */
	if (myopts[TELOPT_TN3270E]) {
		any = true;

		space3270out(5 +
			((connected_type != NULL)? strlen(connected_type): 0) +
			((connected_lu != NULL)? + strlen(connected_lu): 0) +
			2);
		*obptr++ = IAC;
		*obptr++ = SB;
		*obptr++ = TELOPT_TN3270E;
		*obptr++ = TN3270E_OP_DEVICE_TYPE;
		*obptr++ = TN3270E_OP_IS;
		if (connected_type != NULL) {
			(void) memcpy(obptr, connected_type,
					strlen(connected_type));
			obptr += strlen(connected_type);
		}
		if (connected_lu != NULL) {
			*obptr++ = TN3270E_OP_CONNECT;
			(void) memcpy(obptr, connected_lu,
					strlen(connected_lu));
			obptr += strlen(connected_lu);
		}
		*obptr++ = IAC;
		*obptr++ = SE;

		space3270out(38);
		(void) memcpy(obptr, functions_req, 4);
		obptr += 4;
		*obptr++ = TN3270E_OP_IS;
		for (i = 0; i < MX8; i++) {
			if (b8_bit_is_set(&e_funcs, i)) {
				*obptr++ = i;
			}
		}
		*obptr++ = IAC;
		*obptr++ = SE;

		if (tn3270e_bound) {
			tn3270e_header *h;
			size_t i;
			int xlen = 0;

			for (i = 0; i < bind_image_len; i++)  {
			    if (bind_image[i] == 0xff)
				xlen++;
			}

			space3270out(EH_SIZE + bind_image_len + xlen + 3);
			h = (tn3270e_header *)obptr;
			h->data_type = TN3270E_DT_BIND_IMAGE;
			h->request_flag = 0;
			h->response_flag = 0;
			h->seq_number[0] = 0;
			h->seq_number[1] = 0;
			obptr += EH_SIZE;
			for (i = 0; i < bind_image_len; i++) {
			    if (bind_image[i] == 0xff)
				*obptr++ = 0xff;
			    *obptr++ = bind_image[i];
			}
			*obptr++ = IAC;
			*obptr++ = EOR;
		}
	}
	return any;
}

/*
 * Set blocking/non-blocking mode on the socket.  On error, pops up an error
 * message, but does not close the socket.
 */
static int
non_blocking(bool on)
{
#if !defined(BLOCKING_CONNECT_ONLY) /*[*/
# if defined(FIONBIO) /*[*/
    IOCTL_T i = on? 1: 0;

    vtrace("Making host socket %sblocking\n", on? "non-": "");
    if (sock == INVALID_SOCKET) {
	return 0;
    }

    if (SOCK_IOCTL(sock, FIONBIO, &i) < 0) {
	popup_a_sockerr("ioctl(FIONBIO, %d)", on);
	return -1;
    }
# else /*][*/
    int f;

    vtrace("Making host socket %sblocking\n", on? "non-": "");
    if (sock == INVALID_SOCKET) {
	return 0;
    }

    if ((f = fcntl(sock, F_GETFL, 0)) == -1) {
	popup_an_errno(errno, "fcntl(F_GETFL)");
	return -1;
    }
    if (on) {
	f |= O_NDELAY;
    } else {
	f &= ~O_NDELAY;
    }
    if (fcntl(sock, F_SETFL, f) < 0) {
	popup_an_errno(errno, "fcntl(F_SETFL)");
	return -1;
    }
# endif /*]*/
#endif /*]*/
    return 0;
}

#if defined(HAVE_LIBSSL) /*[*/

/* Password callback. */
static int
passwd_cb(char *buf, int size, int rwflag _is_unused,
	void *userdata _is_unused)
{
    if (appres.ssl.key_passwd == NULL) {
	int psize = ssl_passwd_gui_callback(buf, size);

	if (psize >= 0) {
	    return psize;
	} else {
	    popup_an_error("No OpenSSL private key password specified");
	    return 0;
	}
    }

    if (!strncasecmp(appres.ssl.key_passwd, "string:", 7)) {
	/* Plaintext in the resource. */
	size_t len = strlen(appres.ssl.key_passwd + 7);

	if (len > (size_t)size - 1) {
	    len = size - 1;
	}
	strncpy(buf, appres.ssl.key_passwd + 7, len);
	buf[len] = '\0';
	return (int)len;
    } else if (!strncasecmp(appres.ssl.key_passwd, "file:", 5)) {
	/* In a file. */
	FILE *f;
	char *s;

	f = fopen(appres.ssl.key_passwd + 5, "r");
	if (f == NULL) {
	    popup_an_errno(errno, "OpenSSL private key file '%s'",
		    appres.ssl.key_passwd + 5);
	    return 0;
	}
	memset(buf, '\0', size);
	s = fgets(buf, size - 1, f);
	fclose(f);
	return s? (int)strlen(s): 0;
    } else {
	popup_an_error("Unknown OpenSSL private key syntax '%s'",
		appres.ssl.key_passwd);
	return 0;
    }
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
    unsigned long e;

    e = ERR_get_error();
    if (getenv("SSL_VERBOSE_ERRORS")) {
	(void) ERR_error_string(e, buf);
    } else {
	char xbuf[120];
	char *colon;

	(void) ERR_error_string(e, xbuf);
	colon = strrchr(xbuf, ':');
	if (colon != NULL) {
	    strcpy(buf, colon + 1);
	} else {
	    strcpy(buf, xbuf);
	}
    }
    return buf;
}

/*
 * Base-level initialization.
 * Happens once, before the screen switches modes (for c3270).
 */
void
ssl_base_init(char *cl_hostname, bool *pending)
{
    char err_buf[120];
    int cert_file_type = SSL_FILETYPE_PEM;

#if defined(_WIN32) /*[*/
    if (ssl_dll_init() < 0) {
	/* The DLLs may not be there, or may be the wrong ones. */
	vtrace("SSL DLL init failed: %s\n", ssl_fail_reason);
	ssl_supported = false;
	return;
    }
#endif /*]*/

    /* Parse the -accepthostname option. */
    if (appres.ssl.accept_hostname != NULL) {
	if (!strcasecmp(appres.ssl.accept_hostname, "any") ||
	    !strcmp(appres.ssl.accept_hostname, "*")) {
	    accept_specified_host = true;
	    accept_dnsname = "*";
	} else if (!strncasecmp(appres.ssl.accept_hostname, "DNS:", 4) &&
		    appres.ssl.accept_hostname[4] != '\0') {
	    accept_specified_host = true;
	    accept_dnsname = &appres.ssl.accept_hostname[4];
	} else if (!strncasecmp(appres.ssl.accept_hostname, "IP:", 3)) {
	    unsigned short port;
	    sockaddr_46_t ahaddr;
	    socklen_t len;
	    char *errmsg;
	    rhp_t rv;

	    rv = resolve_host_and_port(&appres.ssl.accept_hostname[3],
		    "0", 0, &port, &ahaddr.sa, &len, &errmsg,
		    NULL);
	    if (RHP_IS_ERROR(rv)) {
		popup_an_error("Invalid acceptHostname '%s': %s",
			appres.ssl.accept_hostname, errmsg);
		return;
	    }
	    switch (ahaddr.sa.sa_family) {
	    case AF_INET:
		memcpy(&host_inaddr, &ahaddr.sin.sin_addr,
			sizeof(struct in_addr));
		host_inaddr_valid = true;
		accept_specified_host = true;
		accept_dnsname = "";
		break;
#if defined(X3270_IPV6) /*[*/
	    case AF_INET6:
		memcpy(&host_in6addr, &ahaddr.sin6.sin6_addr,
			sizeof(struct in6_addr));
		host_in6addr_valid = true;
		accept_specified_host = true;
		accept_dnsname = "";
		break;
#endif /*]*/
	    default:
		break;
	    }

	} else {
	    popup_an_error("Cannot parse acceptHostname '%s' "
		    "(must be 'any' or 'DNS:name' or 'IP:addr')",
		    appres.ssl.accept_hostname);
	    return;
	}
    }

    if (cl_hostname != NULL)
	ssl_cl_hostname = NewString(cl_hostname);
    if (pending != NULL) {
	*pending = false;
	ssl_pending = pending;
    }

    SSL_load_error_strings();
    SSL_library_init();
try_again:
    ssl_passwd_gui_reset();
    ssl_ctx = SSL_CTX_new(SSLv23_method());
    if (ssl_ctx == NULL) {
	popup_an_error("SSL_CTX_new failed");
	goto fail;
    }
    SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
    SSL_CTX_set_info_callback(ssl_ctx, client_info_callback);
    SSL_CTX_set_default_passwd_cb(ssl_ctx, passwd_cb);

    /* Pull in the CA certificate file. */
    if (appres.ssl.ca_file != NULL || appres.ssl.ca_dir != NULL) {
	if (SSL_CTX_load_verify_locations(ssl_ctx, appres.ssl.ca_file,
		    appres.ssl.ca_dir) != 1) {
	    popup_an_error("CA database load (%s%s%s%s%s%s%s%s%s) failed:\n%s",
		    appres.ssl.ca_file? "file ": "",
		    appres.ssl.ca_file? "\"": "",
		    appres.ssl.ca_file? appres.ssl.ca_file: "",
		    appres.ssl.ca_file? "\"": "",
		    (appres.ssl.ca_file && appres.ssl.ca_dir)? ", ": "",
		    appres.ssl.ca_dir? "dir ": "",
		    appres.ssl.ca_dir? "\"": "",
		    appres.ssl.ca_dir? appres.ssl.ca_dir: "",
		    appres.ssl.ca_dir? "\"": "",
		    get_ssl_error(err_buf));
	    goto fail;
	}
    } else {
#if defined(_WIN32) /*[*/
	char *certs;

	/*
	 * Look in:
	 * (1) Current directory.
	 * (2) Install directory, which if the program wasn't actually
	 *     installed, is the directory of the path it was run from.
	 */
#define readable(path)	(access(path, R_OK) == 0)
	if (!readable(certs = ROOT_CERTS) &&
	    !readable(certs = lazyaf("%s%s", instdir, ROOT_CERTS))) {
	    popup_an_error("No %s found", ROOT_CERTS);
	    goto fail;
	}
#undef readable

	if (SSL_CTX_load_verify_locations(ssl_ctx, certs, NULL) != 1) {
	    popup_an_error("CA database load (file \"%s\") failed:\n%s", certs,
		    get_ssl_error(err_buf));
	    goto fail;
	}
#else /*][*/
	SSL_CTX_set_default_verify_paths(ssl_ctx);
#endif /*]*/
    }

    /* Pull in the client certificate file. */
    if (appres.ssl.chain_file != NULL) {
	if (SSL_CTX_use_certificate_chain_file(ssl_ctx,
		    appres.ssl.chain_file) != 1) {
	    popup_an_error("Client certificate chain file load (\"%s\") "
		    "failed:\n%s", appres.ssl.chain_file,
		    get_ssl_error(err_buf));
	    goto fail;
	}
    } else if (appres.ssl.cert_file != NULL) {
	cert_file_type = parse_file_type(appres.ssl.cert_file_type);
	if (cert_file_type == -1) {
	    popup_an_error("Invalid client certificate file type '%s'",
		    appres.ssl.cert_file_type);
	    goto fail;
	}
	if (SSL_CTX_use_certificate_file(ssl_ctx, appres.ssl.cert_file,
		    cert_file_type) != 1) {
	    popup_an_error("Client certificate file load (\"%s\") failed:\n%s",
		    appres.ssl.cert_file, get_ssl_error(err_buf));
	    goto fail;
	}
    }

    /* Pull in the private key file. */
    if (appres.ssl.key_file != NULL) {
	int key_file_type = parse_file_type(appres.ssl.key_file_type);

	if (key_file_type == -1) {
	    popup_an_error("Invalid private key file type '%s'",
		    appres.ssl.key_file_type);
	    goto fail;
	}
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, appres.ssl.key_file,
		    key_file_type) != 1) {
	    if (pending == NULL || !*pending) {
		popup_an_error("Private key file load (\"%s\") failed:\n%s",
			appres.ssl.key_file, get_ssl_error(err_buf));
	    }
	    goto password_fail;
	}
    } else if (appres.ssl.chain_file != NULL) {
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, appres.ssl.chain_file,
		    SSL_FILETYPE_PEM) != 1) {
	    if (pending == NULL || !*pending) {
		popup_an_error("Private key file load (\"%s\") failed:\n%s",
			appres.ssl.chain_file, get_ssl_error(err_buf));
	    }
	    goto password_fail;
	}
    } else if (appres.ssl.cert_file != NULL) {
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, appres.ssl.cert_file,
		    cert_file_type) != 1) {
	    if (pending == NULL || !*pending) {
		popup_an_error("Private key file load (\"%s\") "
			"failed:\n%s", appres.ssl.cert_file,
			get_ssl_error(err_buf));
	    }
	    goto password_fail;
	}
    }

    /* Check the key. */
    if (appres.ssl.key_file != NULL &&
	    SSL_CTX_check_private_key(ssl_ctx) != 1) {
	popup_an_error("Private key check failed:\n%s",
		get_ssl_error(err_buf));
	goto fail;
    }
    ssl_pending = NULL;

    return;

password_fail:
    if (ssl_passwd_gui_retry()) {
	SSL_CTX_free(ssl_ctx);
	ssl_ctx = NULL;
	goto try_again;
    }

fail:
    ssl_pending = NULL;
    if (ssl_ctx != NULL) {
	SSL_CTX_free(ssl_ctx);
	ssl_ctx = NULL;
    }
    return;
}

/* Verify function. */
static int
ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    int err;
    char *why_not = NULL;

    /* If OpenSSL thinks it's okay, so do we. */
    if (preverify_ok) {
	return 1;
    }

    /* Fetch the error. */
    err = X509_STORE_CTX_get_error(ctx);

    /* We might not care. */
    if (!appres.ssl.verify_host_cert) {
	why_not = "not verifying";
    } else if (appres.ssl.self_signed_ok &&
	    (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
	     err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)) {
	why_not = "self-signed okay";
    }
    if (why_not != NULL) {
	char *s;

	vtrace("SSL_verify_callback: %s, ignoring '%s' (%d)\n",
		why_not, X509_verify_cert_error_string(err), err);
	secure_unverified = true;
	s = xs_buffer("%s (%d)", X509_verify_cert_error_string(err), err);
	add_unverified_reason(s);
	Free(s);
	return 1;
    }

    /* Then again, we might. */
    return 0;
}

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

/* IP address match function. */
static int
ipaddr_matches(unsigned char *v4addr, unsigned char *v6addr,
	unsigned char *data, int len)
{
    switch (len) {
    case 4:
	if (v4addr) {
	    return !memcmp(v4addr, data, 4);
	}
	break;
    case 16:
	if (v6addr) {
	    return !memcmp(v6addr, data, 16);
	}
	break;
    default:
	break;
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

	    new = xs_buffer("%s%s%s", r? r: "", r? " ": "", list[i]);
	    Replace(r, new);
	}
    }
    return r? r: NewString("(none)");
}

/* Hostname validation function. */
static char *
spc_verify_cert_hostname(X509 *cert, char *hostname, unsigned char *v4addr,
	unsigned char *v6addr)
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
	if (!strcmp(hostname, "*") ||
	     (!v4addr && !v6addr &&
	      hostname_matches(hostname, name, len))) {
	    ok = 1;
	    vtrace("SSL_connect: commonName %s matches hostname %s\n", name,
		    hostname);
	} else {
	    vtrace("SSL_connect: non-matching commonName: %s\n",
		    expand_hostname(name, len));
	    nnl = xs_buffer("DNS:%s", expand_hostname(name, len));
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
		    (!v4addr && !v6addr &&
		     hostname_matches(hostname, (char *)dns, len))) {

		    ok = 1;
		    vtrace("SSL_connect: alternameName DNS:%s matches "
			    "hostname %s\n", expand_hostname((char *)dns, len),
			    hostname);
		    OPENSSL_free(dns);
		    break;
		} else {
		    vtrace("SSL_connect: non-matching alternateName: DNS:%s\n",
			    expand_hostname((char *)dns, len));
		    nnl = xs_buffer("DNS:%s", expand_hostname((char *)dns,
				len));
		    namelist = add_to_namelist(namelist, nnl);
		    Free(nnl);
		}
		OPENSSL_free(dns);
	    } else if (value->type == GEN_IPADD) {
		int i;
		char *ipbuf;

		if (!strcmp(hostname, "*") ||
			ipaddr_matches(v4addr, v6addr,
			    value->d.iPAddress->data,
			    value->d.iPAddress->length)) {
		    vtrace("SSL_connect: matching alternateName IP:");
		    ok = 1;
		} else {
		    vtrace("SSL_connect: non-matching alternateName: IP:");
		}
		ipbuf = NewString("IP:");
		switch (value->d.iPAddress->length) {
		case 4:
		    for (i = 0; i < 4; i++) {
			nnl = xs_buffer("%s%s%u", ipbuf, (i > 0)? ".": "",
				value->d.iPAddress->data[i]);
			Replace(ipbuf, nnl);
		    }
		    break;
		case 16:
		    for (i = 0; i < 16; i+= 2) {
			nnl = xs_buffer("%s%s%x", ipbuf,
				(i > 0)? ":": "",
	 (value->d.iPAddress->data[i] << 8) | value->d.iPAddress->data[i + 1]);
			Replace(ipbuf, nnl);
		    }
		    break;
		default:
		    for (i = 0; i < value->d.iPAddress->length; i++) {
			nnl = xs_buffer("%s%s%u", ipbuf, (i > 0)? "-": "",
				value->d.iPAddress->data[i]);
			Replace(ipbuf, nnl);
		    }
		    break;
		}
		vtrace("%s\n", ipbuf);
		if (!ok) {
		    namelist = add_to_namelist(namelist, ipbuf);
		}
		Free(ipbuf);
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

/* Create a new OpenSSL connection. */
static int
ssl_init(void)
{
    if (!ssl_supported) {
	popup_an_error("Cannot connect:\nSSL DLLs not found\n");
	return -1;
    }
    if (ssl_ctx == NULL) {
	popup_an_error("Cannot connect:\nSSL initialization error");
	return -1;
    }

    ssl_con = SSL_new(ssl_ctx);
    if (ssl_con == NULL) {
	popup_an_error("SSL_new failed");
	return -1;
    }
    SSL_set_verify_depth(ssl_con, 64);
    vtrace("SSL_init: %sverifying host certificate\n",
	    appres.ssl.verify_host_cert? "": "not ");
    SSL_set_verify(ssl_con, SSL_VERIFY_PEER, ssl_verify_callback);
    return 0;
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
		(void) ERR_error_string(e, err_buf + 1);
#if defined(_WIN32) /*[*/
	    } else if (GetLastError() != 0) {
		strcpy(err_buf + 1, win32_strerror(GetLastError()));
#else /*][*/
	    } else if (errno != 0) {
		strcpy(err_buf + 1, strerror(errno));
#endif /*]*/
	    } else {
		err_buf[0] = '\0';
	    }
	    st = xs_buffer("SSL_connect trace: error in %s%s",
		    SSL_state_string_long(s), err_buf);
	    if ((colon = strrchr(st, ':')) != NULL) {
		*colon = '\n';
	    }

	    popup_an_error("%s", st);
	    Free(st);
	}
    }
}

/* Process a STARTTLS subnegotiation. */
static void
continue_tls(unsigned char *sbbuf, int len)
{
    int rv;

    /* Whatever happens, we're not expecting another SB STARTTLS. */
    need_tls_follows = false;

    /* Make sure the option is FOLLOWS. */
    if (len < 2 || sbbuf[1] != TLS_FOLLOWS) {
	/* Trace the junk. */
	vtrace("%s ? %s\n", opt(TELOPT_STARTTLS), cmd(SE));
	popup_an_error("TLS negotiation failure");
	net_disconnect();
	return;
    }

    /* Trace what we got. */
    vtrace("%s FOLLOWS %s\n", opt(TELOPT_STARTTLS), cmd(SE));

    /* Initialize the SSL library. */
    if (ssl_init() < 0) {
	/* Failed. */
	net_disconnect();
	return;
    }

    /* Set up the TLS/SSL connection. */
    if (SSL_set_fd(ssl_con, (int)sock) != 1) {
	vtrace("Can't set fd!\n");
    }

#if defined(_WIN32) /*[*/
    /* Make the socket blocking for SSL. */
    (void) WSAEventSelect(sock, sock_handle, 0);
    (void) non_blocking(false);
#endif /*]*/

    rv = SSL_connect(ssl_con);

#if defined(_WIN32) /*[*/
    /* Make the socket non-blocking again for event processing. */
    (void) WSAEventSelect(sock, sock_handle, FD_READ | FD_CONNECT | FD_CLOSE);
#endif /*]*/

    if (rv != 1) {
	long v;

	v = SSL_get_verify_result(ssl_con);
	if (v != X509_V_OK)
	    popup_an_error("Host certificate verification failed:\n%s (%ld)%s",
		    X509_verify_cert_error_string(v), v,
		    (v == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)?
	       "\nCA certificate needs to be added to the local database": "");

	/*
	 * No need to trace the error, it was already
	 * displayed.
	 */
	host_disconnect(true);
	return;
    }

    /* Check the host certificate. */
    if (!check_cert_name()) {
	host_disconnect(true);
	return;
    }

    secure_connection = true;

    /* Success. */
    vtrace("TLS/SSL negotiated connection complete. "
	    "Connection is now secure.\n");

    /* Tell the world that we are (still) connected, now in secure mode. */
    host_connected();
}

#else /*][*/
static int
ssl_init(void)
{
    popup_an_error("Secure connections not supported");
    return -1;
}
#endif /*]*/

/* Return the current BIND application name, if any. */
const char *
net_query_bind_plu_name(void)
{
    /*
     * Return the PLU name, if we're in TN3270E 3270 mode and have
     * negotiated the BIND-IMAGE option.
     */
    if ((cstate == CONNECTED_TN3270E) &&
	    b8_bit_is_set(&e_funcs, TN3270E_FUNC_BIND_IMAGE)) {
	return plu_name? plu_name: "";
    } else {
	return "";
    }
}

/* Return the current connection state. */
const char *
net_query_connection_state(void)
{
    if (CONNECTED) {
	if (IN_E) {
	    switch (tn3270e_submode) {
	    default:
	    case E_UNBOUND:
		return "tn3270e unbound";
	    case E_3270:
		return "tn3270e 3270";
	    case E_NVT:
		return "tn3270e nvt";
	    case E_SSCP:
		return "tn3270 sscp-lu";
	    }
	} else {
	    if (IN_3270) {
		return "tn3270 3270";
	    } else {
		return "tn3270 nvt";
	    }
	}
    } else if (HALF_CONNECTED) {
	return "connecting";
    } else {
	return "";
    }
}

/* Return the LU name. */
const char *
net_query_lu_name(void)
{
    if (CONNECTED && connected_lu != NULL) {
	return connected_lu;
    } else {
	return "";
    }
}

/* Return the hostname and port. */
const char *
net_query_host(void)
{
    if (CONNECTED) {
#if defined(LOCAL_PROCESS) /*[*/
	if (local_process) {
	    return lazyaf("process %s", hostname);
	}
#endif /*]*/
	return lazyaf("host %s %u", hostname, current_port);
    } else {
	return "";
    }
}

/* Return the SSL state. */
const char *
net_query_ssl(void)
{
    static char *not_secure = "not secure";

    if (CONNECTED) {
#if defined(HAVE_LIBSSL) /*[*/
	if (!secure_connection) {
	    return not_secure;
	}
	return lazyaf("secure %s",
		secure_unverified? "host-unverified": "host-verified");
#else /*][*/
	return not_secure;
#endif /*]*/
    } else {
	return "";
    }
}

/* Return the local address for the socket. */
int
net_getsockname(void *buf, int *len)
{
    if (sock == INVALID_SOCKET) {
	return -1;
    }
    return getsockname(sock, buf, (socklen_t *)(void *)len);
}

/* Return a text version of the current proxy type, or NULL. */
char *
net_proxy_type(void)
{
    if (proxy_type > 0) {
	return proxy_type_name(proxy_type);
    } else {
	return NULL;
    }
}

/* Return the current proxy host, or NULL. */
char *
net_proxy_host(void)
{
    if (proxy_type > 0) {
	return proxy_host;
    } else {
	return NULL;
    }
}

/* Return the current proxy port, or NULL. */
char *
net_proxy_port(void)
{
    if (proxy_type > 0) {
	return proxy_portname;
    } else {
	return NULL;
    }
}

/* Return the SNA binding state. */
bool
net_bound(void)
{
    return (IN_E && tn3270e_bound);
}

/*
 * Set the default termtype.
 *
 * This is called at init time, whenever we disconnect, and whenever the screen
 * dimensions change (which by definition happens while we are disconnected).
 * It sets 'termtype' to the default value, assuming an extended data stream
 * host. When we connect to a particular host, we may use a different value
 * (such as without the -E, for the S: prefix).
 */
void
net_set_default_termtype(void)
{
    if (appres.termname) {
	termtype = appres.termname;
    } else if (appres.nvt_mode) {
	termtype = "xterm";
    } else if (ov_rows || ov_cols) {
	termtype = "IBM-DYNAMIC";
    } else {
	termtype = full_model_name;
    }
}
