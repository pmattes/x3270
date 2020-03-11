/*
 * Copyright (c) 2016-2020 Paul Mattes.
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
#include "popups.h"
#include "screen.h"
#include "telnet.h"
#include "toggles.h"
#include "utils.h"

#include "model.h"

static char *pending_model;
static char *pending_oversize;

/*
 * Canonical representation of the model, returning color and extended
 * state.
 */
static char *
canonical_model_x(const char *res, int *model, bool *is_color,
	bool *is_extended)
{
    size_t sl;
    char *digitp;
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
    return xs_buffer("327%c-%c%s", *colorp, *digitp, extended? "-E": "");
}

/*
 * Canonical representation of the model.
 */
static char *
canonical_model(const char *res)
{
    int model;
    bool color, extended;

    return canonical_model_x(res, &model, &color, &extended);
}

/*
 * Toggle the model.
 */
static bool
toggle_model(const char *name _is_unused, const char *value)
{
    if (!model_can_change()) {
	popup_an_error("Cannot change " ResModel);
	return false;
    }

    Replace(pending_model, *value? NewString(value): NULL);
    return true;
}

/*
 * Toggle oversize.
 */
static bool
toggle_oversize(const char *name _is_unused, const char *value)
{
    if (!model_can_change()) {
	popup_an_error("Cannot change " ResOversize);
	return false;
    }

    Replace(pending_oversize, NewString(value));
    return true;
}

/*
 * Done function for changing the model and oversize.
 */
static bool
toggle_model_done(bool success)
{
    unsigned ovr = 0, ovc = 0;
    int model_number = model_num;
    bool is_color = mode.m3279;
    bool is_extended = mode.extended;
    struct {
	int model_num;
	int rows;
	int cols;
	int ov_cols;
	int ov_rows;
	bool m3279;
	bool alt;
	bool extended;
    } old;
    bool oversize_was_pending = (pending_oversize != NULL);
    bool res = true;

    if (!success ||
	    (pending_model == NULL &&
	     pending_oversize == NULL)) {
	goto done;
    }

    if (PCONNECTED) {
	popup_an_error("Cannot change %s or %s while connected",
		ResModel, ResOversize);
	goto fail;
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
	char *canon = canonical_model_x(pending_model, &model_number,
		&is_color, &is_extended);

	if (canon == NULL) {
	    popup_an_error("%s value must be 327{89}-{2345}[-E]",
		    ResModel);
	    goto fail;
	}

	Replace(pending_model, canon);
    }

    if (!is_extended) {
	/* Without extended, no oversize. */
	Replace(pending_oversize, NewString(""));
    }

    if (pending_oversize != NULL) {
	if (*pending_oversize) {
	    char x, junk;
	    if (sscanf(pending_oversize, "%u%c%u%c", &ovc, &x, &ovr, &junk) != 3
		    || x != 'x') {
		popup_an_error("%s value must be <cols>x<rows>",
			ResOversize);
		goto fail;
	    }
	} else {
	    ovc = 0;
	    ovr = 0;
	}
    } else {
	ovc = ov_cols;
	ovr = ov_rows;
    }

    /* Save the current settings. */
    old.model_num = model_num;
    old.rows = ROWS;
    old.cols = COLS;
    old.ov_rows = ov_rows;
    old.ov_cols = ov_cols;
    old.m3279 = mode.m3279;
    old.alt = screen_alt;
    old.extended = mode.extended;

    /* Change settings. */
    mode.m3279 = is_color;
    mode.extended = is_extended;
    set_rows_cols(model_number, ovc, ovr);

    if (model_num != model_number ||
	    ov_rows != (int)ovr ||
	    ov_cols != (int)ovc) {
	/* Failed. Restore the old settings. */
	mode.m3279 = old.m3279;
	set_rows_cols(old.model_num, old.ov_cols, old.ov_rows);
	ROWS = old.rows;
	COLS = old.cols;
	screen_alt = old.alt;
	mode.extended = old.extended;
	return false;
    }

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
    res = false;

done:
    Replace(pending_model, NULL);
    Replace(pending_oversize, NULL);
    return res;
}

/*
 * Terminal name toggle.
 */
static bool
toggle_terminal_name(const char *name _is_unused, const char *value)
{
    if (PCONNECTED) {
	popup_an_error("%s cannot change while connected",
		ResTermName);
	return false;
    }

    appres.termname = clean_termname(*value? value: NULL);
    net_set_default_termtype();
    return true;
}

/*
 * Toggle the NOP interval.
 */
static bool
toggle_nop_seconds(const char *name _is_unused, const char *value)
{
    unsigned long l;
    char *end;
    int secs;

    if (!*value) {
	appres.nop_seconds = 0;
	net_nop_seconds();
	return true;
    }

    l = strtoul(value, &end, 10);
    secs = (int)l;
    if (*end != '\0' || (unsigned long)secs != l || secs < 0) {
	popup_an_error("Invalid %s value", ResNopSeconds);
	return false;
    }
    appres.nop_seconds = secs;
    net_nop_seconds();
    return true;
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
	    NULL, (void **)&appres.oversize, XRM_STRING);
    register_extended_toggle(ResTermName, toggle_terminal_name, NULL, NULL,
	    (void **)&appres.termname, XRM_STRING);
    register_extended_toggle(ResNopSeconds, toggle_nop_seconds, NULL,
	    NULL, (void **)&appres.nop_seconds, XRM_INT);
}
