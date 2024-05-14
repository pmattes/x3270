/*
 * Copyright (c) 2007-2024 Paul Mattes.
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
 *	icmd.c
 *		A curses-based 3270 Terminal Emulator
 *		Interactive commands
 */

#include "globals.h"
#include "appres.h"

#include "codepage.h"
#include "ft_dft.h"
#include "ft_private.h" /* must precede ft_gui */
#include "ft_gui.h"
#include "icmdc.h"
#include "popups.h"
#include "split_host.h"
#include "task.h"
#include "txa.h"
#include "utf8.h"
#include "utils.h"

/* Support functions for interactive commands. */

/**
 * Interactive command module registration.
 */
void
icmd_register(void)
{
}

typedef enum {
    YN_NO = 0,
    YN_YES = 1,
    YN_RETRY = -1
} yn_t;

/**
 * Process the response to a yes or no question.
 * 
 * @param defval	Default value
 * @param response	Response text to process
 *
 * @returns YN_NO (0) for no, YN_YES (1) for yes, YN_RETRY (-1) for retry
 */
static yn_t
getyn_iter(int defval, const char *response)
{
    if (!response[0]) {
	return (yn_t)defval;
    }
    if (!strncasecmp(response, "yes", strlen(response))) {
	return YN_YES;
    } else if (!strncasecmp(response, "no", strlen(response))) {
	return YN_NO;
    } else {
	action_output("Please answer 'yes', 'no' or 'quit'.");
	return YN_RETRY;
    }
}

/**
 * Process a numeric response.
 *
 * @param defval	Default value
 * @param response	Response text to process
 *
 * @returns numeric response, or -1 for error
 */
static int
getnum_iter(int defval, const char *response)
{
    unsigned long u;
    char *ptr;

    if (!*response) {
	return defval;
    }
    u = strtoul(response, &ptr, 10);
    if (*ptr == '\0') {
	return (int)u;
    }
    return -1;
}

/* Format a text string to fit on an 80-column display. */
static void
fmt80(const char *s)
{
    char *nl;
    size_t nc;

    action_output(" ");

    while (*s) {
	nl = strchr(s, '\n');
	if (nl == NULL) {
	    nc = strlen(s);
	} else {
	    nc = nl - s;
	}
	if (nc > 78) {
	    const char *t = s + 78;

	    while (t > s && *t != ' ') {
		t--;
	    }
	    if (t != s) {
		nc = t - s;
	    }
	}

	action_output("%.*s", (int)nc, s);
	s += nc;
	if (*s == '\n' || *s == ' ') {
	    s++;
	}
    }
}

/*
 * Pseudo-code:
 *
 *  ask to continue
 *  ask for direction
 *  ask for source file
 *  ask for destination file
 *  ask for host type
 *  ask for ascii/binary
 *  if ascii
 *       ask for cr
 *       ask for remap
 *       if windows and remap
 *          ask for codepage
 *  if receive
 *      ask for overwrite
 *  if not receive
 *      if not CICS
 *          ask for recfm
 *          if not default recfm
 *              ask for lrecl
 *      if tso
 *          ask for blksz
 *          ask for units
 *          if non-default units
 *              ask for primary
 *              ask for secondary
 *              if avblock
 *                  ask for avblock size
 *  if not std data stream
 *      ask for buffer size
 *  ask for additional IND$FILE options
 *  ask to go ahead with the transfer
 */

/* File transfer dialog states */
typedef enum {
    ITS_BASE,		/* base state */
    ITS_CONTINUE,	/* Continue? */
    ITS_DIRECTION,	/* Direction: */
    ITS_SOURCE_FILE,	/* Source file: */
    ITS_DEST_FILE,	/* Destination file: */
    ITS_HOST_TYPE,	/* Host type: */
    ITS_ASCII,		/* Ascii/Binary? */
    ITS_CR,		/* Cr keep/remove? */
    ITS_REMAP,		/* Remap? */
#if defined(_WIN32) /*[*/
    ITS_WINDOWS_CP,	/* Windows code page? */
#endif /*]*/
    ITS_KEEP,		/* Keep? */
    ITS_RECFM,		/* Record format: */
    ITS_LRECL,		/* Record length: */
    ITS_BLKSIZE,	/* Block size: */
    ITS_ALLOC,		/* Allocation type: */
    ITS_PRIMARY,	/* Primary space: */
    ITS_SECONDARY,	/* Secondary space: */
    ITS_AVBLOCK,	/* Avblock size: */
    ITS_BUFFER_SIZE,	/* DFT buffer size: */
    ITS_OTHER_OPTIONS,	/* Other IND$FILE options: */
    ITS_GO		/* Continue? */
} its_t;

