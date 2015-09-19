/*
 * Copyright (c) 1996-2015 Paul Mattes.
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
#include "macros.h"
#include "popups.h"
#include "screen.h"
#include "utils.h"

#include "ft_gui.h"

/* Macros. */

/* Globals. */

/* Statics. */
static bool ft_sigint_aborting = false;
static ioid_t ft_poll_id = NULL_IOID;

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
    if (ftc->is_interactive || escaped) {
	printf("\r%79s\r", "");
	fflush(stdout);
    } else {
	popup_an_info(" ");
    }
}

/* Pop up a successful completion message. */
void
ft_gui_complete_popup(const char *msg)
{
#if !defined(_WIN32) /*[*/
    signal(SIGINT, SIG_IGN);
#else /*][*/
    screen_set_ctrlc_fn(NULL);
#endif /*]*/
    if (ftc->is_interactive) {
	/*
	 * We call sms_info here instead of plain printf, so that
	 * macro_output is set and the user will stay at the c3270> prompt.
	 */
	sms_info("%s\n", msg);
    } else if (escaped) {
	printf("%s\n", msg);
	fflush(stdout);
    }
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_gui_update_length(size_t length)
{
    if (ftc->is_interactive || escaped) {
	if (ft_sigint_aborting) {
	    ft_sigint_aborting = false;
	    if (!ft_do_cancel()) {
		printf("Aborting... waiting for host acknowledgment... ");
	    }
	} else {
	    printf("\r%79s\rTransferred %lu bytes ", "",
		    (unsigned long)length);
	}
	fflush(stdout);
    } else {
	/* Not interactive, put it in the OIA. */
	popup_an_info("Transferred %lu bytes", (unsigned long)length);
    }
}

/* Replace the 'waiting' pop-up with the 'in-progress' pop-up. */
void
ft_gui_running(size_t length _is_unused)
{
    if (ftc->is_interactive) {
	RemoveTimeOut(ft_poll_id);
	ft_poll_id = NULL_IOID;
    }
    ft_update_length();
}

/* Process a protocol-generated abort. */
void
ft_gui_aborting(void)
{
#if !defined(_WIN32) /*[*/
    signal(SIGINT, SIG_IGN);
#else /*][*/
    screen_set_ctrlc_fn(NULL);
#endif /*]*/
}

/* Check for interactive mode. */
ft_gui_interact_t 
ft_gui_interact(ft_conf_t *p)
{   
    if (!escaped) {
	return FGI_NOP;
    }
    if (interactive_transfer(p) < 0) {
	printf("\n");
	fflush(stdout);
	action_output("Aborted");
	return FGI_ABORT;
    }
    p->is_interactive = true;
    return FGI_SUCCESS;
}

#if !defined(_WIN32) /*[*/
static void
ft_sigint(int ignored _is_unused)
{
    signal(SIGINT, SIG_IGN);
    ft_sigint_aborting = true;
}
#else /*][*/
static void
ft_ctrlc_fn(void)
{
    screen_set_ctrlc_fn(NULL);
    ft_sigint_aborting = true;
}
#endif /*]*/

static void
ft_poll_abort(ioid_t id _is_unused)
{
    if (ft_sigint_aborting) {
	ft_sigint_aborting = false;
	if (!ft_do_cancel()) {
	    printf("Aborting... waiting for host acknowledgment... ");
	    fflush(stdout);
	}
    } else {
	/* Poll again. */
	ft_poll_id = AddTimeOut(500, ft_poll_abort);
    }
}

/* Display an "Awaiting start of transfer" message. */
void
ft_gui_awaiting(void)
{   
    if (ftc->is_interactive) {
	printf("Press ^C to abort\n");
	printf("Awaiting start of transfer... ");
	fflush(stdout);

	/* Set up a SIGINT handler. */
	ft_sigint_aborting = false;
#if !defined(_WIN32) /*[*/
	signal(SIGINT, ft_sigint);
#else /*][*/
	screen_set_ctrlc_fn(ft_ctrlc_fn);
#endif /*]*/

	/* Start polling for ^C. */
	ft_poll_id = AddTimeOut(500, ft_poll_abort);
	fflush(stdout);
    } else if (escaped) {
	printf("Awaiting start of transfer... ");
	fflush(stdout);
    } else {
	popup_an_info("Awaiting start of transfer");
    }
}
