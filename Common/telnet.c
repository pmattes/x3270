/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
#include <assert.h>
#include "tn3270e.h"
#include "3270ds.h"

#include "appres.h"

#include "actions.h"
#include "b3270proto.h"
#include "b8.h"
#include "boolstr.h"
#include "ctlrc.h"
#include "host.h"
#include "indent_s.h"
#include "kybd.h"
#include "linemode.h"
#include "model.h"
#include "names.h"
#include "nvt.h"
#include "popups.h"
#include "proxy.h"
#include "query.h"
#include "resolver.h"
#include "resources.h"
#include "sio.h"
#include "varbuf.h"	/* must precede sioc.h */
#include "sioc.h"
#include "split_host.h"
#include "stats.h"
#include "task.h"
#include "telnet.h"
#include "telnet_core.h"
#include "telnet_gui.h"
#include "telnet_private.h"
#include "telnet_sio.h"
#include "tls_passwd_gui.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
#include "unicodec.h"
#include "utils.h"
#include "vstatus.h"
#include "w3misc.h"
#include "xio.h"

#if !defined(TELOPT_NAWS) /*[*/
# define TELOPT_NAWS	31
#endif /*]*/

#if !defined(TELOPT_STARTTLS) /*[*/
# define TELOPT_STARTTLS	46
#endif /*]*/
#define TLS_FOLLOWS	1
#define TELNET_PORT	23
#define TELNETS_PORT	992

#define BUFSZ		32768
#define TRACELINE	72

#define N_OPTS		256

#define LINEMODE_NAME	"lineMode"

#if !defined(LOCAL_PROCESS) /*[*/
# define local_process false
#endif /*]*/

/* Globals */
char    	*hostname = NULL;
time_t          ns_time;
int             ns_brcvd;
int             ns_rrcvd;
int             ns_bsent;
int             ns_rsent;
unsigned char  *obuf;		/* 3270 output buffer */
unsigned char  *obptr = (unsigned char *) NULL;
bool            linemode = true;
bool		charmode_onlcr = false;
#if defined(LOCAL_PROCESS) /*[*/
bool		local_process = false;
#endif /*]*/
char           *termtype;
struct timeval	net_last_recv_ts;
char	       *numeric_host;
char	       *numeric_port;

const char *telquals[3] = { "IS", "SEND", "INFO" };

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
static ioid_t	nop_timeout_id = NULL_IOID;
static char     ttype_tmpval[13];

static unsigned short e_xmit_seq; /* transmit sequence number */
static int response_required;

static size_t   nvt_data = 0;
static bool	nvt_any = false;
static bool	nvt_last_cmd = false;
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

static proxytype_t proxy_type = PT_NONE;
static bool	proxy_pending = false;
static char	*proxy_user = NULL;
static char	*proxy_host = NULL;
static char	*proxy_portname = NULL;
static unsigned short proxy_port = 0;

static b8_t e_funcs;		/* negotiated TN3270E functions */

static bool	secure_connection;
static char	*net_accept;

static enum cstate starttls_pending = NOT_CONNECTED;

typedef enum {
    NTIM_LINE,
    NTIM_CHARACTER,
    NTIM_CHARACTER_CRLF,
    NTIM_UNKNOWN
} ntim_t;
static const char *ntim_name[] = {
    "line", "character", "characterCrLf", NULL
};
static ntim_t ntim = NTIM_UNKNOWN;
static ntim_t parse_ntim(const char *value);

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
static void net_starttls_continue(void);

static const char *nnn(int c);

static void net_hexnvt_out_framed(unsigned char *buf, size_t len, bool framed);

static void trace_envt_in(unsigned const char *buf, size_t len);

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

static const char *reason_code[8] = { "CONN-PARTNER", "DEVICE-IN-USE",
	"INV-ASSOCIATE", "INV-NAME", "INV-DEVICE-TYPE", "TYPE-NAME-ERROR",
	"UNKNOWN-ERROR", "UNSUPPORTED-REQ" };
#  define rsn(n)	(((n) <= TN3270E_REASON_UNSUPPORTED_REQ)? \
			reason_code[(n)]: "??")
static const char *function_name[7] = { "BIND-IMAGE", "DATA-STREAM-CTL",
	"RESPONSES", "SCS-CTL-CODES", "SYSREQ", "CONTENTION-RESOLUTION",
	"SNA-SENSE" };
# define fnn(n)	(((n) <= TN3270E_FUNC_SNA_SENSE)? \
			function_name[(n)]: "??")
static const char *data_type[10] = { "3270-DATA", "SCS-DATA", "RESPONSE",
	"BIND-IMAGE", "UNBIND", "NVT-DATA", "REQUEST", "SSCP-LU-DATA",
	"PRINT-EOJ", "BID" };
# define e_dt(n)	(((n) <= TN3270E_DT_BID)? \
			data_type[(n)]: "??")
static const char *hrsp_flag[3] = { "NO-RESPONSE", "ERROR-RESPONSE",
	"ALWAYS-RESPONSE" };
# define e_hrsp(n) (((n) <= TN3270E_RSF_ALWAYS_RESPONSE)? \
			hrsp_flag[(n)]: "??")
static const char *trsp_flag[3] = { "POSITIVE-RESPONSE", "NEGATIVE-RESPONSE",
	"SNA-SENSE" };
# define e_trsp(n) (((n) <= TN3270E_RSF_SNA_SENSE)? \
			trsp_flag[(n)]: "??")
# define e_rsp(fn, n) (((fn) == TN3270E_DT_RESPONSE)? e_trsp(n): e_hrsp(n))
const char *state_name[NUM_CSTATE] = {
    CstateNotConnected,
    CstateReconnecting,
    CstateTlsPasswordPending,
    CstateResolving,
    CstateTcpPending,
    CstateTlsPending,
    CstateProxyPending,
    CstateTelnetPending,
    CstateConnectedNvt,
    CstateConnectedNvtCharmode,
    CstateConnected3270,
    CstateConnectedUnbound,
    CstateConnectedEnvt,
    CstateConnectedSscp,
    CstateConnectedTn3270e
};

#if !defined(_WIN32) /*[*/
# define XMIT_ROWS	((appres.c3270.altscreen)? MODEL_2_ROWS: maxROWS)
# define XMIT_COLS	((appres.c3270.altscreen)? MODEL_2_COLS: maxCOLS)
#else /*][*/
# define XMIT_ROWS	maxROWS
# define XMIT_COLS	maxCOLS
#endif /*]*/

sio_t sio = NULL;

static void continue_tls(unsigned char *sbbuf, int len);
static bool refused_tls = false;
static bool nested_tls = false;
static bool any_host_data = false;
static bool need_tls_follows = false;

static bool net_connect_pending;

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
    struct sockaddr_in6 sin6;
} sockaddr_46_t;

#define NUM_HA	4
static sockaddr_46_t haddr[NUM_HA];
static socklen_t ha_len[NUM_HA] = {
    sizeof(haddr[0]), sizeof(haddr[0]), sizeof(haddr[0]), sizeof(haddr[0])
};
static int num_ha = 0;
static int ha_ix = 0;
static int resolver_pipe[2] = { -1, -1 };
static int resolver_slot = -1;
static iosrc_t resolver_event = INVALID_IOSRC;

#if defined(_WIN32) /*[*/
void
popup_a_sockerr(const char *fmt, ...)
{
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = txVasprintf(fmt, args);
    va_end(args);
    connect_error("%s: %s", buffer, win32_strerror(socket_errno()));
}
#else /*][*/
void
popup_a_sockerr(const char *fmt, ...)
{
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = txVasprintf(fmt, args);
    va_end(args);
    connect_errno(errno, "%s", buffer);
}
#endif /*]*/

