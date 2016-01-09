/*
 * Copyright (c) 1995-2009, 2013-2016 Paul Mattes.
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
 *	kybd.h
 *		Global declarations for kybd.c.
 */

/* keyboard lock states */
extern unsigned int kybdlock;
#define KL_OERR_MASK		0x000f
#define  KL_OERR_PROTECTED	1
#define  KL_OERR_NUMERIC	2
#define  KL_OERR_OVERFLOW	3
#define  KL_OERR_DBCS		4
#define	KL_NOT_CONNECTED	0x0010
#define	KL_AWAITING_FIRST	0x0020
#define	KL_OIA_TWAIT		0x0040
#define	KL_OIA_LOCKED		0x0080
#define	KL_DEFERRED_UNLOCK	0x0100
#define KL_ENTER_INHIBIT	0x0200
#define KL_SCROLLED		0x0400
#define KL_OIA_MINUS		0x0800

extern unsigned char aid;

void do_reset(bool explicit);
size_t emulate_input(const char *s, size_t len, bool pasting);
size_t emulate_uinput(const ucs4_t *s, size_t len, bool pasting);
void hex_input(const char *s);
void kybdlock_clr(unsigned int bits, const char *cause);
void kybd_inhibit(bool inhibit);
void kybd_register(void);
int kybd_prime(void);
void kybd_scroll_lock(bool lock);
bool run_ta(void);
int state_from_keymap(char keymap[32]);
void lightpen_select(int baddr);
void key_UCharacter(ucs4_t ucs4, enum keytype keytype, enum iaction cause);
void kybd_register(void);
bool Down_action(ia_t ia, unsigned argc, const char **argv);
bool Left_action(ia_t ia, unsigned argc, const char **argv);
bool Right_action(ia_t ia, unsigned argc, const char **argv);
bool Up_action(ia_t ia, unsigned argc, const char **argv);