/* Interactive transfer context. */
typedef struct {
    ft_conf_t conf;	/* returned config */
    its_t state;	/* state */
    char *prompt;	/* last prompt displayed */
    enum { CR_REMOVE, CR_ADD, CR_KEEP } cr_mode;
    enum { FE_KEEP, FE_REPLACE, FE_APPEND } fe_mode;
} itc_t;

/* Returned state for the incremental dialog. */
typedef enum {
    ITR_RETRY,		/* ask again */
    ITR_CONTINUE,	/* more input needed */
    ITR_GO,		/* go ahead with transfer */
    ITR_QUIT		/* abort the operation */
} itret_t;

/* Resume functions, per state. */
typedef itret_t itret_fn(itc_t *, const char *);
static itret_fn it_continue;
static itret_fn it_direction;
static itret_fn it_source_file;
static itret_fn it_dest_file;
static itret_fn it_host_type;
static itret_fn it_ascii;
static itret_fn it_cr;
static itret_fn it_remap;
#if defined(_WIN32) /*[*/
static itret_fn it_windows_cp;
#endif /*]*/
static itret_fn it_keep;
static itret_fn it_recfm;
static itret_fn it_lrecl;
static itret_fn it_blksize;
static itret_fn it_alloc;
static itret_fn it_primary;
static itret_fn it_secondary;
static itret_fn it_avblock;
static itret_fn it_buffer_size;
static itret_fn it_other_options;
static itret_fn it_go;

static itret_fn *it_resume_fn[] = {
    NULL,
    it_continue,
    it_direction,
    it_source_file,
    it_dest_file,
    it_host_type,
    it_ascii,
    it_cr,
    it_remap,
#if defined(_WIN32) /*[*/
    it_windows_cp,
#endif /*]*/
    it_keep,
    it_recfm,
    it_lrecl,
    it_blksize,
    it_alloc,
    it_primary,
    it_secondary,
    it_avblock,
    it_buffer_size,
    it_other_options,
    it_go
};

/* Predicate functions, per state. */
typedef bool itpred_t(ft_conf_t *);
static itpred_t pred_base;
static itpred_t pred_continue;
static itpred_t pred_direction;
static itpred_t pred_source_file;
static itpred_t pred_dest_file;
static itpred_t pred_host_type;
static itpred_t pred_ascii;
static itpred_t pred_cr;
static itpred_t pred_remap;
#if defined(_WIN32) /*[*/
static itpred_t pred_windows_cp;
#endif /*]*/
static itpred_t pred_keep;
static itpred_t pred_recfm;
static itpred_t pred_lrecl;
static itpred_t pred_blksize;
static itpred_t pred_alloc;
static itpred_t pred_primary;
static itpred_t pred_secondary;
static itpred_t pred_avblock;
static itpred_t pred_buffer_size;
static itpred_t pred_other_options;
static itpred_t pred_go;

static itpred_t *it_pred[] = {
    pred_base,
    pred_continue,
    pred_direction,
    pred_source_file,
    pred_dest_file,
    pred_host_type,
    pred_ascii,
    pred_cr,
    pred_remap,
#if defined(_WIN32) /*[*/
    pred_windows_cp,
#endif /*]*/
    pred_keep,
    pred_recfm,
    pred_lrecl,
    pred_blksize,
    pred_alloc,
    pred_primary,
    pred_secondary,
    pred_avblock,
    pred_buffer_size,
    pred_other_options,
    pred_go
};

/* Ask functions, per state. */
typedef char *it_ask_t(itc_t *);
static it_ask_t ask_continue;
static it_ask_t ask_direction;
static it_ask_t ask_source_file;
static it_ask_t ask_dest_file;
static it_ask_t ask_host_type;
static it_ask_t ask_ascii;
static it_ask_t ask_cr;
static it_ask_t ask_remap;
#if defined(_WIN32) /*[*/
static it_ask_t ask_windows_cp;
#endif /*]*/
static it_ask_t ask_keep;
static it_ask_t ask_recfm;
static it_ask_t ask_lrecl;
static it_ask_t ask_blksize;
static it_ask_t ask_alloc;
static it_ask_t ask_primary;
static it_ask_t ask_secondary;
static it_ask_t ask_avblock;
static it_ask_t ask_buffer_size;
static it_ask_t ask_other_options;
static it_ask_t ask_go;

static it_ask_t *it_ask[] = {
    NULL,
    ask_continue,
    ask_direction,
    ask_source_file,
    ask_dest_file,
    ask_host_type,
    ask_ascii,
    ask_cr,
    ask_remap,
#if defined(_WIN32) /*[*/
    ask_windows_cp,
#endif /*]*/
    ask_keep,
    ask_recfm,
    ask_lrecl,
    ask_blksize,
    ask_alloc,
    ask_primary,
    ask_secondary,
    ask_avblock,
    ask_buffer_size,
    ask_other_options,
    ask_go
};

/**
 * Resume an interactive transfer dialog.
 *
 * @param[in] handle	Handle (transfer state)
 * @param[in] response	Reply text
 *
 * @returns true to continue, false to quit
 */
