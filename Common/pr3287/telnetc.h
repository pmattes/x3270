/*
 * Copyright 1995, 1999, 2000, 2002, 2003, 2005, 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	telnetc.h
 *		Global declarations for telnet.c.
 */

/* Output buffer. */
extern unsigned char *obuf, *obptr;

extern int negotiate(int s, char *lu, char *assoc);
extern Boolean net_add_dummy_tn3270e(void);
extern void net_add_eor(unsigned char *buf, int len);
extern void net_disconnect(void);
extern void net_exception(void);
extern int net_input(int s);
extern void net_output(void);
extern int process(int s);
extern void space3270out(int n);
extern void trace_netdata(char direction, unsigned const char *buf, int len);
extern void popup_a_sockerr(char *fmt, ...);
