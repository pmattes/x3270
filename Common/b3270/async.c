/*
 * Copyright (c) 2015-2024 Paul Mattes.
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
 *	async.c
 *		Asynchronous functions for b3270.
 */

#include "globals.h"

#include "b3270proto.h"
#include "ctlr.h"
#include "screen.h"
#include "telnet_gui.h"
#include "txa.h"
#include "ui_stream.h"

/* stubs1 */

bool
screen_selected(int baddr _is_unused)
{
    return false;
}

void
ring_bell(void)
{
    ui_leaf(IndBell, NULL);
}

static int cw = 7;
int *char_width = &cw;

static int ch = 7;
int *char_height = &ch;

void
blink_start(void)
{
}

unsigned
display_heightMM(void)
{
    return 100;
}

unsigned
display_height(void)
{
    return 1;
}

unsigned
display_widthMM(void)
{
    return 100;
}

unsigned
display_width(void)
{
    return 1;
}

void
mcursor_locked(void)
{
}

void
mcursor_normal(void)
{
}

void
mcursor_waiting(void)
{
}

bool
screen_obscured(void)
{
    return false;
}

unsigned long
screen_window_number(void)
{
    return 0L;
}

bool
screen_has_bg_color(void)
{
    return true;
}

void
screen_132(void)
{
}

void
screen_80(void)
{
}

bool
screen_new_display_charsets(const char *realname)
{
    return true;
}

void
screen_system_fixup(void)
{
}

void
telnet_gui_connecting(const char *hostip, const char *portname)
{
    ui_leaf(IndConnectAttempt,
	    AttrHostIp, AT_STRING, hostip,
	    AttrPort, AT_STRING, portname, /* XXX */
	    NULL);
}