static bool
it_resume(void *handle, const char *response)
{
    char *r;
    itc_t *itc = (itc_t *)handle;
    itret_t ret;
    its_t state;

    if (response != NULL) {
	size_t sl;

	/* Trim spaces. */
	r = txdFree(NewString(response));
	while (*r == ' ') {
	    r++;
	}
	sl = strlen(r);
	while (sl > 0 && r[sl - 1] == ' ') {
	    r[--sl] = '\0';
	}

	/* Test for 'quit'. */
	if (!strcasecmp(r, "quit")) {
	    return false;
	}
    } else {
	r = NULL;
    }

    /* Call the resume function for the current state. */
    ret = (*it_resume_fn[itc->state])(itc, r);
    if (ret == ITR_RETRY) {
	task_request_input("Transfer", itc->prompt, it_resume, NULL, itc,
		false);
	return false;
    }
    if (ret == ITR_QUIT) {
	/* Go no further. */
	return false;
    }
    if (ret == ITR_GO) {
	/* IA_COMMAND is a lie here, but it is harmless. */
	bool rv = ft_start_backend(&itc->conf, IA_COMMAND);

	if (rv) {
	    action_output("Transfer initiated.");
	    action_output(" ");
	}
    }

    /*
     * More input needed.
     * Look for the next state with a match.
     * e.g., if state is BASE, then if pred[base](), call ask[base](), which is
     *  ask_continue() and sets state to BASE+1, which is CONTINUE.
     */
    for (state = itc->state + 1; state <= ITS_GO; state++) {
	if ((*it_pred[state])(&itc->conf)) {
	    Replace(itc->prompt, (*it_ask[state])(itc));
	    itc->state = state;
	    task_request_input("Transfer", itc->prompt, it_resume, NULL, itc,
		    false);
	    return false;
	    break;
	}
    }

    return false;
}

/**
 * Per-session abort. Free the context saved for this session.
 *
 * @param[in] handle	Handle (transfer state)
 */
static void
interactive_transfer_type_abort(void *handle)
{
    itc_t *itc = (itc_t *)handle;

    if (itc != NULL) {
	ft_conf_t *p = &itc->conf;

	Replace(itc->prompt, NULL);
	Replace(p->local_filename, NULL);
	Replace(p->local_filename, NULL);
	Free(itc);
    }
}

/*
 * Start an interactive transfer.
 * Returns true if dialog in progress, false otherwise.
 */
static bool
interactive_transfer_start(void)
{
    itc_t *itc;

    /* Check for an interactive session. */
    if (!task_is_interactive()) {
	return false;
    }

    /* Check for per-type state, and allocate some if needed. */
    itc = (itc_t *)task_get_ir_state("Transfer");
    if (itc == NULL) {
	itc = (itc_t *)Calloc(1, sizeof(itc_t));
	ft_init_conf(&itc->conf);
	itc->conf.is_action = true;
	task_set_ir_state("Transfer", itc, interactive_transfer_type_abort);
    }

    /* Initialize the state. */
    itc->cr_mode = CR_REMOVE;
    itc->fe_mode = FE_KEEP;

    /* Print the banner. */
    action_output(" ");
    action_output(
"File Transfer");
    action_output(" ");
    action_output(
"Type 'quit' at any prompt to abort this dialog.");
    action_output(" ");
    action_output(
"Note: In order to initiate a file transfer, the 3270 cursor must be");
    action_output(
"positioned on an input field that can accept the IND$FILE command, e.g.,");
    action_output(
"at the VM/CMS or TSO command prompt.");
    action_output(" ");

    /* Ask about continuing. */
    itc->state = ITS_CONTINUE;
    Replace(itc->prompt, NewString("Continue? (y/n) [y] "));
    task_request_input("Transfer", itc->prompt, it_resume, NULL, itc, false);
    return true;
}

/**
 * UI hook for the Transfer() action.
 *
 * @param[in] p		Configuration (ignored)
 *
 * @returns FGI_ASYNC if started, FGI_NOP if not interactive.
 */
ft_gui_interact_t
ft_gui_interact(ft_conf_t *p)
{
    return interactive_transfer_start()? FGI_ASYNC: FGI_NOP;
}

/* ===================== Resume functions ====================== */

/* Received an answer to the initial "Continue?". */
static itret_t
it_continue(itc_t *itc, const char *response)
{
    switch (getyn_iter(1, response)) {
    case YN_YES:
	return ITR_CONTINUE;
    case YN_NO:
	return ITR_QUIT;
    default:
    case YN_RETRY:
	return ITR_RETRY;
    }
}

/* Received an answer to "Direction:" */
static itret_t
it_direction(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (*response) {
	if (!strncasecmp(response, "receive", strlen(response))) {
	    p->receive_flag = true;
	} else if (!strncasecmp(response, "send", strlen(response))) {
	    p->receive_flag = false;
	} else {
	    return ITR_RETRY;
	}
    }

    return ITR_CONTINUE;
}

