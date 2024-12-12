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

#include "boolstr.h"
#include "ctlrc.h"
#include "host.h"
#include "names.h"
#include "popups.h"
#include "product.h"
#include "screen.h"
#include "telnet.h"
#include "toggles.h"
#include "txa.h"
#include "utils.h"

#include "model.h"

static char *pending_model;
static char *pending_oversize;
static char *pending_extended_data_stream;

/**
 * Canonical representation of the model, returning color and extended state.
 *
 * @param[in] res	Resource (raw) value
 * @param[out] canon	Returned canonical representation
 * @param[out] model	Returned model number
 * @param[out] is_color	Returned true if it's a 3279
 * @param[in] extended	Append -E to canon if true
 *
 * @returns true for success, false for error.
 */
static bool
canonical_model_x(const char *res, const char **canon, int *model, bool *is_color, bool extended)
{
    size_t sl;
    char *digitp = NULL;
    char *colorp = "9";

    if (res == NULL) {
	return false;
    }
    if (!strncasecmp("IBM-", res, 4)) {
	res += 4;
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
	return false;
    }
    if (model != NULL) {
	*model = *digitp - '0';
    }
    if (is_color != NULL) {
	*is_color = (*colorp == '9');
    }
    if (canon != NULL) {
	*canon = txAsprintf("327%c-%c%s", *colorp, *digitp, extended? "-E": "");
    }
    return true;
}

/*
 * Canonical representation of a model, for toggle operations.
 */
static const char *
canonical_model(const char *res)
{
    const char *canon;

    if (!canonical_model_x(res, &canon, NULL, NULL, appres.extended_data_stream)) {
	return NULL;
    }
    return canon;
}

/* Get the model, for display purposes. */
const char *
get_model(void)
{
    return canonical_model(appres.model);
}

/* Get the model, including the IBM- at the front. */
const char *
get_full_model(void)
{
    return txAsprintf("IBM-%s", get_model());
}

/* Create a canonical model name. */
char *
create_model(int model_num, bool color)
{
    return Asprintf("327%c-%d", color? '9': '8', model_num);
}

/* Set up the model. */
int
common_model_init(void)
{
    int model_number = 0;
    bool is_color = true;

    if (appres.model != NULL && appres.model[0]) {
	if (!canonical_model_x(appres.model, NULL, &model_number, &is_color, false)) {
	    popup_an_error("Invalid model number: %s", appres.model);
	}
    }
    if (!model_number) {
        model_number = 4;
    }
    mode3279 = !appres.interactive.mono && is_color;
    Replace(appres.model, create_model(model_number, mode3279));
    return model_number;
}

/*
 * Canonical representation of oversize.
 */
static const char *
canonical_oversize_x(const char *res, unsigned *ovc, unsigned *ovr)
{
    unsigned x_ovc, x_ovr;
    char x, junk;

    if (res != NULL && (product_auto_oversize() && !strcasecmp(appres.oversize, KwAuto))) {
	return KwAuto;
    }

    if (res == NULL ||
	    sscanf(res, "%u%c%u%c", &x_ovc, &x, &x_ovr, &junk) != 3 ||
	    strchr("Xx", x) == NULL) {
	return NULL;
    }

    if (ovc != NULL) {
	*ovc = x_ovc;
    }
    if (ovr != NULL) {
	*ovr = x_ovr;
    }
    return txAsprintf("%ux%u", x_ovc, x_ovr);
}

/*
 * Canonical representation of oversize, for toggles.
 */
static const char *
canonical_oversize(const char *res)
{
    return canonical_oversize_x(res, NULL, NULL);
}

