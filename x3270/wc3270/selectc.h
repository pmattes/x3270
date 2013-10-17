/*
 * Copyright (c) 2013, Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	selectc.h
 *		A Windows console-based 3270 Terminal Emulator
 *		Screen selections
 */

/* Actions. */
extern void Copy_action(Widget w, XEvent *event, String *params,
	Cardinal *num_params);

/* Used by the screen logic. */
extern Boolean select_changed(unsigned row, unsigned col, unsigned rows,
	unsigned cols);
typedef enum {
	SE_BUTTON_DOWN,
	SE_BUTTON_UP,
	SE_MOVE,
	SE_DOUBLE_CLICK
} select_event_t;
extern Boolean select_event(unsigned row, unsigned col, select_event_t event,
	Boolean shift);
extern void select_init(unsigned max_rows, unsigned max_cols);
extern void select_sync(unsigned row, unsigned col, unsigned rows,
	unsigned cols);

/* Used by common code. */
extern void unselect(int baddr, int len);
extern Boolean area_is_selected(int baddr, int len);