/* Received an answer to "Source file:". */
static itret_t
it_source_file(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (!*response) {
	if (!((p->receive_flag && p->host_filename) ||
		    (!p->receive_flag && p->local_filename))) {
	    return ITR_RETRY;
	}
    } else {
	if (p->receive_flag) {
	    Replace(p->host_filename, NewString(response));
	} else {
	    Replace(p->local_filename, NewString(response));
	}
    }

    return ITR_CONTINUE;
}

/* Received an answer to "Destination file:". */
static itret_t
it_dest_file(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (!*response) {
	if (!((!p->receive_flag && p->host_filename) ||
	     (p->receive_flag && p->local_filename))) {
	    return ITR_RETRY;
	}
    } else {
	if (!p->receive_flag) {
	    Replace(p->host_filename, NewString(response));
	} else {
	    Replace(p->local_filename, NewString(response));
	}
    }

    return ITR_CONTINUE;
}

/* Received an answer to "Host type" */
static itret_t
it_host_type(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (*response) {
	host_type_t h;

	if (!ft_encode_host_type(response, &h)) {
	    return ITR_RETRY;
	}
	p->host_type = h;
    }

    return ITR_CONTINUE;
}

/* Received an answer to "ASCII/binary". */
static itret_t
it_ascii(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (*response) {
	if (!strncasecmp(response, "ascii", strlen(response))) {
	    p->ascii_flag = true;
	} else if (!strncasecmp(response, "binary", strlen(response))) {
	    p->ascii_flag = false;
	} else {
	    return ITR_RETRY;
	}
    }

    return ITR_CONTINUE;
}


/* Received an answer to "CR?". */
static itret_t
it_cr(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (!*response) {
	itc->cr_mode = p->cr_flag? (p->receive_flag? CR_ADD: CR_REMOVE):
			CR_KEEP;
    } else if (!strncasecmp(response, "remove", strlen(response))) {
	p->cr_flag = true;
	itc->cr_mode = CR_REMOVE;
    } else if (!strncasecmp(response, "add", strlen(response))) {
	p->cr_flag = true;
	itc->cr_mode = CR_ADD;
    } else if (!strncasecmp(response, "keep", strlen(response))) {
	p->cr_flag = false;
	itc->cr_mode = CR_KEEP;
    } else {
	return ITR_RETRY;
    }

    return ITR_CONTINUE;
}

/* Got an answer to "Remap?". */
static itret_t
it_remap(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (!response[0]) {
	if (!strncasecmp(response, "yes", strlen(response))) {
	    p->remap_flag = true;
	} else if (!strncasecmp(response, "no", strlen(response))) {
	    p->remap_flag = false;
	} else {
	    return ITR_RETRY;
	}
    }

    return ITR_CONTINUE;
}

#if defined(_WIN32) /*[*/
/* Got an answer to "Windows codepage?". */
static itret_t
it_windows_cp(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int cp = getnum_iter(p->windows_codepage, response);

    if (cp < 0) {
	return ITR_RETRY;
    }
    p->windows_codepage = cp;

    return ITR_CONTINUE;
}
#endif /*]*/

/* Got an answer to "Keep?" */
static itret_t
it_keep(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (!*response) {
	itc->fe_mode = p->allow_overwrite? FE_REPLACE:
	    (p->append_flag? FE_APPEND: FE_KEEP);
    } else if (!strncasecmp(response, "keep", strlen(response))) {
	p->append_flag = false;
	p->allow_overwrite = false;
	itc->fe_mode = FE_KEEP;
    } else if (!strncasecmp(response, "replace", strlen(response))) {
	p->append_flag = false;
	p->allow_overwrite = true;
	itc->fe_mode = FE_REPLACE;
    } else if (!strncasecmp(response, "append", strlen(response))) {
	p->append_flag = true;
	p->allow_overwrite = false;
	itc->fe_mode = FE_APPEND;
    } else {
	return ITR_RETRY;
    }

    return ITR_CONTINUE;
}

/* Got an answer to "Record format?". */
static itret_t
it_recfm(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (*response) {
	recfm_t recfm;

	if (ft_encode_recfm(response, &recfm)) {
	    p->recfm = recfm;
	} else {
	    return ITR_RETRY;
	}
    }

    return ITR_CONTINUE;
}

/* Got an answer to "Logical record length?". */
static itret_t
it_lrecl(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int lrecl = getnum_iter(p->lrecl, response);

    if (lrecl < 0) {
	return ITR_RETRY;
    }
    p->lrecl = lrecl;

    return ITR_CONTINUE;
}

