/*
 * Copyright (c) 2007-2009, 2013-2015, 2018-2020 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
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
 *	proxy.c
 *		This module implements various kinds of proxies.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif /*]*/

#include "popups.h"
#include "proxy.h"
#include "proxy_names.h"
#include "proxy_private.h"
#include "proxy_passthru.h"
#include "proxy_http.h"
#include "proxy_names.h"
#include "proxy_telnet.h"
#include "proxy_socks4.h"
#include "proxy_socks5.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "w3misc.h"

#define PROXY_MSEC	(15 * 1000)

/* proxy type names -- keep these in sync with proxytype_t! */
const char *type_name[PT_MAX] = {
    "unknown",
    PROXY_PASSTHRU,
    PROXY_HTTP,
    PROXY_TELNET,
    PROXY_SOCKS4,
    PROXY_SOCKS4A,
    PROXY_SOCKS5,
    PROXY_SOCKS5D
};

int proxy_ports[PT_MAX] = {
    0,
    NPORT_PASSTHRU,
    NPORT_HTTP,
    0,
    NPORT_SOCKS4,
    NPORT_SOCKS4A,
    NPORT_SOCKS5,
    NPORT_SOCKS5D
};

static bool parse_host_port(char *s, char **puser, char **phost, char **pport);

/* Continue functions. */
static continue_t *continues[PT_MAX] = {
    NULL,
    NULL,
    proxy_http_continue,
    NULL,
    proxy_socks4_continue,
    proxy_socks4_continue,
    proxy_socks5_continue,
    proxy_socks5_continue
};

/* Close functions. */
typedef void close_t(void);
static close_t *closes[PT_MAX] = {
    NULL,
    NULL,
    proxy_http_close,
    NULL,
    proxy_socks4_close,
    proxy_socks4_close,
    proxy_socks5_close,
    proxy_socks5_close
};

static proxytype_t proxy_type = PT_NONE;
static bool proxy_pending = false;
static ioid_t proxy_timeout_id = NULL_IOID;

/* Return the name for a given proxy type. */
const char *
proxy_type_name(proxytype_t type)
{
    if (type <= PT_NONE || type >= PT_MAX) {
	return "unknown";
    } else {
	return type_name[type];
    }
}

/* Return whether a proxy type accepts a username. */
bool
proxy_takes_username(proxytype_t type)
{
    switch (type) {
    case PT_HTTP:
    case PT_SOCKS4:
    case PT_SOCKS4A:
    case PT_SOCKS5:
    case PT_SOCKS5D:
	return true;
    default:
	return false;
    }
}

/* Return the default port for a proxy type. */
int
proxy_default_port(proxytype_t type)
{
    if (type <= PT_NONE || type >= PT_MAX) {
	return 0;
    } else {
	return proxy_ports[type];
    }
}

/*
 * Resolve the type, hostname and port for a proxy.
 * Returns -1 for failure, 0 for no proxy, >0 (the proxy type) for success.
 */
