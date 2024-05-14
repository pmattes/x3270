/*
 * Copyright (c) 1996-2024 Paul Mattes.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ft.c
 *		UI back-end file transfer logic.
 */

#include "globals.h"
#include "resources.h"
#include "unicodec.h"

#include "actions.h"
#include "b3270proto.h"
#include "ft.h"
#include "ft_private.h"
#include "ft_gui.h"
#include "txa.h"
#include "ui_stream.h"

void
ft_gui_progress_popdown(void)
{
}

void
ft_gui_errmsg_prepare(char *msg)
{
}

void ft_gui_clear_progress(void)
{
}

void
ft_gui_complete_popup(const char *msg, bool is_error)
{
    ui_leaf(IndFt,
	    AttrState, AT_STRING, "complete",
	    AttrSuccess, AT_BOOLEAN, !is_error,
	    AttrText, AT_STRING, msg,
	    AttrCause, AT_STRING, ia_name[ft_cause],
	    NULL);
}

void
ft_gui_update_length(size_t length)
{
    ui_leaf(IndFt,
	    AttrState, AT_STRING, "running",
	    AttrBytes, AT_INT, (int64_t)length,
	    AttrCause, AT_STRING, ia_name[ft_cause],
	    NULL);
}

void
ft_gui_running(size_t length)
{
    ui_leaf(IndFt,
	    AttrState, AT_STRING, "running",
	    AttrBytes, AT_INT, (int64_t)length,
	    AttrCause, AT_STRING, ia_name[ft_cause],
	    NULL);
}

void
ft_gui_aborting(void)
{
    ui_leaf(IndFt,
	    AttrState, AT_STRING, "aborting",
	    AttrCause, AT_STRING, ia_name[ft_cause],
	    NULL);
}

void
ft_gui_awaiting(void)
{
    ui_leaf(IndFt,
	    AttrState, AT_STRING, "awaiting",
	    AttrCause, AT_STRING, ia_name[ft_cause],
	    NULL);
}