/* Got an answer to "Blocksize?". */
static itret_t
it_blksize(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int blksize = getnum_iter(p->blksize, response);

    if (blksize < 0) {
	return ITR_RETRY;
    }
    p->blksize = blksize;

    return ITR_CONTINUE;
}

/* Got an answer to "Units?". */
static itret_t
it_alloc(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    units_t units;

    if (ft_encode_units(response, &units)) {
	p->units = units;
    } else {
	return ITR_RETRY;
    }

    return ITR_CONTINUE;
}

/* Got an answer for "Primary?". */
static itret_t
it_primary(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int primary = getnum_iter(p->primary_space, response);

    if (primary < 0) {
	return ITR_RETRY;
    }
    p->primary_space = primary;

    return ITR_CONTINUE;
}

/* Got an answer to "Secondary?". */
static itret_t
it_secondary(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int secondary = getnum_iter(p->secondary_space, response);

    if (secondary < 0) {
	return ITR_RETRY;
    }
    p->secondary_space = secondary;

    return ITR_CONTINUE;
}

/* Got an answer to "Avblock?". */
static itret_t
it_avblock(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int avblock = getnum_iter(p->avblock, response);

    if (avblock < 0) {
	return ITR_RETRY;
    }
    p->avblock = avblock;

    return ITR_CONTINUE;
}

/* Got an answer to "Buffer size?. */
static itret_t
it_buffer_size(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;
    int buffer_size = getnum_iter(p->dft_buffersize, response);
    int nsize;

    if (buffer_size < 0) {
	return ITR_RETRY;
    }
    nsize = set_dft_buffersize(buffer_size);
    if (nsize != buffer_size) {
	action_output("Size changed to %d.", nsize);
    }
    p->dft_buffersize = nsize;

    return ITR_CONTINUE;
}

/* Got an answer to "Other IND$FILE options?. */
static itret_t
it_other_options(itc_t *itc, const char *response)
{
    ft_conf_t *p = &itc->conf;

    if (response[0]) {
	if (!strcasecmp(response, "none")) {
	    Replace(p->other_options, NULL);
	} else {
	    p->other_options = NewString(response);
	}
    }
    return ITR_CONTINUE;
}

/* Got an answer to the final "Continue?". */
static itret_t
it_go(itc_t *itc, const char *response)
{
    int go = getyn_iter(1, response);

    if (go < 0) {
	return ITR_RETRY;
    }
    if (!go) {
	return ITR_QUIT;
    }

    return ITR_GO;
}

/* ===================== Predicates ====================== */

static bool
pred_base(ft_conf_t *p)
{
    return true;
}

static bool
pred_continue(ft_conf_t *p)
{
    return true;
}

static bool
pred_direction(ft_conf_t *p)
{
    return true;
}

static bool
pred_source_file(ft_conf_t *p)
{
    return true;
}

static bool
pred_dest_file(ft_conf_t *p)
{
    return true;
}

static bool
pred_host_type(ft_conf_t *p)
{
    return true;
}

static bool
pred_ascii(ft_conf_t *p)
{
    return true;
}

static bool
pred_cr(ft_conf_t *p)
{
    return p->ascii_flag;
}

static bool
pred_remap(ft_conf_t *p)
{
    return p->ascii_flag;
}

#if defined(_WIN32) /*[*/
static bool
pred_windows_cp(ft_conf_t *p)
{
    return p->ascii_flag && p->remap_flag;
}
#endif /*]*/

static bool
pred_keep(ft_conf_t *p)
{
    return p->receive_flag;
}

static bool
pred_recfm(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type != HT_CICS;
}

static bool
pred_lrecl(ft_conf_t *p)
{
    return !p->receive_flag && p->recfm != DEFAULT_RECFM &&
	p->host_type != HT_CICS;
}

static bool
pred_blksize(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type == HT_TSO;
}

static bool
pred_alloc(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type == HT_TSO;
}

static bool
pred_primary(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type == HT_TSO
	&& p->units != DEFAULT_UNITS;
}

static bool
pred_secondary(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type == HT_TSO
	&& p->units != DEFAULT_UNITS;
}

static bool
pred_avblock(ft_conf_t *p)
{
    return !p->receive_flag && p->host_type == HT_TSO
	&& p->units == AVBLOCK;
}

static bool
pred_buffer_size(ft_conf_t *p)
{
    return !HOST_FLAG(STD_DS_HOST);
}

static bool
pred_other_options(ft_conf_t *p)
{
    return true;
}

static bool
pred_go(ft_conf_t *p)
{
    return true;
}

/* ===================== Ask functions ====================== */

static char *
ask_continue(itc_t *itc)
{
    /* Print the banner. */
    action_output(" ");
    action_output(
"File Transfer");
    action_output(" ");
    action_output(
"Type 'quit' at any prompt to abort this dialog.");
    action_output(" ");
    action_output(
"Note: In order to initiate a file transfer, the 3270 cursor must be");
    action_output(
"positioned on an input field that can accept the IND$FILE command, e.g.,");
    action_output(
"at the VM/CMS or TSO command prompt.");
    action_output(" ");

    /* Ask about continuing. */
    return NewString("Continue? (y/n) [y] ");
}

