/*
 * Copyright (c) 1993-2022 Paul Mattes.
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
 *	host_gui.c
 *		GUI-specific functions called from the host connect/disconnect
 *		logic.
 */

#include "globals.h"
#include "appres.h"

#include "host.h"
#include "host_gui.h"
#include "xio.h"
#include "xpopups.h"

bool
host_gui_connect(void)
{
    if (appres.once && !host_retry_mode) {
	/* Exit when the error pop-up pops down. */
	exiting = true;
	return true;
    } else {
	return false;
    }
}

void
host_gui_connect_initial(void)
{
    if (host_retry_mode && error_popup_visible()) {
	popdown_an_error();
    }
}

bool
host_gui_disconnect(void)
{
    if (appres.once && !host_retry_mode) {
	if (error_popup_visible()) {
	    /* If there is an error pop-up, exit when it pops down. */
	    exiting = true;
	} else {
	    /* Exit now. */
	    x3270_exit(0);
	    return true;
	}
	return true;
    } else {
	return false;
    }
}

void
host_gui_connected(void)
{
    if (error_popup_visible()) {
	popdown_an_error();
    }
}