/* The host connection timed out. */
static void
connect_timed_out(ioid_t id _is_unused)
{
    connect_error("Host connection timed out");
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
    u_short		*portp;
    int			port;
    char		*errmsg;
#if defined(OMTU) /*[*/
    int			mtu = OMTU;
#endif /*]*/
#   define close_fail	{ SOCK_CLOSE(sock); \
    			  sock = INVALID_SOCKET; \
    			  return INVALID_IOSRC; \
			}

    /* create the socket */
    if ((sock = socket(haddr[ix].sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) ==
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
    if (ut_getenv("BLOCKING_CONNECT") == NULL && non_blocking(true) < 0) {
	popup_an_error("non-blocking failure");
	close_fail;
    }

#if !defined(_WIN32) /*[*/
    /* don't share the socket with our children */
    fcntl(sock, F_SETFD, 1);
#endif /*]*/

    /* Init TLS. */
    portp = (haddr[ix].sa.sa_family == AF_INET)? &haddr[ix].sin.sin_port: &haddr[ix].sin6.sin6_port;
    port = ntohs(*portp);
    if (appres.tls992 && port == TELNETS_PORT) {
	SET_HOST_nFLAG(host_flags, TLS_HOST);
    }
    if (HOST_FLAG(TLS_HOST)) {
	if (!sio_supported()) {
	    popup_an_error("TLS not supported\n");
	    close_fail;
	}
    }

    if (numeric_host_and_port(&haddr[ix].sa, ha_len[ix], hn, sizeof(hn), pn,
		sizeof(pn), &errmsg)) {
	vtrace("Trying %s, port %s...\n", hn, pn);
	telnet_gui_connecting(hn, pn);
	Replace(numeric_host, NewString(hn));
	Replace(numeric_port, NewString(pn));
    }

    /* Set an explicit timeout, if configured. */
    if (appres.connect_timeout) {
	connect_timeout_id = AddTimeOut(appres.connect_timeout * 1000,
		connect_timed_out);
    }

    /*
     * Implement a test point to remap port 992, so the tls992 resource can be tested without binding
     * to port 992, which requires root.
     */
    if (port == TELNETS_PORT) {
	const char *remap992 = ut_getenv("REMAP992");

	if (remap992 != NULL) {
	    *portp = htons(atoi(remap992));
	}
    }
    if (port == TELNET_PORT) {
	const char *remap23 = ut_getenv("REMAP23");

	if (remap23 != NULL) {
	    *portp = htons(atoi(remap23));
	}
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
		popup_a_sockerr("%s%s, port %d",
			(proxy_type != PT_NONE)? "Proxy ": "",
			(proxy_type != PT_NONE)? proxy_host : hostname,
			(proxy_type != PT_NONE)? proxy_port : current_port);
	    }
	    close_fail;
	}
    } else {
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

/* Complete a connection, now that the hostname has been resolved. */
static net_connect_t
finish_connect(iosrc_t *iosrc)
{
    iosrc_t s;

    /* Set up the TLS context, whether this is an TLS host or not. */
    if (sio_supported()) {
	bool pending = false;

	sio = sio_init_wrapper(NULL, HOST_FLAG(NO_VERIFY_CERT_HOST),
		net_accept, &pending);
	if (sio == NULL) {
	    if (pending) {
		net_connect_pending = true;
		return NC_TLS_PASS;
	    }
	    net_disconnect(false);
	    return NC_FAILED;
	}
    }

    /* Try each of the haddrs. */
    ha_ix = 0;
    while (ha_ix < num_ha) {
	bool pending = false;

	if ((s = connect_to(ha_ix, (ha_ix == num_ha - 1),
			&pending)) != INVALID_IOSRC) {
	    *iosrc = s;
	    return pending? NC_CONNECT_PENDING: NC_CONNECTED;
	}
	ha_ix++;
    }

    /* Ran out. */
    return NC_FAILED;
}

/* There is data on the resolver pipe. */
static void
resolve_done(iosrc_t fd, ioid_t id)
{
    int nr;
    char slot_byte;
    int slot;
    int rv;
    char *errmsg;
    iosrc_t iosrc = INVALID_IOSRC;
    net_connect_t nc;

    /* Read the data, which is the slot number. */
    nr = read(resolver_pipe[0], &slot_byte, 1);
    if (nr < 0) {
	popup_an_errno(errno, "Resolver pipe");
	return;
    }
    if (nr == 0) {
	popup_an_error("Resolver pipe EOF");
    }

    /* Might be a canceled request. */
    slot = (int)slot_byte;
    if (slot != resolver_slot) {
	vtrace("Cleaning up canceled resolver slot %d\n", slot);
	cleanup_host_and_port(slot);
	return;
    }

    rv = collect_host_and_port(slot, &haddr[0].sa, sizeof(haddr[0]), ha_len,
	    &current_port, &errmsg, NUM_HA, &num_ha);
    if (RHP_IS_ERROR(rv)) {
	connect_error("%s", errmsg);
	return;
    }
    vtrace("Resolution complete, %d address%s\n", num_ha, 
	    (num_ha == 1)? "": "es");

    /* Proceed with the connection. */
    nc = finish_connect(&iosrc);
    if (nc == NC_FAILED) {
	host_disconnect(true);
    } else {
	host_continue_connect(iosrc, nc);
    }
}

/*
 * net_connect
 *	Establish a telnet socket to the given host passed as an argument.
 *	Called only once and is responsible for setting up the telnet
 *	variables.  Returns the file descriptor of the connected socket.
 */
net_connect_t
net_connect(const char *host, char *portname, char *accept, bool ls,
	iosrc_t *iosrc)
{
    struct servent       *sp;
    struct hostent       *hp;
    char	       	passthru_haddr[8];
    int			passthru_len = 0;
    unsigned short	passthru_port = 0;
    char		*errmsg;

    if (sizeof(state_name)/sizeof(state_name[0]) != NUM_CSTATE) {
	Error("telnet cstate_name has the wrong number of elements");
    }

    *iosrc = INVALID_IOSRC;

    if (netrbuf == NULL) {
	netrbuf = (unsigned char *)Malloc(BUFSZ);
    }

    linemode_init();
    ns_brcvd = 0;
    ns_rrcvd = 0;
    ns_bsent = 0;
    ns_rsent = 0;

    environ_init();

    Replace(hostname, NewString(host));
    Replace(net_accept, NewString(accept));

    starttls_pending = NOT_CONNECTED;
    st_changed(ST_SECURE, false);

    /* set up temporary termtype */
    net_set_default_termtype();

    /* get the passthru host and port number */
    if (HOST_FLAG(PASSTHRU_HOST)) {
	const char *hn;

	hn = getenv("INTERNET_HOST");
	if (hn == NULL) {
	    hn = "internet-gateway";
	}

	hp = gethostbyname(hn);
	if (hp == (struct hostent *) 0) {
	    connect_error("Unknown passthru host: %s", hn);
	    return NC_FAILED;
	}
	memmove(passthru_haddr, hp->h_addr, hp->h_length);
	passthru_len = hp->h_length;

	sp = getservbyname("telnet-passthru","tcp");
	if (sp != NULL) {
	    passthru_port = sp->s_port;
	} else {
	    passthru_port = htons(3514);
	}
    } else if (appres.proxy != NULL) {
	proxytype_t pt;
	unsigned long lport;
	char *ptr;
	struct servent *sp;

	pt = proxy_setup(appres.proxy, &proxy_user, &proxy_host,
		&proxy_portname);
	if (pt == PT_ERROR) {
	    return NC_FAILED;
	}

	lport = strtoul(portname, &ptr, 0);
	if (ptr == portname || *ptr != '\0' || lport == 0L ||
		lport & ~0xffff) {
	    if (!(sp = getservbyname(portname, "tcp"))) {
		connect_error("Unknown port number or service: %s",
			portname);
		return NC_FAILED;
	    }
	    current_port = ntohs(sp->s_port);
	} else {
	    current_port = (unsigned short)lport;
	}
	proxy_type = pt;
	proxy_pending = true;
    }

    /* fill in the socket address of the given host */
    memset((char *) &haddr, 0, sizeof(haddr));
    if (HOST_FLAG(PASSTHRU_HOST)) {
	/*
	 * XXX: We don't try multiple addresses for the passthru
	 * host.
	 */
	haddr[0].sin.sin_family = AF_INET;
	memmove(&haddr[0].sin.sin_addr, passthru_haddr, passthru_len);
	haddr[0].sin.sin_port = passthru_port;
	ha_len[0] = sizeof(struct sockaddr_in);
	num_ha = 1;
	ha_ix = 0;
    } else if (proxy_pending) {
	/*
	 * XXX: We don't try multiple addresses for a proxy
	 * host.
	 */
	rhp_t rv;
	int nr;

	rv = resolve_host_and_port(proxy_host, proxy_portname, &proxy_port,
		&haddr[0].sa, sizeof(haddr[0]), &ha_len[0], &errmsg, 1, &nr);
	if (RHP_IS_ERROR(rv)) {
	    connect_error("%s", errmsg);
	    return NC_FAILED;
	}
	num_ha = 1;
	ha_ix = 0;
    } else {
#if defined(LOCAL_PROCESS) /*[*/
	if (ls) {
	    local_process = true;
	} else {
#endif /*]*/
	    rhp_t rv;

	    if (resolver_pipe[0] == -1) {
		int rv;

#if !defined(_WIN32) /*[*/
		rv = pipe(resolver_pipe);
#else /*][*/
		rv = _pipe(resolver_pipe, 512, _O_BINARY);
#endif /*]*/
		if (rv < 0) {
		    connect_error("resolver pipe: %s", strerror(errno));
		    return NC_FAILED;
		}
#if !defined(_WIN32) /*[*/
		AddInput(resolver_pipe[0], resolve_done);
#else /*][*/
		resolver_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		AddInput(resolver_event, resolve_done);
#endif /*]*/
	    }

#if defined(LOCAL_PROCESS) /*[*/
	    local_process = false;
#endif /*]*/
	    rv = resolve_host_and_port_a(host, portname, &current_port,
		    &haddr[0].sa, sizeof(haddr[0]), ha_len, &errmsg, NUM_HA,
		    &num_ha, &resolver_slot, resolver_pipe[1], resolver_event);
	    if (RHP_IS_ERROR(rv)) {
		connect_error("%s", errmsg);
		return NC_FAILED;
	    }
	    ha_ix = 0;

	    if (rv == RHP_PENDING) {
		vtrace("Resolver slot is %d\n", resolver_slot);
		return NC_RESOLVING;
	    }
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
	    connect_errno(errno, "forkpty");
	    close_fail;
	case 0:	/* child */
	    putenv(Asprintf("TERM=%s",
		appres.termname?
		    appres.termname:
		    (mode3279? "xterm-color": "xterm")));
	    if (strchr(host, ' ') != NULL) {
		execlp("/bin/sh", "sh", "-c", host, NULL);
	    } else {
		char *arg1;

		arg1 = strrchr(host, '/');
		execlp(host, (arg1 == NULL)? host: arg1 + 1, NULL);
	    }
	    perror(host);
	    _exit(1);
	    break;
	default:	/* parent */
	    sock = amaster;
	    fcntl(sock, F_SETFD, 1);
	    connection_complete();
	    host_in3270(linemode? CONNECTED_NVT: CONNECTED_NVT_CHAR);
	    host_set_flag(NO_TELNET_HOST);
	    ntim = NTIM_CHARACTER;
	    break;
	}
	*iosrc = sock;
	return NC_CONNECTED;
    }
#endif /*]*/

    return finish_connect(iosrc);
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
    strcpy(lu, luname);
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

/* Send a periodic TELNET NOP. */
static void
send_nop(ioid_t id _is_unused)
{
    static unsigned char nop[] = { IAC, NOP };

    if (cstate >= TELNET_PENDING) {
	net_rawout(nop, sizeof(nop));
	vtrace("SENT NOP\n");
    }
    nop_timeout_id = AddTimeOut(appres.nop_seconds * 1000, send_nop);
}

/* Initialize, or re-initialize TN3270E state. */
static void
tn3270e_init(void)
{
    b8_zero(&e_funcs);
    b8_set_bit(&e_funcs, TN3270E_FUNC_BIND_IMAGE);
    b8_set_bit(&e_funcs, TN3270E_FUNC_RESPONSES);
    b8_set_bit(&e_funcs, TN3270E_FUNC_SYSREQ);
    if (appres.contention_resolution) {
	b8_set_bit(&e_funcs, TN3270E_FUNC_CONTENTION_RESOLUTION);
    }
    e_xmit_seq = 0;
    response_required = TN3270E_RSF_NO_RESPONSE;

    tn3270e_negotiated = 0;
    tn3270e_submode = E_UNBOUND;
    tn3270e_bound = 0;
}

static void
net_connected_complete(void)
{
    /* Done with TLS or proxy. */
    if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	host_in3270(CONNECTED_NVT);
    } else {
	change_cstate(TELNET_PENDING, "net_connected_complete");
    }
    st_changed(ST_SECURE, false);

    /* Check no-TELNET input mode. */
    if (ntim == NTIM_UNKNOWN) {
	ntim_t n = parse_ntim(appres.interactive.no_telnet_input_mode);

	if (n == NTIM_UNKNOWN) {
	    xs_warning("Unknown %s value '%s', defaulting to %s",
		    ResNoTelnetInputMode,
		    appres.interactive.no_telnet_input_mode,
		    ntim_name[NTIM_LINE]);
	    ntim = NTIM_LINE;
	} else {
	    ntim = n;
	}
    }

    /* set up telnet options */
    memset((char *)myopts, 0, sizeof(myopts));
    memset((char *)hisopts, 0, sizeof(hisopts));
    did_ne_send = false;
    deferred_will_ttype = false;
    tn3270e_init();
    need_tls_follows = false;
    telnet_state = TNS_DATA;
    ibptr = ibuf;

    /* clear statistics and flags */
    time(&ns_time);
    ns_brcvd = 0;
    ns_rrcvd = 0;
    ns_bsent = 0;
    ns_rsent = 0;
    syncing = 0;

    setup_lus();

    check_linemode(true);

    /* write out the passthru hostname and port nubmer */
    if (HOST_FLAG(PASSTHRU_HOST)) {
	char *buf;

	buf = Asprintf("%s %d\r\n", hostname, current_port);
	send(sock, buf, (int)strlen(buf), 0);
	Free(buf);
    }

    /* set up NOP transmission */
    if (appres.nop_seconds > 0 && !HOST_FLAG(NO_TELNET_HOST)) {
	nop_timeout_id = AddTimeOut(appres.nop_seconds * 1000, send_nop);
    }
}

static void
net_connected(void)
{
    bool data = false;

    /* Cancel the timeout. */
    if (connect_timeout_id != NULL_IOID) {
	RemoveTimeOut(connect_timeout_id);
	connect_timeout_id = NULL_IOID;
    }

    if (cstate != TLS_PENDING) {
	vtrace("Connected to %s, port %u.\n", hostname, current_port);
    }

    if (ut_getenv("BLOCKING_CONNECT") != NULL && non_blocking(true) < 0) {
	connect_error("non-blocking failure");
	host_disconnect(true);
	return;
    }

    if (proxy_pending) {
	proxy_negotiate_ret_t ret;

	/* Don't do this again. */
	proxy_pending = false;

	/* Negotiate with the proxy. */
	vtrace("Connected to proxy server %s, port %u.\n", proxy_host,
		proxy_port);

	change_cstate(PROXY_PENDING, "net_connected");

	ret = proxy_negotiate(sock, proxy_user, hostname, current_port, false);
	if (ret == PX_FAILURE) {
	    host_disconnect(true);
	    return;
	}
	if (ret == PX_WANTMORE) {
	    vtrace("Proxy needs more data\n");
	    return;
	}
    }

    /* Set up TLS. */
    if (HOST_FLAG(TLS_HOST) && sio != NULL && !secure_connection) {
	sio_negotiate_ret_t rv;
	char *session, *cert;

	change_cstate(TLS_PENDING, "net_connected");

	rv = sio_negotiate(sio, sock, hostname, &data);
	if (rv == SIG_FAILURE) {
	    /* No need to trace the error, it was already displayed. */
	    connect_error("%s", sio_last_error());
	    host_disconnect(true);
	    return;
	}
	if (rv == SIG_WANTMORE) {
	    vtrace("Need more TLS data\n");
	    return;
	}

	secure_connection = true;
	session = indent_s(sio_session_info(sio));
	cert = indent_s(sio_server_cert_info(sio));
	vtrace("Connection is now secure.\n"
		"Provider: %s\n"
		"Session:\n%s\nServer certificate:\n%s\n",
		sio_provider(), session, cert);
	Free(session);
	Free(cert);
	st_changed(ST_SECURE, true);

	/* Tell everyone else again. */
	host_connected();
    }

    net_connected_complete();

    if (data) {
	vtrace("Reading extra data after negotiation\n");
	net_input(INVALID_IOSRC, NULL_IOID);
    }
}

/*
 * net_password_continue
 * 	Called by the password GUI when a password has been entered.
 */
void
net_password_continue(const char *password)
{
    bool pending;
    iosrc_t s;

    if (!net_connect_pending) {
	/* Connection is gone. */
	return;
    }
    net_connect_pending = false;

    /* Try initializing sio again, with a new password. */
    if ((sio = sio_init_wrapper(password, HOST_FLAG(NO_VERIFY_CERT_HOST),
		    net_accept, &pending)) == NULL) {
	if (pending) {
	    /* Still pending, try again. */
	    net_connect_pending = true;
	}
	return;
    }

    /* Try connecting. */
    while (ha_ix < num_ha) {
	s = connect_to(ha_ix, (ha_ix == num_ha - 1), &pending);
	if (s != INVALID_IOSRC) {
	    host_newfd(s);
	    host_new_connection(pending);
	    break;
	}
	ha_ix++;
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
    host_connected();
    net_connected();
    remove_output();
}

/*
 * Clean up the pre-TCP-connected network state.
 */
static void
net_pre_close(void)
{
    SOCK_CLOSE(sock);
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

    /* We have no more interest in output buffer space. */
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
    sockaddr_46_t sa;
    socklen_t len = sizeof(sa);

    vtrace("Output possible\n");

    /*
     * Do a getpeername() to see if we are connected. If not, try a recv(),
     * which should fail with the connection error. I hope.
     */
    if (getpeername(sock, &sa.sa, &len) < 0) {
	/* Not connected. Find out why. */
	int e;
	socklen_t len = sizeof(e);

	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &e, &len) >= 0) {
	    vtrace("RCVD socket error %d (%s)\n", e, strerror(e));
	    net_pre_close();
	    while (++ha_ix < num_ha) {
		bool pending;
		iosrc_t s = connect_to(ha_ix, (ha_ix == num_ha - 1), &pending);

		if (s != INVALID_IOSRC) {
		    host_newfd(s);
		    host_new_connection(pending);
		    return;
		}
	    }
	    connect_errno(e, "%s%s, port %d",
			(proxy_type != PT_NONE)? "Proxy ": "",
			(proxy_type != PT_NONE)? proxy_host : hostname,
			(proxy_type != PT_NONE)? proxy_port : current_port);
	} else {
	    assert(0);
	}
	host_disconnect(true);
	return;
    }

    if (cstate == TCP_PENDING) {
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
net_disconnect(bool including_tls)
{
    if (including_tls && sio != NULL) {
	sio_close(sio);
	sio = NULL;
	secure_connection = false;
	st_changed(ST_SECURE, false);
    }
    if (CONNECTED) {
	shutdown(sock, 2);
    }

    net_pre_close();

    net_connect_pending = false;

    /* Cancel proxy. */
    if (proxy_type != PT_NONE) {
	proxy_close();
	proxy_type = PT_NONE;
	proxy_pending = false;
    }

    /* Cancel NOPs. */
    if (nop_timeout_id != NULL_IOID) {
	RemoveTimeOut(nop_timeout_id);
	nop_timeout_id = NULL_IOID;
    }

    /* We're not connected to an LU any more. */
    vstatus_lu(NULL);

    /* If we refused TLS and never entered 3270 mode, say so. */
    if (refused_tls && !any_host_data) {
	if (!appres.tls.starttls) {
	    connect_error("Host requested STARTTLS but STARTTLS disabled");
	} else if (nested_tls) {
	    connect_error("Host requested nested STARTTLS");
	} else {
	    connect_error("Host requested STARTTLS but TLS not supported");
	}
    }
    refused_tls = false;
    nested_tls = false;
    any_host_data = false;
    starttls_pending = NOT_CONNECTED;

    change_cstate(NOT_CONNECTED, "net_disconnect");
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
    register unsigned char *cp;
    int	nr;
    bool ignore_tls = false;

#if defined(_WIN32) /*[*/
    WSANETWORKEVENTS events;

    /*
     * Make the socket non-blocking.
     * Note that WSAEventSelect does this automatically (and won't allow
     * us to change it back to blocking), except on Wine.
     */
    if (sock != INVALID_SOCKET && non_blocking(true) < 0) {
	host_disconnect(true);
	return;
    }
#endif /*]*/
    if (sock == INVALID_SOCKET) {
	return;
    }

#if defined(_WIN32) /*[*/
    if (WSAEnumNetworkEvents(sock, sock_handle, &events) != 0) {
	popup_an_error("WSAEnumNetworkEvents failed: %s",
		win32_strerror(WSAGetLastError()));
	host_disconnect(true);
	return;
    }
    vtrace("net_input: NetworkEvents 0x%lx%s%s%s\n",
	    events.lNetworkEvents,
	    (events.lNetworkEvents & FD_CONNECT) ? " CONNECT": "",
	    (events.lNetworkEvents & FD_CLOSE) ? " CLOSE": "",
	    (events.lNetworkEvents & FD_READ) ? " READ": "");
    if (cstate == TCP_PENDING) {
	if (events.lNetworkEvents & FD_CONNECT) {
	    if (events.iErrorCode[FD_CONNECT_BIT] != 0) {
		if (ha_ix == num_ha - 1) {
		    connect_error("%s%s, port %d: %s",
			    (proxy_type != PT_NONE)? "Proxy ": "",
			    (proxy_type != PT_NONE)? proxy_host : hostname,
			    (proxy_type != PT_NONE)? proxy_port : current_port,
			    win32_strerror(events.iErrorCode[FD_CONNECT_BIT]));
		} else {
		    bool pending;
		    iosrc_t s;

		    net_pre_close();
		    while (++ha_ix < num_ha) {
			s = connect_to(ha_ix, (ha_ix == num_ha - 1), &pending);
			if (s != INVALID_IOSRC) {
			    host_newfd(s);
			    host_new_connection(pending);
			    return;
			}
		    }
		}
		host_disconnect(true);
		return;
	    } else {
		connection_complete();
		if (sock == INVALID_SOCKET) {
		    return;
		}
	    }
	} else {
	    vtrace("Spurious net_input call\n");
	    return;
	}
    }
#endif /*]*/

    if (cstate == PROXY_PENDING) {
	/* More proxy data available. */
	proxy_negotiate_ret_t ret = proxy_continue();

	if (ret == PX_WANTMORE) {
	    vtrace("Proxy needs more data\n");
	    return;
	}
	if (ret == PX_FAILURE) {
	    host_disconnect(true);
	    return;
	}

	if (HOST_FLAG(TLS_HOST) && sio != NULL && !secure_connection) {
	    /* Set up TLS tunnel after proxy connection. */
	    net_connected();
	    if (cstate == NOT_CONNECTED) {
		return;
	    }
	} else {
	    net_connected_complete();
	}
    }

    if (cstate == TLS_PENDING) {
	/* More TLS data available. Process it. */
	if (starttls_pending != NOT_CONNECTED) {
	    net_starttls_continue();
	} else {
	    net_connected();
	}
	return;
    }

    nvt_data = 0;

    vtrace("Reading host socket%s\n", secure_connection? " via TLS": "");

    if (secure_connection) {
	/*
	 * OpenSSL does not like getting refused connections
	 * when it hasn't done any I/O yet.  So peek ahead to
	 * see if it's worth getting it involved at all.
	 */
	if (cstate == TLS_PENDING &&
		(nr = recv(sock, (char *) netrbuf, 1, MSG_PEEK)) <= 0) {
	    ignore_tls = true;
	} else {
	    nr = sio_read(sio, (char *) netrbuf, BUFSZ);
	}
    } else {
#if defined(LOCAL_PROCESS) /*[*/
	if (local_process) {
	    nr = read(sock, (char *) netrbuf, BUFSZ);
	} else
#endif /*]*/
	{
	    nr = recv(sock, (char *) netrbuf, BUFSZ, 0);
	}
    }
    gettimeofday(&net_last_recv_ts, NULL);
    vtrace("Host socket read complete nr=%d\n", nr);
    if (nr < 0) {
	if ((secure_connection && nr == SIO_EWOULDBLOCK) ||
	    (!secure_connection && socket_errno() == SE_EWOULDBLOCK)) {
	    vtrace("EWOULDBLOCK\n");
	    return;
	}
	if (secure_connection && !ignore_tls) {
	    connect_error("%s", sio_last_error());
	    host_disconnect(true);
	    return;
	}
	if (cstate == TCP_PENDING && socket_errno() == SE_EAGAIN) {
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
	vtrace("RCVD socket error %d (%s)\n", socket_errno(),
		socket_strerror(socket_errno()));
	if (cstate == TCP_PENDING) {
	    if (ha_ix == num_ha - 1) {
		popup_a_sockerr("%s%s, port %d",
			(proxy_type != PT_NONE)? "Proxy ": "",
			(proxy_type != PT_NONE)? proxy_host : hostname,
			(proxy_type != PT_NONE)? proxy_port : current_port);
	    } else {
		bool pending;
		iosrc_t s;

		net_pre_close();
		while (++ha_ix < num_ha) {
		    s = connect_to(ha_ix, (ha_ix == num_ha - 1), &pending);
		    if (s != INVALID_IOSRC) {
			host_newfd(s);
			host_new_connection(pending);
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

    if (cstate == TCP_PENDING) {
	host_connected();
	net_connected();
	remove_output();
    }

    trace_netdata('<', netrbuf, nr);

    ns_brcvd += nr;
    stats_poke();
    for (cp = netrbuf; cp < (netrbuf + nr); cp++) {
#if defined(LOCAL_PROCESS) /*[*/
	if (local_process) {
	    /* More to do here, probably. */
	    if (cstate == TELNET_PENDING) {
		host_in3270(linemode? CONNECTED_NVT: CONNECTED_NVT_CHAR);
		hisopts[TELOPT_ECHO] = 1;
		check_linemode(false);
		kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
		vstatus_reset();
		ps_process();
	    }
	    nvt_process((unsigned int) *cp);
	} else {
#endif /*]*/
	    if (!telnet_fsm(*cp)) {
		ctlr_dbcs_postprocess();
		host_disconnect(true);
		return;
	    }
#if defined(LOCAL_PROCESS) /*[*/
	}
#endif /*]*/
    }

    if (IN_NVT) {
	ctlr_dbcs_postprocess();
    }
    net_nvt_break();

#if defined(_WIN32) /*[*/
    if (events.lNetworkEvents & FD_CLOSE) {
	vtrace("RCVD disconnect\n");
	host_disconnect(false);
    }
#endif /*]*/

    /* See if it's time to roll over the trace file. */
    trace_rollover_check();
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
    if ((n / 256) == IAC) {
	*(unsigned char *)buf++ = IAC;
    }
    *buf++ = (n / 256);
    n %= 256;
    if (n == IAC) {
	*(unsigned char *)buf++ = IAC;
    }
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

    snprintf(naws_msg, sizeof(naws_msg), "%c%c%c", IAC, SB, TELOPT_NAWS);
    naws_len += 3;
    naws_len += set16(naws_msg + naws_len, XMIT_COLS);
    naws_len += set16(naws_msg + naws_len, XMIT_ROWS);
    sprintf(naws_msg + naws_len, "%c%c", IAC, SE);
    naws_len += 2;
    net_rawout((unsigned char *)naws_msg, naws_len);
    vtrace("SENT %s NAWS %d %d %s\n", cmd(SB), XMIT_COLS, XMIT_ROWS, cmd(SE));
}


/* Advance 'try_lu' to the next desired LU name. */
static void
next_lu(void)
{
    if (curr_lu != NULL && (try_lu = *++curr_lu) == NULL) {
	curr_lu = NULL;
    }
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
	if (e) {
	    buf[i++] = e;
	} else {
	    buf[i++] = 0x3f; /* '?' */
	}
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
	if (!HOST_FLAG(NO_TELNET_HOST) && c == IAC) {
	    /* got a telnet command */
	    telnet_state = TNS_IAC;
	    net_nvt_break();
	    break;
	}
	if (cstate == TELNET_PENDING) {
	    /* now can assume NVT mode */
	    if (linemode) {
		linemode_buf_init();
	    }
	    host_in3270(linemode? CONNECTED_NVT: CONNECTED_NVT_CHAR);
	    kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
	    vstatus_reset();
	    ps_process();
	}
	if (IN_NVT && !IN_E) {
	    const char *space;

	    if (!nvt_data) {
		ntvtrace("<.. ");
		nvt_data = 4;
		nvt_any = false;
		nvt_last_cmd = false;
	    }
	    see_chr = ctl_see((int) c);
	    sl = strlen(see_chr);
	    if (sl > 1) {
		space = nvt_any? " ": "";
	    } else {
		space = nvt_last_cmd? " ": "";
	    }
	    nvt_data += strlen(space) + sl;
	    if (nvt_data >= TRACELINE) {
		ntvtrace(" ...\n... ");
		nvt_data = 4 + sl;
	    }
	    ntvtrace("%s%s", space, see_chr);
	    nvt_last_cmd = sl > 1;
	    nvt_any = true;
	    if (!syncing) {
		if (((linemode && appres.linemode.onlcr) ||
		     (!linemode && charmode_onlcr))
			&& c == '\n') {
		    nvt_process((unsigned int) '\r');
		}
		nvt_process((unsigned int) c);
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
		const char *space;

		if (!nvt_data) {
		    ntvtrace("<.. ");
		    nvt_data = 4;
		    nvt_any = false;
		    nvt_last_cmd = false;
		}
		see_chr = ctl_see((int) c);
		sl = strlen(see_chr);
		if (sl > 1) {
		    space = nvt_any? " ": "";
		} else {
		    space = nvt_last_cmd? " ": "";
		}
		nvt_data += strlen(space) +  sl;
		if (nvt_data >= TRACELINE) {
		    ntvtrace(" ...\n ...");
		    nvt_data = 4 + sl;
		    nvt_any = false;
		    nvt_last_cmd = false;
		}
		ntvtrace("%s%s", space, see_chr);
		nvt_last_cmd = sl > 1;
		nvt_any = true;
		nvt_process((unsigned int) c);
	    } else {
		store3270in(c);
	    }
	    telnet_state = TNS_DATA;
	    break;
	case EOR:	/* eor, process accumulated input */
	    if (IN_3270 || (IN_E && tn3270e_negotiated)) {
		ns_rrcvd++;
		stats_poke();
		if (process_eor()) {
		    return false;
		}
	    } else {
		popup_an_error("EOR received when not in 3270 mode, ignored");
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
	} else if (c == TELOPT_TN3270E && myopts[c]) {
	    /*
	     * Ugly hack for hosts that send WONT TN3270E instead of
	     * DONT TN3270E.
	     */
	    myopts[c] = 0;
	    wont_opt[2] = c;
	    net_rawout(wont_opt, sizeof(wont_opt));
	    vtrace("SENT %s %s\n", cmd(WONT), opt(c));
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
	    if (c == TELOPT_STARTTLS &&
		    (!sio_supported() ||
		     !appres.tls.starttls ||
		     secure_connection)) {
		refused_tls = true;
		if (secure_connection) {
		    nested_tls = true;
		}
		goto wont;
	    }
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
		    connect_error("Cannot connect to specified LU");
		    return false;
		}

		tt_len = strlen(termtype);
		if (try_lu != NULL && *try_lu) {
		    tt_len += strlen(try_lu) + 1;
		    connected_lu = try_lu;
		} else {
		    connected_lu = NULL;
		}

		tb_len = 4 + tt_len + 2;
		tt_out = Malloc(tb_len + 1);
		sprintf(tt_out, "%c%c%c%c%s%s%s%c%c",
			IAC, SB, TELOPT_TTYPE, TELQUAL_IS,
			force_ascii(termtype),
			(try_lu != NULL && *try_lu)? "@": "",
			(try_lu != NULL && *try_lu)?  force_ascii(try_lu) : "",
			IAC, SE);
		net_hexnvt_out_framed((unsigned char *)tt_out, tb_len, true);
		Free(tt_out);

		vstatus_lu(connected_lu);

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
	    } else if (sio_supported() &&
		    sio != NULL &&
		    need_tls_follows &&
		    myopts[TELOPT_STARTTLS] &&
		    sbbuf[0] == TELOPT_STARTTLS) {
		continue_tls(sbbuf, (int)(sbptr - sbbuf));
	    } else if (sbbuf[0] == TELOPT_NEW_ENVIRON &&
		    sbbuf[1] == TELQUAL_SEND && appres.new_environ) {
		unsigned char *reply_buf;
		size_t reply_buflen;
		char *trace_in;
		char *trace_out;

		if (!telnet_new_environ(sbbuf + 2, (sbptr - sbbuf - 3),
			    &reply_buf, &reply_buflen, &trace_in,
			    &trace_out)) {
		    vtrace("%s %s [error]\n", opt(sbbuf[0]),
			    telquals[sbbuf[1]]);
		} else {
		    vtrace("%s\n", trace_in);
		    Free(trace_in);
		    net_rawout(reply_buf, reply_buflen);
		    Free(reply_buf);
		    vtrace("SENT %s\n", trace_out);
		    Free(trace_out);
		}

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

/* Create a 3270 termtype string. */
static char *
create_3270_termtype(bool force_3278)
{
    char *base_type = create_model(model_num,
	    !force_3278 && mode3279 && (appres.wrong_terminal_name || model_num < 4));
    char *ret = Asprintf("IBM-%s%s",
	    base_type,
	    (!appres.extended_data_stream || HOST_FLAG(STD_DS_HOST))? "": "-E");

    Free(base_type);
    return ret;
}

/* Send a TN3270E terminal type request. */
static void
tn3270e_request(void)
{
    size_t tt_len, tb_len;
    char *tt_out;
    char *t;
    char *xtn;

    /* Always use 3278, per the RFC. */
    xtn = create_3270_termtype(true);
    tt_len = strlen(termtype);
    if (try_lu != NULL && *try_lu) {
	tt_len += strlen(try_lu) + 1;
    }

    tb_len = 5 + tt_len + 2;
    tt_out = Malloc(tb_len + 1);
    t = tt_out;
    t += sprintf(tt_out, "%c%c%c%c%c%s",
	IAC, SB, TELOPT_TN3270E, TN3270E_OP_DEVICE_TYPE, TN3270E_OP_REQUEST,
	force_ascii(xtn));

    if (try_lu != NULL && *try_lu) {
	t += sprintf(t, "%c%s", TN3270E_OP_CONNECT, force_ascii(try_lu));
    }

    sprintf(t, "%c%c", IAC, SE);

    net_hexnvt_out_framed((unsigned char *)tt_out, tb_len, true);
    Free(tt_out);

    vtrace("SENT %s %s DEVICE-TYPE REQUEST %s%s%s %s\n",
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
    tn3270e_init();
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
	if (sbbuf[sblen] == SE) {
	    break;
	}
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
		   sbbuf[3+tnlen] != TN3270E_OP_CONNECT) {
		tnlen++;
	    }
	    snlen = 0;
	    if (sbbuf[3+tnlen] == TN3270E_OP_CONNECT) {
		while(sbbuf[3+tnlen+1+snlen] != SE) {
		    snlen++;
		}
	    }

	    /* Remember the LU. */
	    if (tnlen) {
		if (tnlen > LU_MAX) {
		    tnlen = LU_MAX;
		}
		strncpy(reported_type, (char *)&sbbuf[3], tnlen);
		reported_type[tnlen] = '\0';
		force_local(reported_type);
		connected_type = reported_type;
	    }
	    if (snlen) {
		if (snlen > LU_MAX) {
		    snlen = LU_MAX;
		}
		strncpy(reported_lu, (char *)&sbbuf[3+tnlen+1], snlen);
		reported_lu[snlen] = '\0';
		force_local(reported_lu);
		connected_lu = reported_lu;
	    }

	    vtrace("IS %s CONNECT %s SE\n", tnlen? connected_type: "",
		    snlen? connected_lu: "");

	    if (snlen) {
		vstatus_lu(connected_lu);
	    }

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
		vtrace("TN3270E option negotiation complete.\n");
		check_in3270();
	    } else {
		/*
		 * They want us to do something we can't.
		 * Request the common subset.
		 */
		b8_and(&e_funcs, &e_funcs, &e_rcvd);
		tn3270e_subneg_send(TN3270E_OP_REQUEST, &e_funcs);
	    }
	    break;

	case TN3270E_OP_IS:
	    /* They accept our last request, or a subset thereof. */
	    vtrace("IS %s SE\n", tn3270e_function_names(sbbuf+3, sblen-3));
	    tn3270e_fdecode(sbbuf+3, sblen-3, &e_rcvd);
	    if (b8_none_added(&e_funcs, &e_rcvd)) {
		/* They want what we want, or less.  Done. */
		b8_copy(&e_funcs, &e_rcvd);
	    } else {
		/*
		 * They've added something. Abandon TN3270E,
		 * they're brain dead.
		 */
		backoff_tn3270e("Host illegally added function(s)");
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

    if (!len) {
	return("(none)");
    }
    for (i = 0; i < len; i++) {
	s += sprintf(s, "%s%s", (s == text_buf)? "": " ", fnn(buf[i]));
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

    if (b8_is_zero(&e_funcs) || !IN_E) {
	return NULL;
    }
    for (i = 0; i < MX8; i++) {
	if (b8_bit_is_set(&e_funcs, i)) {
	    s += sprintf(s, "%s%s", (s == text_buf)? "": " ", fnn(i));
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
    memcpy(proto_buf, functions_req, 4);
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
    if (!(c & 0x80)) {
	return 0;
    }
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
    if (plu_name == NULL) {
	plu_name = Malloc(mb_max_len(BIND_PLU_NAME_MAX + 1));
    }
    memset(plu_name, '\0', mb_max_len(BIND_PLU_NAME_MAX + 1));
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
    if (buflen > BIND_OFF_MAXRU_SEC) {
	maxru_sec = maxru(buf[BIND_OFF_MAXRU_SEC]);
    }
    if (buflen > BIND_OFF_MAXRU_PRI) {
	maxru_pri = maxru(buf[BIND_OFF_MAXRU_PRI]);
    }

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
	    bind_state = BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
	    break;
	case 0x03:
	    bind_rd = MODEL_2_ROWS;
	    bind_cd = MODEL_2_COLS;
	    bind_ra = maxROWS;
	    bind_ca = maxCOLS;
	    bind_state = BIND_DIMS_PRESENT | BIND_DIMS_VALID;
	    break;
	case 0x7e:
	    bind_rd = buf[BIND_OFF_RD];
	    bind_cd = buf[BIND_OFF_CD];
	    bind_ra = buf[BIND_OFF_RD];
	    bind_ca = buf[BIND_OFF_CD];
	    bind_state = BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
	    break;
	case 0x7f:
	    bind_rd = buf[BIND_OFF_RD];
	    bind_cd = buf[BIND_OFF_CD];
	    bind_ra = buf[BIND_OFF_RA];
	    bind_ca = buf[BIND_OFF_CA];
	    bind_state = BIND_DIMS_PRESENT | BIND_DIMS_ALT | BIND_DIMS_VALID;
	    break;
	default:
	    bind_state = 0;
	    break;
	}
    }

    /* Validate and implement the screen size. */
    if (appres.bind_limit && (bind_state & BIND_DIMS_PRESENT)) {
	if (bind_rd > maxROWS || bind_cd > maxCOLS) {
	    popup_an_error("Ignoring invalid BIND image screen "
		    "size parameters:\n"
		    " BIND Default Rows-Cols %ux%u > Maximum %ux%u",
		    bind_rd, bind_cd, maxROWS, maxCOLS);
	    bind_state &= ~BIND_DIMS_VALID;
	} else if (bind_rd < MODEL_2_ROWS || bind_cd < MODEL_2_COLS) {
	    popup_an_error("Ignoring invalid BIND image screen "
		    "size parameters:\n"
		    " BIND Default Rows-Cols %ux%u < Minimum %ux%u",
		    bind_rd, bind_cd, MODEL_2_ROWS, MODEL_2_COLS);
	    bind_state &= ~BIND_DIMS_VALID;
	} else if (bind_ra > maxROWS || bind_ca > maxCOLS) {
	    popup_an_error("Ignoring invalid BIND image screen "
		    "size parameters:\n"
		    " BIND Alternate Rows-Cols %ux%u > Maximum %ux%u",
		    bind_ra, bind_ca, maxROWS, maxCOLS);
	    bind_state &= ~BIND_DIMS_VALID;
	} else if (bind_ra < MODEL_2_ROWS || bind_ca < MODEL_2_COLS) {
	    popup_an_error("Ignoring invalid BIND image screen "
		    "size parameters:\n"
		    " BIND Alternate Rows-Cols %ux%u < Minimum %ux%u",
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
	if (namelen > BIND_PLU_NAME_MAX) {
	    namelen = BIND_PLU_NAME_MAX;
	}
	if ((namelen > 0) && (buflen > BIND_OFF_PLU_NAME + namelen)) {
#if defined(EBCDIC_HOST) /*[*/
	    memcpy(plu_name, &buf[BIND_OFF_PLU_NAME], namelen);
	    plu_name[namelen] = '\0';
#else /*][*/
	    size_t i;

	    for (i = 0; i < namelen; i++) {
		size_t nx;

		nx = ebcdic_to_multibyte(buf[BIND_OFF_PLU_NAME + i],
			plu_name + dest_ix, mb_max_len(1));
		if (nx > 1) {
		    dest_ix += nx - 1;
		}
	    }
#endif /*]*/
	    vstatus_lu(plu_name);
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
	return txAsprintf("unknown X'%02x'", r);
    }
}

/* Returns the string representation of a request flag, given the data type. */
static char *
e_rq(unsigned char data_type, unsigned char request_flag)
{
    static struct {
	unsigned char flag;
	const char *name;
    } req_flag[] = {
	{ TN3270E_RQF_SEND_DATA, "SEND-DATA" },
	{ TN3270E_RQF_KEYBOARD_RESTORE, "KEYBOARD-RESTORE" },
	{ TN3270E_RQF_SIGNAL, "SIGNAL" },
	{ 0, NULL }
    };
    varbuf_t r;
    int i;
    char *sep = " ";

    if (data_type == TN3270E_DT_REQUEST) {
	return (request_flag == TN3270E_RQF_ERR_COND_CLEARED)?
	    " ERR-COND-CLEARED": txAsprintf("%02x", request_flag);
    }

    vb_init(&r);
    for (i = 0; req_flag[i].name != NULL; i++) {
	if (request_flag & req_flag[i].flag) {
	    vb_appendf(&r, "%s%s", sep, req_flag[i].name);
	    request_flag &= ~req_flag[i].flag;
	    sep = "|";
	}
    }
    if (request_flag != 0) {
	vb_appendf(&r, "%s%02x", sep, request_flag);
    }
    return txdFree(vb_consume(&r));
}

static int
process_eor(void)
{
    if (syncing || !(ibptr - ibuf)) {
	return(0);
    }

    if (IN_E) {
	tn3270e_header *h = (tn3270e_header *)ibuf;
	unsigned char *s;
	enum pds rv;
	bool bid_success;

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
	    rv = process_ds(ibuf + EH_SIZE, (ibptr - ibuf) - EH_SIZE,
		    (h->request_flag & TN3270E_RQF_KEYBOARD_RESTORE) != 0);
	    if (rv < 0 && response_required != TN3270E_RSF_NO_RESPONSE) {
		tn3270e_nak(rv);
	    }
	    else if (rv == PDS_OKAY_NO_OUTPUT &&
		    response_required == TN3270E_RSF_ALWAYS_RESPONSE) {
		tn3270e_ack();
	    }
	    response_required = TN3270E_RSF_NO_RESPONSE;
	    if (b8_bit_is_set(&e_funcs, TN3270E_FUNC_CONTENTION_RESOLUTION)) {
		if (h->request_flag & TN3270E_RQF_SEND_DATA) {
		    /* Okay to send data. */
		    kybd_send_data();
		} else {
		    /* Not okay to send data. */
		    (void) kybd_bid(false);
		}
	    }
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
		trace_ds("< UNBIND %s\n", unbind_reason(ibuf[EH_SIZE]));
	    }
	    tn3270e_bound = 0;
	    /* Go back to the connected LU. */
	    vstatus_lu(connected_lu);
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
	    trace_envt_in(ibuf + EH_SIZE, ibptr - (ibuf + EH_SIZE));
	    for (s = ibuf + EH_SIZE; s < ibptr; s++) {
		nvt_process((unsigned int)*s);
	    }
	    if (h->response_flag == TN3270E_RSF_ALWAYS_RESPONSE) {
		tn3270e_ack();
	    }
	    return 0;
	case TN3270E_DT_SSCP_LU_DATA:
	    tn3270e_submode = E_SSCP;
	    check_in3270();
	    ctlr_write_sscp_lu(ibuf + EH_SIZE, (ibptr - ibuf) - EH_SIZE);
	    return 0;
	case TN3270E_DT_BID:
	    if (!b8_bit_is_set(&e_funcs, TN3270E_FUNC_CONTENTION_RESOLUTION)) {
		return 0;
	    }
	    bid_success = kybd_bid((h->request_flag & TN3270E_RQF_SIGNAL) != 0);
	    if (h->response_flag != TN3270E_RSF_NO_RESPONSE) {
		if (bid_success) {
		    tn3270e_ack();
		} else {
		    tn3270e_nak(PDS_BAD_CMD);
		}
	    }
	    return 0;
	default:
	    /* Should do something more extraordinary here. */
	    return 0;
	}
    } else {
	process_ds(ibuf, ibptr - ibuf, false);
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
	bool any = false;
	bool last_cmd = false;

	ntvtrace(">.. ");
	for (i = 0; i < len; i++) {
	    char *s = ctl_see((int)*(buf + i));

	    if (strlen(s) > 1) {
		/* Control character. */
		ntvtrace("%s%s", any? " ": "", s);
		last_cmd = true;
	    } else {
		/* Not a control character. */
		ntvtrace("%s%s", last_cmd? " ": "", s);
		last_cmd = false;
	    }
	    any = true;
	}
	ntvtrace("\n");
    }

    if (IN_E_NVT) {
	space3270out(len);
	memcpy(obuf, buf, len);
	obptr = obuf + len;
	net_output();
    } else {
	net_rawout((unsigned const char *)buf, len);
    }
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
    int nw;

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
# define n2w len
#endif
	if (secure_connection) {
	    nw = sio_write(sio, (const char *) buf, (int)n2w);
	} else
#if defined(LOCAL_PROCESS) /*[*/
	if (local_process) {
	    nw = write(sock, (const char *) buf, (int)n2w);
	} else
#endif /*]*/
	{
	    nw = send(sock, (const char *) buf, (int)n2w, 0);
	}
	if (nw < 0) {
	    if (secure_connection) {
		connect_error("%s", sio_last_error());
		host_disconnect(false);
		return;
	    }
	    vtrace("RCVD socket error %d (%s)\n", socket_errno(),
		    socket_strerror(socket_errno()));
	    if (socket_errno() == SE_EPIPE || socket_errno() == SE_ECONNRESET) {
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
	stats_poke();
	len -= nw;
	buf += nw;
    bot:
#if defined(OMTU) /*[*/
	if (pause) {
	    sleep(1);
	}
#endif /*]*/
    	;
    }
}

/*
 * net_hexnvt_out_framed
 *	Send uncontrolled user data to the host in NVT mode, performing IAC
 *	and CR quoting as necessary.
 */
static void
net_hexnvt_out_framed(unsigned char *buf, size_t len, bool framed)
{
    unsigned char *tbuf;
    unsigned char *xbuf;
    bool first = true;

    if (!len) {
	return;
    }

    if (HOST_FLAG(NO_TELNET_HOST)) {
	net_rawout(buf, len);
	return;
    }

    /* Expand it. */
    tbuf = xbuf = (unsigned char *)Malloc(2*len);
    while (len) {
	unsigned char c = *buf++;

	*tbuf++ = c;
	len--;
	if (framed && (first || len == 1)) {
	    /* Don't quote initial IAC or trailing IAC SE. */
	    first = false;
	    continue;
	}
	if (c == IAC) {
	    *tbuf++ = IAC;
	} else if (c == '\r' && (!len || *buf != '\n')) {
	    *tbuf++ = '\0';
	}
	first = false;
    }

    /* Send it to the host. */
    net_rawout(xbuf, tbuf - xbuf);
    Free(xbuf);
}

/*
 * net_hexnvt_out
 *	Send uncontrolled user data to the host in NVT mode, performing IAC
 *	and CR quoting as necessary.
 */
void
net_hexnvt_out(unsigned char *buf, size_t len)
{
    net_hexnvt_out_framed(buf, len, false);
}

/*
 * check_in3270
 *	Check for switches between NVT, SSCP-LU and 3270 modes.
 */
static void
check_in3270(void)
{
    enum cstate new_cstate = NOT_CONNECTED;

    if (myopts[TELOPT_TN3270E]) {
	if (!tn3270e_negotiated) {
	    new_cstate = CONNECTED_UNBOUND;
	} else switch (tn3270e_submode) {
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
    } else if (cstate == TELNET_PENDING) {
	/* Nothing has happened, yet. */
	return;
    } else if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	new_cstate = linemode? CONNECTED_NVT: CONNECTED_NVT_CHAR;
    } else {
	new_cstate = TELNET_PENDING;
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
	if (new_cstate >= TELNET_PENDING && !ibuf_size) {
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
	    vtrace("Aborting TN3270E: negotiated off\n");
	    tn3270e_init();
	}
	vtrace("Now operating in %s mode.\n", state_name[new_cstate]);
	if (FULL_SESSION) {
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

    if (obuf_size) {
	nc = obptr - obuf;
    }

    while ((nc + n + EH_SIZE) > (obuf_size + more)) {
	more += BUFSIZ;
    }

    if (more) {
	obuf_size += more;
	obuf_base = (unsigned char *)Realloc((char *)obuf_base, obuf_size);
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
    bool wasline = linemode;

    if (local_process) {
	linemode = false;
    } else if (HOST_FLAG(NO_TELNET_HOST)) {
	linemode = (ntim == NTIM_LINE);
    } else {
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
    }

    if (init || linemode != wasline) {
	if (cstate == CONNECTED_NVT || cstate == CONNECTED_NVT_CHAR) {
	    host_in3270(linemode? CONNECTED_NVT: CONNECTED_NVT_CHAR);
	}
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

    /* Set output newline expansion. */
    charmode_onlcr = !local_process && HOST_FLAG(NO_TELNET_HOST) &&
	ntim == NTIM_CHARACTER_CRLF;
}

/*
 * nnn
 *	Expands a number to a character string, for displaying unknown telnet
 *	commands and options.
 */
static const char *
nnn(int c)
{
    return txAsprintf("%d", c);
}

/*
 * cmd
 *	Expands a TELNET command into a character string.
 */
const char *
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
const char *
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

    if (!toggled(TRACING)) {
	    return;
    }
    for (offset = 0; offset < len; offset++) {
	if (!(offset % LINEDUMP_MAX)) {
	    ntvtrace("%s%c 0x%-3x ", (offset? "\n": ""), direction,
		    (unsigned)offset);
	}
	ntvtrace("%02x", buf[offset]);
    }
    ntvtrace("\n");
}

/* Trace incoming E-mode NVT data. */
static void
trace_envt_in(unsigned const char *buf, size_t len)
{
    size_t count;
    bool any = false;
    bool last_cmd = false;

    ntvtrace("<.. ");
    count = 4;
    while (len--) {
	const char *see_chr = ctl_see((int)*buf++);
	size_t sl;
	char *space;

	sl = strlen(see_chr);
	if (sl > 1) {
	    space = any? " ": "";
	} else {
	    space = last_cmd? " ": "";
	}
	count += strlen(space) + sl;
	if (count >= TRACELINE) {
	    ntvtrace(" ...\n... ");
	    count = 4 + sl;
	    any = false;
	    last_cmd = false;
	}
	ntvtrace("%s%s", space, see_chr);
	last_cmd = sl > 1;
	any = true;
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

#define BSTART	((IN_TN3270E || IN_SSCP || IN_E_NVT)? obuf_base: obuf)

    /* Set the TN3720E header. */
    if (IN_TN3270E || IN_SSCP || IN_E_NVT) {
	tn3270e_header *h = (tn3270e_header *)obuf_base;

	/* Check for sending a TN3270E response. */
	if (response_required == TN3270E_RSF_ALWAYS_RESPONSE) {
	    tn3270e_ack();
	    response_required = TN3270E_RSF_NO_RESPONSE;
	}

	/* Set the outbound TN3270E header. */
	h->data_type = IN_TN3270E? TN3270E_DT_3270_DATA:
	    ((IN_E_NVT)? TN3270E_DT_NVT_DATA:
	     TN3270E_DT_SSCP_LU_DATA);
	h->request_flag = 0;
	h->response_flag = 0;
	h->seq_number[0] = (e_xmit_seq >> 8) & 0xff;
	h->seq_number[1] = e_xmit_seq & 0xff;

	vtrace("SENT TN3270E(%s NO-RESPONSE %u)\n",
		IN_TN3270E? "3270-DATA":
		((IN_E_NVT)? "NVT-DATA": "SSCP-LU-DATA"),
		e_xmit_seq);
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
    stats_poke();
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
    rsp_buf[rsp_len++] = 0;			    /* request_flag */
    rsp_buf[rsp_len++] = TN3270E_RSF_POSITIVE_RESPONSE; /* response_flag */	
    rsp_buf[rsp_len++] = h_in->seq_number[0];	    /* seq_number[0] */
    if (h_in->seq_number[0] == IAC) {
	rsp_buf[rsp_len++] = IAC;
    }
    rsp_buf[rsp_len++] = h_in->seq_number[1];	    /* seq_number[1] */
    if (h_in->seq_number[1] == IAC) {
	rsp_buf[rsp_len++] = IAC;
    }
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
    rsp_buf[rsp_len++] = 0;			    /* request_flag */
    rsp_buf[rsp_len++] = TN3270E_RSF_NEGATIVE_RESPONSE; /* response_flag */
    rsp_buf[rsp_len++] = h_in->seq_number[0];	    /* seq_number[0] */
    if (h_in->seq_number[0] == IAC) {
	rsp_buf[rsp_len++] = IAC;
    }
    rsp_buf[rsp_len++] = h_in->seq_number[1];	    /* seq_number[1] */
    if (h_in->seq_number[1] == IAC) {
	rsp_buf[rsp_len++] = IAC;
    }
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

    if (!IN_E || tn3270e_submode == E_UNBOUND) {
	return false;
    }

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
    if (c == '\r' && !local_process) {
	if (HOST_FLAG(NO_TELNET_HOST) && ntim == NTIM_CHARACTER_CRLF) {
	    net_cookout("\n", 1);
	    return;
	}
	if (!linemode) {
	    /* CR must be quoted */
	    net_cookout("\r\0", 2);
	    return;
	}
    }
    net_cookout(&c, 1);
}

/*
 * net_sends
 *	Send a null-terminated string of user data in NVT mode.
 */
void
net_sends(const char *s)
{
    if (strlen(s) == 1 && *s == '\r' && !local_process) {
	if (HOST_FLAG(NO_TELNET_HOST) && ntim == NTIM_CHARACTER_CRLF) {
	    net_cookout("\n", 1);
	    return;
	}
	if (!linemode) {
	    /* CR must be quoted */
	    net_cookout("\r\0", 2);
	    return;
	}
    }

    net_cookout(s, strlen(s));
}

/*
 * External entry points to negotiate line or character mode.
 */
void
net_linemode(void)
{
    if (!IN_NVT) {
	return;
    }
    if (!HOST_FLAG(NO_TELNET_HOST)) {
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
    } else {
	hisopts[TELOPT_ECHO] = 0;
	hisopts[TELOPT_SGA] = 0;
	check_linemode(false);
    }
}

void
net_charmode(void)
{
    if (!IN_NVT) {
	return;
    }
    if (!HOST_FLAG(NO_TELNET_HOST)) {
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
    } else {
	hisopts[TELOPT_ECHO] = 1;
	hisopts[TELOPT_SGA] = 1;
	check_linemode(false);
    }
}

/*
 * net_break
 *	Send telnet break, which is used to implement 3270 ATTN.
 *
 */
void
net_break(char c)
{
    if (!HOST_FLAG(NO_TELNET_HOST)) {
	static unsigned char buf[] = { IAC, BREAK };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	vtrace("SENT BREAK\n");
    } else if (c != '\0') {
	net_rawout((unsigned char *)&c, 1);
    }
}

/*
 * net_interrupt
 *	Send telnet IP.
 *
 */
void
net_interrupt(char c)
{
    if (!HOST_FLAG(NO_TELNET_HOST)) {
	static unsigned char buf[] = { IAC, IP };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	vtrace("SENT IP\n");
    } else if (c != '\0') {
	net_rawout((unsigned char *)&c, 1);
    }
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
	switch (tn3270e_submode) {
	case E_UNBOUND:
	case E_NVT:
	    break;
	case E_SSCP:
	case E_3270:
	    net_rawout(buf, sizeof(buf));
	    vtrace("SENT AO\n");
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

    if (!CONNECTED) {
	return false;
    }

    obptr = obuf;

    /* Do TTYPE first. */
    if (myopts[TELOPT_TTYPE]) {
	unsigned j;

	space3270out(sizeof(ttype_str));
	for (j = 0; j < sizeof(ttype_str); j++) {
	    *obptr++ = ttype_str[j];
	}
    }

    /* Do the other options. */
    for (i = 0; i < N_OPTS; i++) {
	space3270out(6);
	if (i == TELOPT_TTYPE) {
	    continue;
	}
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
	    memcpy(obptr, connected_type, strlen(connected_type));
	    obptr += strlen(connected_type);
	}
	if (connected_lu != NULL) {
	    *obptr++ = TN3270E_OP_CONNECT;
	    memcpy(obptr, connected_lu, strlen(connected_lu));
	    obptr += strlen(connected_lu);
	}
	*obptr++ = IAC;
	*obptr++ = SE;

	space3270out(38);
	memcpy(obptr, functions_req, 4);
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
		if (bind_image[i] == 0xff) {
		    *obptr++ = 0xff;
		}
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
	connect_errno(errno, "fcntl(F_GETFL)");
	return -1;
    }
    if (on) {
	f |= O_NDELAY;
    } else {
	f &= ~O_NDELAY;
    }
    if (fcntl(sock, F_SETFL, f) < 0) {
	connect_errno(errno, "fcntl(F_SETFL)");
	return -1;
    }
# endif /*]*/
#endif /*]*/
    return 0;
}

/* Continue TLS negotiation in response to a STARTTLS. */
static void
net_starttls_continue(void)
{
    sio_negotiate_ret_t ret;
    bool data = false;
    char *session, *cert;

    /* Negotiate the session. */
    ret = sio_negotiate(sio, sock, hostname, &data);
    if (ret == SIG_FAILURE) {
	connect_error("%s", sio_last_error());
	host_disconnect(true);
	return;
    }
    if (ret == SIG_WANTMORE) {
	vtrace("Need more TLS data\n");
	if (starttls_pending == NOT_CONNECTED) {
	    starttls_pending = cstate;
	    change_cstate(TLS_PENDING, "net_starttls_continue");
	}
	return;
    }

    secure_connection = true;

    /* Success. */
    session = indent_s(sio_session_info(sio));
    cert = indent_s(sio_server_cert_info(sio));
    vtrace("TLS negotiated connection complete. "
	    "Connection is now secure.\n"
	    "Provider: %s\n"
	    "Session:\n%s\nServer certificate:\n%s\n",
	    sio_provider(), session, cert);
    Free(session);
    Free(cert);
    st_changed(ST_SECURE, true);

    if (starttls_pending == TELNET_PENDING) {
	/*
	 * TLS negotiation happened before 3270 negotiation (which is the usual
	 * case).  Tell the world that we are (still) connected, now in secure
	 * mode.
	 */
	host_connected();
    } else {
	/*
	 * The host negotiated TLS while in some other state. Try to restore
	 * it.
	 */
	if (cHALF_CONNECTED(starttls_pending)) {
	    st_changed(ST_NEGOTIATING, true);
	} else {
	    st_changed(ST_3270_MODE, true);
	}
    }
    starttls_pending = NOT_CONNECTED;

    if (data) {
	/* Got extra data with the negotiation. */
	vtrace("Reading extra data after negotiation\n");
	net_input(INVALID_IOSRC, NULL_IOID);
    }
}

/* Process a STARTTLS subnegotiation. */
static void
continue_tls(unsigned char *sbbuf, int len)
{
    /* Whatever happens, we're not expecting another SB STARTTLS. */
    need_tls_follows = false;

    /* Make sure the option is FOLLOWS. */
    if (len < 2 || sbbuf[1] != TLS_FOLLOWS) {
	/* Trace the junk. */
	vtrace("%s ? %s\n", opt(TELOPT_STARTTLS), cmd(SE));
	connect_error("TLS negotiation failure");
	host_disconnect(true);
	return;
    }

    /* Trace what we got. */
    vtrace("%s FOLLOWS %s\n", opt(TELOPT_STARTTLS), cmd(SE));

    /* Negotiate. */
    net_starttls_continue();
}

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
    return state_name[cstate];
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
	    return txAsprintf("process %s", hostname);
	}
#endif /*]*/
	return txAsprintf("host %s %u", hostname, current_port);
    } else {
	return "";
    }
}

/* Return the TLS state. */
const char *
net_query_tls(void)
{
    static char *not_secure = "not secure";

    if (CONNECTED) {
	if (!secure_connection) {
	    return not_secure;
	}
	return txAsprintf("secure %s",
		net_secure_unverified()? "host-unverified": "host-verified");
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
const char *
net_proxy_type(void)
{
    if (proxy_type != PT_NONE) {
	return proxy_type_name(proxy_type);
    } else {
	return NULL;
    }
}

/* Return the current proxy user, or NULL. */
const char *
net_proxy_user(void)
{
    if (proxy_type != PT_NONE) {
	return proxy_user;
    } else {
	return NULL;
    }
}

/* Return the current proxy host, or NULL. */
const char *
net_proxy_host(void)
{
    if (proxy_type != PT_NONE) {
	return proxy_host;
    } else {
	return NULL;
    }
}

/* Return the current proxy port, or NULL. */
const char *
net_proxy_port(void)
{
    if (proxy_type != PT_NONE) {
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

/* Format TELNET options. */
static const char *
net_opts(unsigned char opts[])
{
    int i;
    char *ret = NULL;
    size_t sl = 0;

    for (i = 0; i < N_OPTS; i++) {
	if (opts[i]) {
	    const char *o = opt(i);

	    ret = (char *)Realloc(ret, sl + 1 + strlen(o) + 1);
	    if (sl) {
		ret[sl++] = ' ';
	    }
	    strcpy(ret + sl, o);
	    sl += strlen(o);
	}
    }
    return txdFree(ret);
}

/* Return my TELNET options. */
const char *
net_myopts(void)
{
    return net_opts(myopts);
}

/* Return his TELNET options. */
const char *
net_hisopts(void)
{
    return net_opts(hisopts);
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
    char *previous = NewString(termtype);

    if (appres.termname) {
	termtype = appres.termname;
    } else if (appres.nvt_mode || HOST_FLAG(ANSI_HOST)) {
	termtype = mode3279? "xterm-color": "xterm";
    } else if (ov_rows || ov_cols) {
	termtype = "IBM-DYNAMIC";
    } else {
	char *ttype = create_3270_termtype(false);

	strncpy(ttype_tmpval, ttype, sizeof(ttype_tmpval) - 1);
	ttype_tmpval[sizeof(ttype_tmpval) - 1] = '\0';
	Free(ttype);
	termtype = ttype_tmpval;
    }

    if (previous != NULL) {
	if (strcmp(termtype, previous)) {
	    st_changed(ST_TERMINAL_NAME, true);
	}
	Free(previous);
    }
}

/* Handle an ST_REMODEL indication. */
static void
net_remodel(bool ignored _is_unused)
{
    net_set_default_termtype();
}

bool
net_secure_unverified(void)
{
    return secure_connection && sio_secure_unverified(sio);
}

const char *
net_session_info(void)
{
    if (sio == NULL) {
	return NULL;
    }
    return sio_session_info(sio);
}

const char *
net_server_cert_info(void)
{
    if (sio == NULL) {
	return NULL;
    }
    return sio_server_cert_info(sio);
}

const char *
net_server_subject_names(void)
{
    if (sio == NULL) {
	return NULL;
    }
    return sio_server_subject_names(sio);
}

bool
net_secure_connection(void)
{
    return secure_connection;
}

unsigned
net_sio_supported(void)
{
    return sio_supported();
}

const char *
net_sio_provider(void)
{
    return sio_provider();
}

/*
 * Change the NOP transmit interval.
 */
void
net_nop_seconds(void)
{
    /* Cancel first. */
    if (nop_timeout_id != NULL_IOID) {
	RemoveTimeOut(nop_timeout_id);
	nop_timeout_id = NULL_IOID;
    }

    /* Restart with the new interval. */
    if (appres.nop_seconds > 0 &&
	    !HOST_FLAG(NO_TELNET_HOST) &&
	    cstate >= TCP_PENDING) {
	nop_timeout_id = AddTimeOut(appres.nop_seconds * 1000, send_nop);
    }
}

/* Break NVT data tracing. */
void
net_nvt_break(void)
{
    if (nvt_data) {
	ntvtrace("\n");
	nvt_data = 0;
    }
}

/* Toggle line mode. */
static toggle_upcall_ret_t
toggle_linemode(const char *name, const char *value, unsigned flags, ia_t ia)
{
    bool v;
    const char *errmsg;

    if (!IN_NVT || HOST_FLAG(NO_TELNET_HOST)) {
	popup_an_error("Can only change %s in NVT mode", LINEMODE_NAME);
	return TU_FAILURE;
    }
    errmsg = boolstr(value, &v);
    if (errmsg != NULL) {
	popup_an_error("%s %s", LINEMODE_NAME, errmsg);
	return TU_FAILURE;
    }
    if (v) {
	net_linemode();
    } else {
	net_charmode();
    }
    return TU_SUCCESS;
}

/* Canonicalization for no-TELNET input mode. */
static const char *
canonicalize_ntim(const char *value)
{
    int i;

    if (value == NULL) {
	return NULL;
    }
    for (i = 0; ntim_name[i] != NULL; i++) {
	if (!strcasecmp(value, ntim_name[i])) {
	    return ntim_name[i];
	}
    }
    return "?";
}

/* Parse a no-TELNET input mode value. */
static ntim_t
parse_ntim(const char *value)
{
    int i;

    /* Validate and translate the value. */
    for (i = 0; ntim_name[i] != NULL; i++) {
	if (!strcasecmp(value, ntim_name[i])) {
	    return (ntim_t)i;
	}
    }
    return NTIM_UNKNOWN;
}

/* Toggle no-TELNET input mode. */
static toggle_upcall_ret_t
toggle_ntim(const char *name, const char *value, unsigned flags, ia_t ia)
{
    ntim_t n;

    /* Validate and translate the value. */
    n = parse_ntim(value);
    if (n == NTIM_UNKNOWN) {
	popup_an_error("Invalid %s value '%s'", ResNoTelnetInputMode, value);
	return TU_FAILURE;
    }

    /* Implement it. */
    ntim = n;
    Replace(appres.interactive.no_telnet_input_mode, NewString(value));
    if (CONNECTED && HOST_FLAG(NO_TELNET_HOST)) {
	check_linemode(false);
    }
    return TU_SUCCESS;
}

/* Toggle bind limit mode. */
static toggle_upcall_ret_t
toggle_bind_limit(const char *name, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg;

    if ((errmsg = boolstr(value, &appres.bind_limit)) != NULL) {
        popup_an_error("%s", errmsg);
        return TU_FAILURE;
    }
    return TU_SUCCESS;
}

/* Toggle wrong terminal name mode. */
static toggle_upcall_ret_t
toggle_wrong_terminal_name(const char *name, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg;
    bool previous = appres.wrong_terminal_name;

    if ((errmsg = boolstr(value, &appres.wrong_terminal_name)) != NULL) {
        popup_an_error("%s", errmsg);
        return TU_FAILURE;
    }
    if (appres.wrong_terminal_name != previous) {
	net_set_default_termtype();
    }
    return TU_SUCCESS;
}

/* Toggle contention resolution. */
static toggle_upcall_ret_t
toggle_contention_resolution(const char *name, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg;

    if ((errmsg = boolstr(value, &appres.contention_resolution)) != NULL) {
        popup_an_error("%s", errmsg);
        return TU_FAILURE;
    }
    return TU_SUCCESS;
}

/* Toggle port 992 TLS mapping. */
static toggle_upcall_ret_t
toggle_tls992(const char *name, const char *value, unsigned flags, ia_t ia)
{
    const char *errmsg;

    if ((errmsg = boolstr(value, &appres.tls992)) != NULL) {
        popup_an_error("%s", errmsg);
        return TU_FAILURE;
    }
    return TU_SUCCESS;
}

/* Module registration. */
void
net_register(void)
{
    /* Register for state changes. */
    register_schange(ST_REMODEL, net_remodel);

    /* Register extended toggles. */
    register_extended_toggle(LINEMODE_NAME, toggle_linemode, NULL, NULL, (void **)&linemode, XRM_BOOLEAN);
    register_extended_toggle(ResNoTelnetInputMode, toggle_ntim, NULL, canonicalize_ntim,
	    (void **)&appres.interactive.no_telnet_input_mode, XRM_STRING);
    register_extended_toggle(ResBindLimit, toggle_bind_limit, NULL, NULL, (void **)&appres.bind_limit, XRM_BOOLEAN);
    register_extended_toggle(ResWrongTerminalName, toggle_wrong_terminal_name,
	    NULL, NULL, (void **)&appres.wrong_terminal_name, XRM_BOOLEAN);
    register_extended_toggle(ResContentionResolution, toggle_contention_resolution, NULL, NULL,
	    (void **)&appres.contention_resolution, XRM_BOOLEAN);
    register_extended_toggle(ResTls992, toggle_tls992, NULL, NULL, (void **)&appres.tls992, XRM_BOOLEAN);
}