static char *
ask_direction(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask about the direction. */
    action_output(" ");
    action_output(
"'send' means copy a file from this workstation to the host.");
    action_output(
"'receive' means copy a file from the host to this workstation.");

    return Asprintf("Direction: (send/receive) [%s] ",
	    p->receive_flag? "receive": "send");
}

static char *
ask_source_file(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;
    char *default_file;

    /* Ask about the source file. */
    if (p->receive_flag && p->host_filename) {
	default_file = txAsprintf(" [%s]", p->host_filename);
    } else if (!p->receive_flag && p->local_filename) {
	default_file = txAsprintf(" [%s]", p->local_filename);
    } else {
	default_file = "";
    }

    action_output(" ");

    return Asprintf("Name of source file on %s:%s ",
	    p->receive_flag? "the host": "this workstation", default_file);
}

static char *
ask_dest_file(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;
    char *default_file;

    /* Ask about the destination file. */
    if (!p->receive_flag && p->host_filename) {
	default_file = txAsprintf(" [%s]", p->host_filename);
    } else if (p->receive_flag && p->local_filename) {
	default_file = txAsprintf(" [%s]", p->local_filename);
    } else {
	default_file = "";
    }
    return Asprintf("Name of destination file%s on %s:%s ",
	    p->receive_flag? " or folder": "",
	    p->receive_flag? "this workstation": "the host",
	    default_file);
}

static char *
ask_host_type(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask about the host type. */
    action_output(" ");
    return Asprintf("Host type: (tso/vm/cics) [%s] ",
	    ft_decode_host_type(p->host_type));
}

static char *
ask_ascii(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    action_output(" ");
    action_output(
"An 'ascii' transfer does automatic translation between EBCDIC on the host and");
    action_output(
"ASCII on the workstation.");
    action_output(
"A 'binary' transfer does no data translation.");

    return Asprintf("Transfer mode: (ascii/binary) [%s] ",
	    p->ascii_flag? "ascii": "binary");
}

static char *
ask_cr(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;
    char *default_cr;

    /* Ask about CR handling. */
    action_output(" ");
    action_output(
"For ASCII transfers, carriage return (CR) characters can be handled specially.");
    if (p->receive_flag) {
	action_output(
"'add' means that CRs will be added to each record during the transfer.");
    } else {
	action_output(
"'remove' means that CRs will be removed during the transfer.");
    }
    action_output(
"'keep' means that no special action is taken with CRs.");
    default_cr = p->cr_flag? (p->receive_flag? "add": "remove"): "keep";
    return Asprintf("CR handling: (%s/keep) [%s] ",
	    p->receive_flag? "add": "remove", default_cr);
}

static char *
ask_remap(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask about character set remapping. */
    fmt80(txAsprintf("For ASCII transfers, "
#if defined(WC3270) /*[*/
"w"
#endif /*]*/
"c3270 can either remap the text to ensure as "
"accurate a translation between "
#if defined(WC3270) /*[*/
"the Windows code page"
#else /*][*/
"%s"
#endif /*]*/
" and EBCDIC code page %s as possible, or it can transfer text as-is and "
"leave all translation to the IND$FILE program on the host.\n\
'yes' means that text will be translated.\n\
'no' means that text will be transferred as-is.",
#if !defined(WC3270) /*[*/
	locale_codeset,
#endif /*]*/
	get_codepage_number()));
    return Asprintf("Re-map character set? (yes/no) [%s] ",
	    p->remap_flag? "yes": "no");
}

#if defined(_WIN32) /*[*/
static char *
ask_windows_cp(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask about the Windows code page. */
    return Asprintf("Windows code page for re-mapping: [%d] ",
	    p->windows_codepage);
}
#endif /*]*/

static char *
ask_keep(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;
    char *default_fe;

    action_output(" ");
    action_output(
"If the destination file exists, you can choose to keep it (and abort the");
    action_output(
"transfer), replace it, or append the source file to it.");
    if (p->allow_overwrite) {
	default_fe = "replace";
    } else if (p->append_flag) {
	default_fe = "append";
    } else {
	default_fe = "keep";
    }
    return Asprintf("Action if destination file exists: "
	    "(keep/replace/append) [%s] ", default_fe);
}

static char *
ask_recfm(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for the record format. */
    action_output("[optional] Destination file record format:");
    return Asprintf(" (default/fixed/variable/undefined) [%s] ",
	    ft_decode_recfm(p->recfm));
}

static char *
ask_lrecl(itc_t *itc)
{
    /* Ask for the logical record length. */
    return NewString("[optional] Destination file logical record length: ");
}

