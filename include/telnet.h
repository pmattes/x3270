/*
 * Copyright (c) 1995-2009, 2013-2015 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC nor
 *       the names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	telnet.h
 *		Global declarations for telnet.c, beyond what is declared in
 *		telnet_core.h.
 */

#if defined(PR3287) /*[*/
#error Do not include this file for pr3287
#endif /*]*/

extern int ns_brcvd;
extern int ns_bsent;
extern int ns_rrcvd;
extern int ns_rsent;
extern time_t ns_time;

void net_abort(void);
void net_break(void);
void net_charmode(void);
iosrc_t net_connect(const char *, char *, bool, bool *, bool *);
void net_exception(iosrc_t fd, ioid_t id);
int net_getsockname(void *buf, int *len);
void net_hexnvt_out(unsigned char *buf, int len);
void net_input(iosrc_t fd, ioid_t id);
void net_interrupt(void);
void net_linemode(void);
const char *net_query_bind_plu_name(void);
const char *net_query_connection_state(void);
const char *net_query_host(void);
const char *net_query_lu_name(void);
const char *net_query_ssl(void);
void net_sendc(char c);
void net_sends(const char *s);
bool net_snap_options(void);
const char *tn3270e_current_opts(void);
char *net_proxy_type(void);
char *net_proxy_host(void);
char *net_proxy_port(void);
bool net_bound(void);
#if defined(HAVE_LIBSSL) /*[*/
void ssl_base_init(char *cl_hostname, bool *pending);
#endif /*]*/
extern int linemode;
void net_set_default_termtype(void);

/* These are for linemode.c to call, not external users. */
void net_cookedout(const char *buf, size_t len);
void net_cookout(const char *buf, size_t len);
