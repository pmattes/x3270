/*
 * Copyright (c) 1995-2009, 2013-2014 Paul Mattes.
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
 *	kybdc.h
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

/* actions */
extern action_t AltCursor_action;
extern action_t Attn_action;
extern action_t BackSpace_action;
extern action_t BackTab_action;
extern action_t CircumNot_action;
extern action_t Clear_action;
extern action_t Compose_action;
extern action_t CursorSelect_action;
extern action_t Delete_action;
extern action_t DeleteField_action;
extern action_t DeleteWord_action;
extern action_t Down_action;
extern action_t Dup_action;
extern action_t Enter_action;
extern action_t Erase_action;
extern action_t EraseEOF_action;
extern action_t EraseInput_action;
extern action_t FieldEnd_action;
extern action_t FieldMark_action;
extern action_t Flip_action;
extern action_t HexString_action;
extern action_t Home_action;
extern action_t Insert_action;
extern action_t Interrupt_action;
extern action_t Key_action;
extern action_t Left2_action;
extern action_t Left_action;
extern action_t MonoCase_action;
extern action_t MoveCursor_action;
extern action_t Newline_action;
extern action_t NextWord_action;
extern action_t PA_action;
extern action_t PF_action;
extern action_t PreviousWord_action;
extern action_t Reset_action;
extern action_t Right2_action;
extern action_t Right_action;
extern action_t String_action;
extern action_t SysReq_action;
extern action_t Tab_action;
extern action_t TemporaryKeymap_action;
extern action_t ToggleInsert_action;
extern action_t ToggleReverse_action;
extern action_t Up_action;

#if defined(C3270) /*[*/
extern action_t ignore_action;
#endif /*]*/

/* other functions */
extern void do_reset(Boolean explicit);
extern int emulate_input(const char *s, int len, Boolean pasting);
extern int emulate_uinput(const ucs4_t *s, int len, Boolean pasting);
extern void hex_input(const char *s);
extern void kybdlock_clr(unsigned int bits, const char *cause);
extern void kybd_inhibit(Boolean inhibit);
extern void kybd_init(void);
extern int kybd_prime(void);
extern void kybd_scroll_lock(Boolean lock);
extern Boolean run_ta(void);
extern int state_from_keymap(char keymap[32]);
extern void lightpen_select(int baddr);
extern void key_UCharacter(ucs4_t ucs4, enum keytype keytype,
	enum iaction cause);