static char *
ask_blksize(itc_t *itc)
{
    /* Ask for the block size. */
    return NewString("[optional] Destination file block size: ");
}

static char *
ask_alloc(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for units. */
    action_output("[optional] Destination file allocation type:");
    return Asprintf(" (default/tracks/cylinders/avblock) [%s] ",
	    ft_decode_units(p->units));
}

static char *
ask_primary(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for primary allocation. */
    if (p->primary_space) {
	return Asprintf("Destination file primary space: [%d]",
		p->primary_space);
    } else {
	return  NewString("Destination file primary space: ");
    }
}

static char *
ask_secondary(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for secondary. */
    if (p->secondary_space) {
	return Asprintf("Destination file secondary space: [%d]",
		p->secondary_space);
    } else {
	return NewString("Destination file secondary space: ");
    }
}

static char *
ask_avblock(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for AVBLOCK. */
    if (p->avblock) {
	return Asprintf("Destination file avblock size: [%d]", p->avblock);
    } else {
	return NewString("Destination file abvlock size: ");
    }
}

static char *
ask_buffer_size(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for the DFT buffer size. */
    action_output(" ");
    return Asprintf("DFT buffer size: [%d] ", p->dft_buffersize);
}

static char *
ask_other_options(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;

    /* Ask for the other IND$FILE options. */
    action_output(" ");
    action_output("You can specify additional options to pass to the IND$FILE command on the host.");
    if (p->other_options) {
	action_output("Enter 'none' to specify no additional options.");
    }
    return Asprintf("Other IND$FILE options: [%s] ", p->other_options? p->other_options: "");
}

static char *
ask_go(itc_t *itc)
{
    ft_conf_t *p = &itc->conf;
    char *ht = "";
    char *cr = "";
    char *remap = "";
    char *windows_cp = "";

    /* Sum up and ask about starting the transfer. */
    action_output(" ");
    action_output("File Transfer Summary:");

    if (p->receive_flag) {
	action_output(" Source file on Host: %s", p->host_filename);
	action_output(" Destination file on Workstation: %s",
		p->local_filename);
    } else {
	action_output(" Source file on workstation: %s", p->local_filename);
	action_output(" Destination file on Host: %s", p->host_filename);
    }

    switch (p->host_type) {
    case HT_TSO:
	ht = "TSO";
	break;
    case HT_VM:
	ht = "VM/CMS";
	break;
    case HT_CICS:
	ht = "CICS";
	break;
    }
    action_output(" Host type: %s", ht);

    if (p->ascii_flag) {
	switch (itc->cr_mode) {
	case CR_REMOVE:
	    cr = ", remove CRs";
	    break;
	case CR_ADD:
	    cr = ", add CRs";
	    break;
	case CR_KEEP:
	    break;
	}
	if (p->remap_flag) {
	    remap = ", remap text";
	} else {
	    remap = ", don't remap text";
	}
#if defined(_WIN32) /*[*/
	if (p->remap_flag) {
	    windows_cp = txAsprintf(", Windows code page %d", p->windows_codepage);
	}
#endif /*]*/
    }
    action_output(" ");
    action_output(" Transfer mode: %s%s%s%s",
	    p->ascii_flag? "ASCII": "Binary",
	    cr,
	    remap,
	    windows_cp);

    if (p->receive_flag) {
	char *exists = "";

	switch (itc->fe_mode) {
	case FE_KEEP:
	    exists = "abort the transfer";
	    break;
	case FE_REPLACE:
	    exists = "replace it";
	    break;
	case FE_APPEND:
	    exists = "append to it";
	    break;
	}
	action_output(" If destination file exists, %s", exists);
    }

    if (!p->receive_flag &&
	    (p->recfm != DEFAULT_RECFM || p->lrecl || p->primary_space ||
	     p->secondary_space)) {

	action_output(" Destination file:");

	switch (p->recfm) {
	case DEFAULT_RECFM:
	    break;
	case RECFM_FIXED:
	    action_output("  Record format: fixed");
	    break;
	case RECFM_VARIABLE:
	    action_output("  Record format: variable");
	    break;
	case RECFM_UNDEFINED:
	    action_output("  Record format: undefined");
	    break;
	}
	if (p->lrecl) {
	    action_output("  Logical record length: %d", p->lrecl);
	}
	if (p->blksize) {
	    action_output("  Block size: %d", p->blksize);
	}

	if (p->primary_space || p->secondary_space) {
	    char *primary = "";
	    char *secondary = "";
	    char *units = "";

	    if (p->primary_space) {
		primary = txAsprintf(" primary %d", p->primary_space);
	    }
	    if (p->secondary_space) {
		secondary = txAsprintf(" secondary %d", p->secondary_space);
	    }
	    switch (p->units) {
	    case DEFAULT_UNITS:
		break;
	    case TRACKS:
		units = " tracks";
		break;
	    case CYLINDERS:
		units = " cylinders";
		break;
	    case AVBLOCK:
		units = txAsprintf(" avblock %d", p->avblock);
		break;
	    }
	    action_output("  Allocation:%s%s%s",
		    primary,
		    secondary,
		    units);
	}
    }

    if (!HOST_FLAG(STD_DS_HOST)) {
	action_output(" DFT buffer size: %d", p->dft_buffersize);
    }
    if (p->other_options) {
	action_output(" Other IND$FILE options: %s", p->other_options);
    }

    action_output(" ");

    return NewString("Continue? (y/n) [y] ");
}

