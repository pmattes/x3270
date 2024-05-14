/*
 * Copyright (c) 2016-2024 Paul Mattes.
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
 *     * Neither the name of Paul Mattes, nor his contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	model.c
 *		Support for changing model and oversize at run-time.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "ctlrc.h"
#include "host.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "telnet.h"
#include "toggles.h"
#include "txa.h"
#include "utils.h"

#include "model.h"

static char *pending_model;
static char *pending_oversize;

/*
 * Canonical representation of the model, returning color and extended
 * state.
 */
static const char *
canonical_model_x(const char *res, int *model, bool *is_color,
	bool *is_extended)
{
    size_t sl;
    char *digitp = NULL;
    char *colorp = "9";
    bool extended = false;

    if (res == NULL) {
	return NULL;
    }
    sl = strlen(res);
    if ((sl != 1 && sl != 6 && sl != 8) ||
	(sl == 1 &&
	 (digitp = strchr("2345", res[0])) == NULL) ||
	(((sl == 6) || (sl == 8)) &&
	 (strncmp(res, "327", 3) ||
	  (colorp = strchr("89", res[3])) == NULL ||
	  res[4] != '-' ||
	  (digitp = strchr("2345", res[5])) == NULL)) ||
	((sl == 8) &&
	 (res[6] != '-' || strchr("Ee", res[7]) == NULL))) {
	return NULL;
    }
    if (sl == 1 || sl == 8) {
	extended = true;
    }
    *model = *digitp - '0';
    *is_color = (*colorp == '9');
    *is_extended = extended;
    return txAsprintf("327%c-%c%s", *colorp, *digitp, extended? "-E": "");
}

/*
 * Canonical representation of the model.
 */
static const char *
canonical_model(const char *res)
{
    int model;
    bool color, extended;

    return canonical_model_x(res, &model, &color, &extended);
}

/*
 * Canonical representation of oversize.
 */
static const char *
canonical_oversize_x(const char *res, unsigned *ovc, unsigned *ovr)
{
    char x, junk;

    if (res == NULL) {
	return NULL;
    }
    if (sscanf(res, "%u%c%u%c", ovc, &x, ovr, &junk) != 3 || x != 'x') {
	return NULL;
    }
    return txAsprintf("%ux%u", *ovc, *ovr);
}

/*
 * Canonical representation of oversize.
 */
static const char *
canonical_oversize(const char *res)
{
    unsigned ovc, ovr;

    return canonical_oversize_x(res, &ovc, &ovr);
}

/*
 * Toggle the model.
 */
static toggle_upcall_ret_t
toggle_model(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    if (!model_can_change()) {
	popup_an_error("Cannot change " ResModel);
	return TU_FAILURE;
    }

    Replace(pending_model, *value? NewString(value): NULL);
    return TU_SUCCESS;
}

/*
 * Toggle oversize.
 */
static toggle_upcall_ret_t
toggle_oversize(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    if (!model_can_change()) {
	popup_an_error("Cannot change " ResOversize);
	return TU_FAILURE;
    }

    Replace(pending_oversize, NewString(value));
    return TU_SUCCESS;
}

/*
 * Done function for changing the model and oversize.
 */
