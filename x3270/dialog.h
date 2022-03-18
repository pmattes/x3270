/*
 * Copyright (c) 1996-2022 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	dialogc.h
 *		Global declarations for dialog.c.
 */

typedef struct sr {
    struct sr *next;
    Widget w;
    bool *bvar1;
    bool bval1;
    bool *bvar2;
    bool bval2;
    bool *bvar3;
    bool bval3;
    bool is_value;
    bool has_focus;
} sr_t;

struct toggle_list {                    /* List of toggle widgets */
    Widget *widgets;
};  

typedef enum { T_NUMERIC, T_HOSTFILE, T_UNIXFILE, T_COMMAND } text_t;
extern text_t t_numeric;
extern text_t t_hostfile;
extern text_t t_unixfile;
extern text_t t_command;
extern bool s_true, s_false;

void dialog_set(sr_t **, Widget);
void dialog_apply_bitmap(Widget w, Pixmap p);
void dialog_check_sensitivity(bool *bvar);
void dialog_register_sensitivity(Widget w, bool *bvar1, bool bval1,
	bool *bvar2, bool bval2, bool *bvar3, bool bval3);
void dialog_flip_toggles(struct toggle_list *toggle_list, Widget w);
void dialog_text_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused);
void dialog_match_dimension(Widget w1, Widget w2, const char *n);
void dialog_mark_toggle(Widget w, Pixmap p);

void PA_dialog_copy_xaction(Widget w, XEvent *event, String *parms,
	Cardinal *num_parms);
void PA_dialog_focus_xaction(Widget w, XEvent *event, String *parms,
	Cardinal *num_parms);
void PA_dialog_next_xaction(Widget w, XEvent *event, String *parms,
	Cardinal *num_parms);
