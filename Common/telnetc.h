/*
 * Copyright 1995, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
 *   2007 by Paul Mattes.
 * RPQNAMES modifications Copyright 2004 by Don Russell.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
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
