/*
 * Copyright (c) 1996-2015, 2018 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft_gui.c
 *		IND$FILE file transfer dialogs (c3270 version).
 */

#include "globals.h"
#include <signal.h>

#include "appres.h"
#include "cscreen.h"
#include "unicodec.h"
#include "ft.h"
#include "ft_private.h"
#include "icmdc.h"
#include "popups.h"
#include "screen.h"
#include "task.h"
#include "utils.h"

#include "ft_gui.h"

/* Macros. */

/* Globals. */

/* Statics. */

/* Entry points called from the common FT logic. */

/* Pop down the transfer-in-progress pop-up. */
void
ft_gui_progress_popdown(void)
{
}

/* Massage a file transfer error message so it will fit in the pop-up. */
void
ft_gui_errmsg_prepare(char *msg _is_unused)
{
}

/* Clear out the progress display. */
void
ft_gui_clear_progress(void)
{
    popup_an_info(" ");
}

/* Pop up a completion message. */
void
ft_gui_complete_popup(const char *msg, bool is_error)
{
    if (is_error) {
	popup_an_error("%s", msg);
    } else {
	popup_an_info("%s", msg);
    }
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_gui_update_length(size_t length)
{
    /* Put it in the OIA. */
    popup_an_info("Transferred %lu bytes", (unsigned long)length);
}

/* Replace the 'waiting' pop-up with the 'in-progress' pop-up. */
void
ft_gui_running(size_t length _is_unused)
{
    ft_update_length();
}

/* Process a protocol-generated abort. */
void
ft_gui_aborting(void)
{
}

/* Display an "Awaiting start of transfer" message. */
void
ft_gui_awaiting(void)
{   
    popup_an_info("Awaiting start of transfer");
}