int
proxy_setup(const char *proxy, char **puser, char **phost, char **pport)
{
    char *colon;
    size_t sl;

    if (proxy == NULL) {
	return PT_NONE;
    }

    if ((colon = strchr(proxy, ':')) == NULL || (colon == proxy)) {
	popup_an_error("Invalid proxy syntax");
	return -1;
    }

    sl = colon - proxy;
    if (sl == strlen(PROXY_PASSTHRU) &&
	    !strncasecmp(proxy, PROXY_PASSTHRU, sl)) {

	if (!parse_host_port(colon + 1, NULL, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_PASSTHRU);
	}
	return proxy_type = PT_PASSTHRU;
    }
    if (sl == strlen(PROXY_HTTP) && !strncasecmp(proxy, PROXY_HTTP, sl)) {

	if (!parse_host_port(colon + 1, puser, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_HTTP);
	}
	return proxy_type = PT_HTTP;
    }
    if (sl == strlen(PROXY_TELNET) && !strncasecmp(proxy, PROXY_TELNET, sl)) {

	if (!parse_host_port(colon + 1, NULL, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    popup_an_error("Must specify port for telnet proxy");
	    return -1;
	}
	return proxy_type = PT_TELNET;
    }
    if (sl == strlen(PROXY_SOCKS4) && !strncasecmp(proxy, PROXY_SOCKS4, sl)) {

	if (!parse_host_port(colon + 1, puser, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_SOCKS4);
	}
	return proxy_type = PT_SOCKS4;
    }
    if (sl == strlen(PROXY_SOCKS4A) &&
	    !strncasecmp(proxy, PROXY_SOCKS4A, sl)) {

	if (!parse_host_port(colon + 1, puser, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_SOCKS4A);
	}
	return proxy_type = PT_SOCKS4A;
    }
    if (sl == strlen(PROXY_SOCKS5) && !strncasecmp(proxy, PROXY_SOCKS5, sl)) {

	if (!parse_host_port(colon + 1, puser, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_SOCKS5);
	}
	return proxy_type = PT_SOCKS5;
    }
    if (sl == strlen(PROXY_SOCKS5D) &&
	    !strncasecmp(proxy, PROXY_SOCKS5D, sl)) {

	if (!parse_host_port(colon + 1, puser, phost, pport)) {
	    return -1;
	}
	if (*pport == NULL) {
	    *pport = NewString(PORT_SOCKS5D);
	}
	return proxy_type = PT_SOCKS5D;
    }
    popup_an_error("Invalid proxy type '%.*s'", (int)sl, proxy);
    return -1;
}

/*
 * Parse [user:password@]host[:port] from a string.
 * 'host' can be in square brackets to allow numeric IPv6 addresses.
 * Returns the host name and port name in heap memory.
 * Returns false for failure, true for success.
 */
static bool
parse_host_port(char *s, char **puser, char **phost, char **pport)
{
    char *at;
    char *h;
    char *colon;
    char *hstart;
    size_t hlen;

    /* Check for 'username:password@' first. */
    if ((at = strchr(s, '@')) != NULL) {
	if (puser == NULL) {
	    popup_an_error("Proxy type does not support username");
	    return false;
	}
	if (at == s) {
	    popup_an_error("Invalid proxy username syntax");
	    return false;
	}
	h = at + 1;
    } else {
	h = s;
    }

    if (*h == '[') {
	char *rbrack;

	/* Hostname in square brackets. */
	hstart = h + 1;
	rbrack = strchr(h, ']');
	if (rbrack == NULL || rbrack == h + 1 ||
		(*(rbrack + 1) != '\0' && *(rbrack + 1) != ':')) {
	    popup_an_error("Invalid proxy hostname syntax");
	    return false;
	}
	if (*(rbrack + 1) == ':') {
	    colon = rbrack + 1;
	} else {
	    colon = NULL;
	}
	hlen = rbrack - (h + 1);
    } else {
	hstart = h;
	colon = strchr(h, ':');
	if (colon == h) {
	    popup_an_error("Invalid proxy hostname syntax");
	    return false;
	}
	if (colon == NULL) {
	    hlen = strlen(h);
	} else {
	    hlen = colon - h;
	}
    }

    /* Parse the port. */
    if (colon == NULL || !*(colon + 1)) {
	*pport = NULL;
    } else {
	*pport = NewString(colon + 1);
    }

    /* Copy out the hostname. */
    *phost = Malloc(hlen + 1);
    strncpy(*phost, hstart, hlen);
    (*phost)[hlen] = '\0';

    /* Copy out the username. */
    if (puser != NULL) {
	if (at != NULL) {
	    *puser = Malloc((at - s) + 1);
	    strncpy(*puser, s, at - s);
	    (*puser)[at - s] = '\0';
	} else {
	    *puser = NULL;
	}
    } else if (at != NULL) {
	popup_an_error("Invalid proxy hostname syntax (user@ not supported for this type");
	return false;
    }

    return true;
}

/*
 * Proxy negotiation timed out.
 */
static void
proxy_timeout(ioid_t id _is_unused)
{
    proxy_timeout_id = NULL_IOID;
    connect_error("%s proxy timed out", type_name[proxy_type]);
}

/*
 * Negotiate with the proxy server.
 */
proxy_negotiate_ret_t
proxy_negotiate(socket_t fd, const char *user, const char *host,
	unsigned short port, bool blocking)
{
    proxy_negotiate_ret_t ret;

    if (proxy_timeout_id != NULL_IOID) {
	RemoveTimeOut(proxy_timeout_id);
	proxy_timeout_id = NULL_IOID;
    }

    switch (proxy_type) {
    case PT_NONE:
	ret = PX_SUCCESS;
	break;
    case PT_PASSTHRU:
	ret = proxy_passthru(fd, host, port);
	break;
    case PT_HTTP:
	ret = proxy_http(fd, user, host, port);
	break;
    case PT_TELNET:
	ret = proxy_telnet(fd, host, port);
	break;
    case PT_SOCKS4:
	ret = proxy_socks4(fd, user, host, port, false);
	break;
    case PT_SOCKS4A:
	ret = proxy_socks4(fd, user, host, port, true);
	break;
    case PT_SOCKS5:
	ret = proxy_socks5(fd, user, host, port, false);
	break;
    case PT_SOCKS5D:
	ret = proxy_socks5(fd, user, host, port, true);
	break;
    default:
	ret = PX_FAILURE;
	break;
    }

    proxy_pending = (ret == PX_WANTMORE);
    if (proxy_pending) {
	if (blocking) {
	    do {
		fd_set rfds;
		struct timeval tv;

		/* Wait for more input. */
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = PROXY_MSEC / 1000;
		tv.tv_usec = (PROXY_MSEC % 1000) * 10;
		if (select((int)(fd + 1), &rfds, NULL, NULL, &tv) <= 0) {
		    popup_an_error("%s proxy timeout", type_name[proxy_type]);
		    return PX_FAILURE;
		}

		ret = proxy_continue();
	    } while (ret == PX_WANTMORE);
	} else {
	    /* Set a timeout in case the input never arrives. */
	    proxy_timeout_id = AddTimeOut(PROXY_MSEC, proxy_timeout);
	}
    }

    if (ret == PX_SUCCESS) {
	proxy_close();
    }
    return ret;
}

/*
 * Continue proxy negotiation.
 */
proxy_negotiate_ret_t
proxy_continue(void)
{
    proxy_negotiate_ret_t ret;

    if (proxy_type <= 0 ||
	    proxy_type >= PT_MAX ||
	    continues[proxy_type] == NULL ||
	    !proxy_pending) {
	popup_an_error("proxy_continue: wrong state");
	return PX_FAILURE;
    }

    ret = (*continues[proxy_type])();
    if (ret == PX_SUCCESS) {
	proxy_close();
    }
    return ret;
}

/*
 * Clean up pending proxy state.
 */
void
proxy_close(void)
{
    if (proxy_type > 0 &&
	    proxy_type < PT_MAX &&
	    closes[proxy_type] != NULL) {
	(*closes[proxy_type])();
    }
    proxy_type = PT_NONE;
    proxy_pending = false;
    if (proxy_timeout_id != NULL_IOID) {
	RemoveTimeOut(proxy_timeout_id);
	proxy_timeout_id = NULL_IOID;
    }
}