/* Set up oversize, canonicalizing the value and setting up the model- and oversize-related globals. */
void
oversize_init(int model_number)
{
    int ovc = 0;
    int ovr = 0;

    if (!appres.extended_data_stream) {
        Replace(appres.oversize, NULL);
    }
    if (appres.oversize != NULL) {
	if (product_auto_oversize() && !strcasecmp(appres.oversize, KwAuto)) {
            ovc = -1;
            ovr = -1;
        } else {
	    const char *canon = canonical_oversize_x(appres.oversize, (unsigned *)&ovc, (unsigned *)&ovr);

	    if (canon != NULL) {
		Replace(appres.oversize, NewString(canon));
	    } else {
		xs_warning("Invalid %s value '%s'", ResOversize, appres.oversize);
		Replace(appres.oversize, NULL);
	    }
	}
    }
    set_rows_cols(model_number, ovc, ovr);
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
 * Toggle extended data stream mode.
 */
static toggle_upcall_ret_t
toggle_extended_data_stream(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    if (!model_can_change()) {
	popup_an_error("Cannot change " ResExtendedDataStream);
	return TU_FAILURE;
    }

    Replace(pending_extended_data_stream, *value? NewString(value): NULL);
    return TU_SUCCESS;
}

/*
 * Done function for changing the model, oversize and extended mode.
 */
static toggle_upcall_ret_t
toggle_model_done(bool success, unsigned flags, ia_t ia)
{
    unsigned ovr = 0, ovc = 0;
    int model_number = model_num;
    bool is_color = mode3279;
    bool oversize_was_pending = (pending_oversize != NULL);
    bool xext = appres.extended_data_stream;
    toggle_upcall_ret_t res = TU_SUCCESS;

    if (!success ||
	    (pending_model == NULL &&
	     pending_oversize == NULL &&
	     pending_extended_data_stream == NULL)) {
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
    if (pending_extended_data_stream != NULL &&
	    boolstr(pending_extended_data_stream, &xext) == NULL &&
	    appres.extended_data_stream == xext) {
	Replace(pending_extended_data_stream, NULL);
    }
    if (pending_model == NULL && pending_oversize == NULL && pending_extended_data_stream == NULL) {
	goto done;
    }

    /* Reconcile simultaneous changes. */
    if (pending_extended_data_stream != NULL) {
	if (boolstr(pending_extended_data_stream, &xext) != NULL) {
	     popup_an_error("Invalid " ResExtendedDataStream);
	    goto fail;
	}
    }
    if (pending_extended_data_stream != NULL || pending_model != NULL) {
	const char *canon;

	if (!canonical_model_x(pending_model? pending_model: appres.model, &canon, &model_number, &is_color, false)) {
	    popup_an_error("%s value must be 327{89}-{2345}[-E]", ResModel);
	    goto fail;
	}

	if (pending_model != NULL) {
	    char *new_model = NewString(canon);
	    if (appres.interactive.mono) {
		/* You can't change to a color model when in mono mode. */
		new_model[3] = '8';
		is_color = false;
	    }
	    Replace(pending_model, new_model);
	}
    }

    if (!xext) {
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
	if (pending_extended_data_stream != NULL) {
	    toggle_save_disconnect_set(ResExtendedDataStream, pending_extended_data_stream, ia);
	}
	res = TU_DEFERRED;
	goto done;
    }

    /* Change settings. */
    mode3279 = is_color;
    if (pending_extended_data_stream != NULL) {
	appres.extended_data_stream = xext;
    }
    if (pending_model != NULL || (pending_oversize != NULL && *pending_oversize)) {
	set_rows_cols(model_number, ovc, ovr);
    }

    /* Finish the rest of the switch. */
    if (pending_model != NULL || (pending_oversize != NULL && *pending_oversize)) {
	ROWS = maxROWS;
	COLS = maxCOLS;
	ctlr_reinit(MODEL_CHANGE);
    }

    /* Reset the screen state. */
    screen_change_model(model_number, ovc, ovr);
    if (pending_model != NULL || (pending_oversize != NULL && *pending_oversize)) {
	ctlr_erase(true);
    }

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
    if (pending_extended_data_stream != NULL) {
	appres.extended_data_stream = xext;
    }
    Replace(pending_extended_data_stream, NULL);

    net_set_default_termtype();

    goto done;

fail:
    res = TU_FAILURE;

done:
    Replace(pending_model, NULL);
    Replace(pending_oversize, NULL);
    Replace(pending_extended_data_stream, NULL);
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
    register_extended_toggle(ResExtendedDataStream, toggle_extended_data_stream,
	    toggle_model_done, NULL, (void **)&appres.extended_data_stream, XRM_BOOLEAN);
    register_extended_toggle(ResModel, toggle_model, toggle_model_done,
	    canonical_model, (void **)&appres.model, XRM_STRING);
    register_extended_toggle(ResNopSeconds, toggle_nop_seconds, NULL,
	    NULL, (void **)&appres.nop_seconds, XRM_INT);
    register_extended_toggle(ResOversize, toggle_oversize, toggle_model_done,
	    canonical_oversize, (void **)&appres.oversize, XRM_STRING);
    register_extended_toggle(ResTermName, toggle_terminal_name, NULL, NULL,
	    (void **)&appres.termname, XRM_STRING);
}
