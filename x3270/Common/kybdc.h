/*
 * Copyright (c) 1995-2009, 2013 Paul Mattes.
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
extern eaction_t AltCursor_eaction;
extern eaction_t Attn_eaction;
extern eaction_t BackSpace_eaction;
extern eaction_t BackTab_eaction;
extern eaction_t CircumNot_eaction;
extern eaction_t Clear_eaction;
extern eaction_t Compose_eaction;
extern eaction_t CursorSelect_eaction;
extern eaction_t Delete_eaction;
extern eaction_t DeleteField_eaction;
extern eaction_t DeleteWord_eaction;
extern eaction_t Down_eaction;
extern eaction_t Dup_eaction;
extern eaction_t Enter_eaction;
extern eaction_t Erase_eaction;
extern eaction_t EraseEOF_eaction;
extern eaction_t EraseInput_eaction;
extern eaction_t FieldEnd_eaction;
extern eaction_t FieldMark_eaction;
extern eaction_t Flip_eaction;
extern eaction_t HexString_eaction;
extern eaction_t Home_eaction;
extern eaction_t Insert_eaction;
extern eaction_t Interrupt_eaction;
extern eaction_t Key_eaction;
extern eaction_t Left2_eaction;
extern eaction_t Left_eaction;
extern eaction_t MonoCase_eaction;
extern eaction_t MoveCursor_eaction;
extern eaction_t Newline_eaction;
extern eaction_t NextWord_eaction;
extern eaction_t PA_eaction;
extern eaction_t PF_eaction;
extern eaction_t PreviousWord_eaction;
extern eaction_t Reset_eaction;
extern eaction_t Right2_eaction;
extern eaction_t Right_eaction;
extern eaction_t String_eaction;
extern eaction_t SysReq_eaction;
extern eaction_t Tab_eaction;
extern eaction_t TemporaryKeymap_eaction;
extern eaction_t ToggleInsert_eaction;
extern eaction_t ToggleReverse_eaction;
extern eaction_t Up_eaction;

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

