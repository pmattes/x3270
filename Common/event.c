/*
 * Copyright (c) 1993-2015, 2019-2020 Paul Mattes.
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
 *	event.c
 *		Event propagation.
 */

#include "globals.h"

#include "telnet.h"
#include "trace.h"
#include "utils.h"

typedef struct st_callback {
    llist_t list;
    schange_callback_t *func;
    unsigned short order;
} st_callback_t;
llist_t st_callbacks[N_ST];

static bool schange_initted = false;

/* Callback initialization. */
static void
init_schange(void)
{
    if (!schange_initted) {
	int i;

	for (i = 0; i < N_ST; i++) {
	    llist_init(&st_callbacks[i]);
	}

	schange_initted = true;
    }
}

/*
 * Register a function with a particular order.
 * 'order' can be:
 *   ORDER_DONTCARE (65534)	insert anywhere (will actually queue up before
 *   				 any 'last')
 *   ORDER_LAST (65535)		insert last (the order of multiple 'lasts' is
 *   				 undefined)
 *   0 through 65533		specific ordering
 */
void
register_schange_ordered(enum st tx, schange_callback_t *func,
	unsigned short order)
{
    st_callback_t *st;
    st_callback_t *before;

    /* Get the lists set up. */
    init_schange();

    st = (struct st_callback *)Malloc(sizeof(*st));
    llist_init(&st->list);
    st->func = func;
    st->order = order;

    FOREACH_LLIST(&st_callbacks[tx], before, st_callback_t *) {
	if (order < before->order) {
	    llist_insert_before(&st->list, &before->list);
	    return;
	}
    } FOREACH_LLIST_END(&st_callbacks[tx], before, st_callback_t *);

    /* Put it at the end. */
    LLIST_APPEND(&st->list, st_callbacks[tx]);
}

/* Register a function interested in a state change. */
void
register_schange(enum st tx, schange_callback_t *func)
{
    register_schange_ordered(tx, func, ORDER_DONTCARE);
}

/* Signal a state change. */
void
st_changed(enum st tx, bool mode)
{
    static const char *st_name[N_ST] = {
	"negotiating",
	"connect",
	"3270-mode",
	"line-mode",
	"remodel",
	"printer",
	"exiting",
	"codepage",
	"selecting",
	"secure",
	"kbd-disable",
	"terminal-name"
    };
    struct st_callback *st;

    vtrace("st_changed(%s,%s)\n", st_name[tx], mode? "true": "false");
    FOREACH_LLIST(&st_callbacks[tx], st, st_callback_t *) {
	(*st->func)(mode);
    } FOREACH_LLIST_END(&st_callbacks[tx], st, st_callback_t *);
}

/* Change connection state. */
void
change_cstate(enum cstate new_cstate, const char *why)
{
    enum cstate old_cstate = cstate;

    if (old_cstate == new_cstate) {
	return;
    }
    vtrace("cstate [%s] -> [%s] (%s)\n", state_name[cstate],
	    state_name[new_cstate], why);

    cstate = new_cstate;

    /* Handle connected/not connected separately. */
    if (cCONNECTED(old_cstate) != cCONNECTED(new_cstate) ||
	cPCONNECTED(old_cstate) != cPCONNECTED(new_cstate)) {
	st_changed(ST_CONNECT, PCONNECTED);
    }

    switch (new_cstate) {
    case RESOLVING:
    case TCP_PENDING:
    case TLS_PENDING:
    case PROXY_PENDING:
    case TELNET_PENDING:
	st_changed(ST_NEGOTIATING, true);
	break;
    case CONNECTED_E_NVT:
    case CONNECTED_NVT:
    case CONNECTED_NVT_CHAR:
	st_changed(ST_LINE_MODE,
		cstate == CONNECTED_NVT || cstate == CONNECTED_E_NVT);
	/* fall through... */
    case CONNECTED_3270:
    case CONNECTED_TN3270E:
    case CONNECTED_UNBOUND:
    case CONNECTED_SSCP:
	if (new_cstate == CONNECTED_UNBOUND) {
	    st_changed(ST_NEGOTIATING, true);
	}
	st_changed(ST_3270_MODE, IN_3270);
	break;
    default:
	break;
    }
}
