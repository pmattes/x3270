/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 *	telnetc.h
 *		Global declarations for telnet.c.
 */

/* Output buffer. */
extern unsigned char *obuf, *obptr;

/* Spelled-out tty control character. */
struct ctl_char {
	const char *name;
	char value[3];
};

extern void net_abort(void);
extern Boolean net_add_dummy_tn3270e(void);
extern void net_add_eor(unsigned char *buf, int len);
extern void net_break(void);
extern void net_charmode(void);
extern int net_connect(const char *, char *, Boolean, Boolean *, Boolean *);
extern void net_disconnect(void);
extern void net_exception(void);
extern int net_getsockname(void *buf, int *len);
extern void net_hexansi_out(unsigned char *buf, int len);
extern void net_input(void);
extern void net_interrupt(void);
extern void net_linemode(void);
extern struct ctl_char *net_linemode_chars(void);
extern void net_output(void);
extern const char *net_query_bind_plu_name(void);
extern const char *net_query_connection_state(void);
extern const char *net_query_host(void);
extern const char *net_query_lu_name(void);
extern void net_sendc(char c);
extern void net_sends(const char *s);
extern void net_send_erase(void);
extern void net_send_kill(void);
extern void net_send_werase(void);
extern Boolean net_snap_options(void);
extern void space3270out(int n);
extern const char *tn3270e_current_opts(void);
extern void trace_netdata(char direction, unsigned const char *buf, int len);
extern void popup_a_sockerr(char *fmt, ...) printflike(1, 2);
extern char *net_proxy_type(void);
extern char *net_proxy_host(void);
extern char *net_proxy_port(void);
extern Boolean net_bound(void);