/* Help for the interactive Transfer action. */
void
ft_help(bool as_action _is_unused)
{
    ft_conf_t conf;
    char *s;

    memset(&conf, 0, sizeof(ft_conf_t));
    ft_init_conf(&conf);
    action_output(
"Syntax:\n\
  To be prompted interactively for parameters:\n\
    Transfer()\n\
  To specify parameters on the command line:\n\
    Transfer(<keyword>=<value>...)\n\
    or Transfer(<keyword>,<value>...)\n\
  To do a transfer using the current defaults:\n\
    Transfer(defaults)\n\
  To cancel a transfer in progress:\n\
    Transfer(cancel)\n\
Keywords:");

    action_output(
"  direction=send|receive               default '%s'",
	    conf.receive_flag? "send": "receive");

    if ((conf.receive_flag && conf.host_filename) ||
	    (!conf.receive_flag && conf.local_filename)) {
	s = txAsprintf("default '%s'",
		conf.receive_flag? conf.host_filename: conf.local_filename);
    } else {
	s = "(required)";
    }
    action_output(
"  hostfile=<path>                      %s", s);

    if ((!conf.receive_flag && conf.host_filename) ||
	    (conf.receive_flag && conf.local_filename)) {
	s = txAsprintf("default '%s'",
		conf.receive_flag? conf.local_filename: conf.host_filename);
    } else {
	s = "(required)";
    }
    action_output(
"  localfile=<path>                     %s", s);

    action_output(
"  host=tso|vm|cics                     default '%s'",
	    ft_decode_host_type(conf.host_type));
    action_output(
"  mode=ascii|binary                    default '%s'",
	    conf.ascii_flag? "ascii": "binary");
    action_output(
"  cr=remove|add|keep                   default '%s'",
	    conf.cr_flag? (conf.receive_flag? "add": "remove"): "keep");
    action_output(
"  remap=yes|no                         default '%s'",
	    conf.remap_flag? "yes": "no");
#if defined(_WIN32) /*[*/
    action_output(
"  windowscodepage=<n>                  default %d",
	    conf.windows_codepage);
#endif /*]*/
    action_output(
"  exist=keep|replace|append            default '%s'",
	    conf.allow_overwrite? "replace":
		(conf.append_flag? "append": "keep"));
    action_output(
"  recfm=fixed|variable|undefined       for direction=send");
    if (conf.recfm != DEFAULT_RECFM) {
	action_output(
"                                        default '%s'",
		ft_decode_recfm(conf.recfm));
    }
    action_output(
"  lrecl=<n>                            for direction=send");
    if (conf.lrecl) {
	action_output(
"                                        default %d",
		conf.lrecl);
    }
    action_output(
"  blksize=<n>                          for direction=send host=tso");
    if (conf.blksize) {
	action_output(
"                                        default %d",
		conf.blksize);
    }
    action_output(
"  allocation=tracks|cylinders|avblock  for direction=send host=tso");
    if (conf.units != DEFAULT_UNITS) {
	action_output(
"                                        default '%s'",
		ft_decode_units(conf.units));
    }
    action_output(
"  primaryspace=<n>                     for direction=send host=tso");
    if (conf.primary_space) {
	action_output(
"                                        default %d",
		conf.primary_space);
    }
    action_output(
"  secondaryspace=<n>                   for direction=send host=tso");
    if (conf.secondary_space) {
	action_output(
"                                        default %d",
		conf.secondary_space);
    }
    action_output(
"  avblock=<n>                          for direction=send host=tso allocation=avblock");
    if (conf.avblock) {
	action_output(
"                                        default %d",
		conf.avblock);
    }
    action_output(
"  buffersize=<n>                       default %d",
		conf.dft_buffersize? conf.dft_buffersize: DFT_BUF);
    action_output(
"  otheroptions=<text>                  other options for IND$FILE");
    action_output(
"Note that when you use <keyword>=<value> syntax, to embed a space in a value,\n\
you must include the keyword inside the quotes, e.g.:\n\
  Transfer(direction=send,localfile=/tmp/foo,\"hostfile=foo text a\",host=vm)");

    if (conf.local_filename) {
	Free(conf.local_filename);
    }
    if (conf.host_filename) {
	Free(conf.host_filename);
    }
}