static toggle_upcall_ret_t
toggle_model_done(bool success, unsigned flags, ia_t ia)
{
    unsigned ovr = 0, ovc = 0;
    int model_number = model_num;
    bool is_color = mode.m3279;
    bool is_extended = mode.extended;
    bool oversize_was_pending = (pending_oversize != NULL);
    toggle_upcall_ret_t res = TU_SUCCESS;

    if (!success ||
	    (pending_model == NULL &&
	     pending_oversize == NULL)) {
	goto done;
    }

    if (pending_model != NULL && !strcmp(pending_model, appres.model)) {
	Replace(pending_model, NULL);
    }
    if (pending_oversize != NULL &&
	    appres.oversize != NULL &&
	    !strcmp(pending_oversize, appres.oversize)) {
	Replace(pending_oversize, NULL);
    }
    if (pending_model == NULL && pending_oversize == NULL) {
	goto done;
    }

    /* Reconcile simultaneous changes. */
    if (pending_model != NULL) {
	const char *canon = canonical_model_x(pending_model, &model_number,
		&is_color, &is_extended);

	if (canon == NULL) {
	    popup_an_error("%s value must be 327{89}-{2345}[-E]",
		    ResModel);
	    goto fail;
	}

	Replace(pending_model, NewString(canon));
    }

    if (!is_extended) {
	/* Without extended, no oversize. */
	Replace(pending_oversize, NewString(""));
    }

    if (pending_oversize != NULL) {
	if (*pending_oversize) {
	    const char *canon = canonical_oversize_x(pending_oversize, &ovc, &ovr);

	    if (canon == NULL) {
		popup_an_error("%s value must be <cols>x<rows>", ResOversize);
		goto fail;
	    }
	    Replace(pending_oversize, NewString(canon));
	} else {
	    ovc = 0;
	    ovr = 0;
	}
    } else {
	ovc = ov_cols;
	ovr = ov_rows;
    }

    /* Check settings. */
    if (!check_rows_cols(model_number, ovc, ovr)) {
	goto fail;
    }

    /* Check connection state. */
    if (cstate >= TELNET_PENDING) {
	/* A change is not valid in this state. */
	if (!(flags & XN_DEFER)) {
	    /* Not valid in this state. */
	    popup_an_error("Cannot change %s or %s while connected", ResModel, ResOversize);
	    goto fail;
	}

	/* Queue up the changes for when we disconnect. */
	if (pending_model != NULL) {
	    toggle_save_disconnect_set(ResModel, pending_model, ia);
	}
	if (pending_oversize != NULL) {
	    toggle_save_disconnect_set(ResOversize, pending_oversize, ia);
	}
	res = TU_DEFERRED;
	goto done;
    }

    /* Change settings. */
    mode.m3279 = is_color;
    mode.extended = is_extended;
    set_rows_cols(model_number, ovc, ovr);

    /* Finish the rest of the switch. */
    ROWS = maxROWS;
    COLS = maxCOLS;
    ctlr_reinit(MODEL_CHANGE);

    /* Reset the screen state. */
    screen_init();
    ctlr_erase(true);

    /* Report the new terminal name. */
    if (appres.termname == NULL) {
	st_changed(ST_TERMINAL_NAME, appres.termname != NULL);
    }

    if (pending_model != NULL) {
	Replace(appres.model, pending_model);
    }
    pending_model = NULL;
    if (pending_oversize != NULL) {
	if (*pending_oversize) {
	    Replace(appres.oversize, pending_oversize);
	    pending_oversize = NULL;
	} else {
	    bool force = !oversize_was_pending && appres.oversize != NULL;

	    Replace(appres.oversize, NULL);
	    if (force) {
		/* Turning off extended killed oversize. */
		force_toggle_notify(ResOversize, IA_NONE);
	    }
	}
    }

    goto done;

fail:
    res = TU_FAILURE;

done:
    Replace(pending_model, NULL);
    Replace(pending_oversize, NULL);
    return res;
}

/*
 * Terminal name toggle.
 */
static toggle_upcall_ret_t
toggle_terminal_name(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    if (cstate >= TELNET_PENDING) {
	popup_an_error("%s cannot change while connected",
		ResTermName);
	return TU_FAILURE;
    }

    appres.termname = clean_termname(*value? value: NULL);
    net_set_default_termtype();
    return TU_SUCCESS;
}

/*
 * Toggle the NOP interval.
 */
static toggle_upcall_ret_t
toggle_nop_seconds(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    unsigned long l;
    char *end;
    int secs;

    if (!*value) {
	appres.nop_seconds = 0;
	net_nop_seconds();
	return TU_SUCCESS;
    }

    l = strtoul(value, &end, 10);
    secs = (int)l;
    if (*end != '\0' || (unsigned long)secs != l || secs < 0) {
	popup_an_error("Invalid %s value", ResNopSeconds);
	return TU_FAILURE;
    }
    appres.nop_seconds = secs;
    net_nop_seconds();
    return TU_SUCCESS;
}

/**
 * Module registration.
 */
void
model_register(void)
{
    /* Register the toggles. */
    register_extended_toggle(ResModel, toggle_model, toggle_model_done,
	    canonical_model, (void **)&appres.model, XRM_STRING);
    register_extended_toggle(ResOversize, toggle_oversize, toggle_model_done,
	    canonical_oversize, (void **)&appres.oversize, XRM_STRING);
    register_extended_toggle(ResTermName, toggle_terminal_name, NULL, NULL,
	    (void **)&appres.termname, XRM_STRING);
    register_extended_toggle(ResNopSeconds, toggle_nop_seconds, NULL,
	    NULL, (void **)&appres.nop_seconds, XRM_INT);
}
