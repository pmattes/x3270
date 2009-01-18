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

/* actions */
extern void AltCursor_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Attn_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void BackSpace_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void BackTab_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void CircumNot_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Clear_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Compose_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void CursorSelect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Default_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void DeleteField_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void DeleteWord_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Delete_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Down_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Dup_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Enter_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void EraseEOF_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void EraseInput_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Erase_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void FieldEnd_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void FieldMark_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Flip_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void HexString_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Home_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ignore_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Insert_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Interrupt_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Key_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Left2_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Left_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void MonoCase_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void MouseSelect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void MoveCursor_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Newline_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void NextWord_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_Shift_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PF_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PreviousWord_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Reset_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Right2_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Right_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void String_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void SysReq_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Tab_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void TemporaryKeymap_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ToggleInsert_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ToggleReverse_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Up_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);

/* other functions */
extern void do_reset(Boolean explicit);
extern int emulate_input(char *s, int len, Boolean pasting);
extern int emulate_uinput(ucs4_t *s, int len, Boolean pasting);
extern void hex_input(char *s);
extern void kybdlock_clr(unsigned int bits, const char *cause);
extern void kybd_inhibit(Boolean inhibit);
extern void kybd_init(void);
extern int kybd_prime(void);
extern void kybd_scroll_lock(Boolean lock);
extern Boolean run_ta(void);
extern int state_from_keymap(char keymap[32]);
