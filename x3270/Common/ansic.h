/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 *	ansic.h
 *		Global declarations for ansi.c.
 */

#if defined(X3270_ANSI) /*[*/

extern void ansi_init(void);
extern void ansi_process(unsigned int c);
extern void ansi_send_clear(void);
extern void ansi_send_down(void);
extern void ansi_send_home(void);
extern void ansi_send_left(void);
extern void ansi_send_pa(int nn);
extern void ansi_send_pf(int nn);
extern void ansi_send_right(void);
extern void ansi_send_up(void);
extern void ansi_snap(void);
extern void ansi_snap_modes(void);
extern void toggle_lineWrap(struct toggle *t, enum toggle_type type);

#else /*][*/

#define ansi_init()
#define ansi_process(n)
#define ansi_send_clear()
#define ansi_send_down()
#define ansi_send_home()
#define ansi_send_left()
#define ansi_send_pa(n)
#define ansi_send_pf(n)
#define ansi_send_right()
#define ansi_send_up()
#define ansi_snap()

#endif /*]*/
