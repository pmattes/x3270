/*
 * Copyright (c) 1995-2024 Paul Mattes.
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
# error "Do not include this file for pr3287"
#endif /*]*/

extern int ns_brcvd;
extern int ns_bsent;
extern int ns_rrcvd;
extern int ns_rsent;
extern time_t ns_time;
extern const char *state_name[];
extern struct timeval net_last_recv_ts;
extern char *numeric_host;
extern char *numeric_port;

void net_abort(void);
void net_break(char c);
void net_charmode(void);
net_connect_t net_connect(const char *, char *, char *, bool, iosrc_t *);
void net_exception(iosrc_t fd, ioid_t id);
int net_getsockname(void *buf, int *len);
void net_hexnvt_out(unsigned char *buf, size_t len);
void net_input(iosrc_t fd, ioid_t id);
void net_interrupt(char c);
void net_linemode(void);
void net_nop_seconds(void);
void net_nvt_break(void);
const char *net_query_bind_plu_name(void);
const char *net_query_connection_state(void);
const char *net_query_host(void);
const char *net_query_lu_name(void);
const char *net_query_tls(void);
void net_sendc(char c);
void net_sends(const char *s);
bool net_snap_options(void);
const char *tn3270e_current_opts(void);
const char *net_proxy_type(void);
const char *net_proxy_user(void);
const char *net_proxy_host(void);
const char *net_proxy_port(void);
bool net_bound(void);
extern bool linemode;
bool net_secure_connection(void);
void net_set_default_termtype(void);
bool net_secure_unverified(void);
const char *net_server_cert_info(void);
const char *net_server_subject_names(void);
const char *net_session_info(void);
void net_password_continue(const char *password);
unsigned net_sio_supported(void);
const char *net_sio_provider(void);
const char *net_myopts(void);
const char *net_hisopts(void);
void net_register(void);

/* These are for linemode.c to call, not external users. */
void net_cookedout(const char *buf, size_t len);
void net_cookout(const char *buf, size_t len);
