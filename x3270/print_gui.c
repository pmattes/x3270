/*
 * Copyright (c) 1994-2015, 2019, Paul Mattes.
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
 *	print_gui.c
 *		x3270-specific functions for the PrintText action.
 */

#include "globals.h"

#include "print_gui.h"
#include "stmenu.h"

/* Typedefs */

/* Globals */

/* Statics */

/**
 * GUI for the PrintText action.
 *
 * @param[in] use_file	true if the output is a file, false if the output is
 * 			going to a printer
 *
 * @return true if a confirmation dialog was popped up, false if the guts of
 *	   the action should be run now.
 */
bool
print_text_gui(bool use_file)
{
    if (ia_cause == IA_COMMAND ||
	ia_cause == IA_MACRO ||
	ia_cause == IA_SCRIPT ||
	ia_cause == IA_HTTPD) {

	/* Invoked by a script. Run the guts now. */
	return false;

    } else {
	/* Invoked from a keymap -- pop up the confirmation dialog. */
	/* XXX: What about the globals referenced by stmenu_popup?
	 *        file_flag
	 *        stm_ptype
	 *        continuously_flag
	 */
	if (use_file) {
	    stmenu_popup(STMP_TEXT);
	} else {
	    stmenu_popup(STMP_PRINTER);
	}
	return true;
    }
}
