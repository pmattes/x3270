/*
 * Copyright (c) 2020-2024 Paul Mattes.
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
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	vstatus.c
 *		Virtual status-line layer.
 */

#include "globals.h"

#include "3270ds.h"
#include "ctlr.h"
#include "ctlrc.h"
#include "kybd.h"
#include "status.h"
#include "telnet.h"
#include "txa.h"
#include "utils.h"
#include "vstatus.h"

#define CM (60*10)	/* csec per minute */

/* Statics. */
static bool voia_compose = false;
static ucs4_t voia_compose_char = 0;
static enum keytype voia_compose_keytype = KT_STD;
static bool voia_undera = true;
static bool voia_im = false;
#define LUCNT   8
static char voia_lu[LUCNT+1];
static const char *voia_msg = "X Not Connected";
static unsigned char voia_msg_color = HOST_COLOR_WHITE;
static bool voia_rm = false;
static char voia_screentrace = 0;
static char voia_script = 0;
static char *voia_scrolled_msg = NULL;
static char voia_timing[32];
static bool voia_ta = false;
static bool voia_boxsolid = false;
static enum {
    SS_INSECURE,
    SS_UNVERIFIED,
    SS_SECURE
} voia_secure = SS_INSECURE;
static bool voia_printer = false;

static void vstatus_connect(bool connected);

void
vstatus_compose(bool on, ucs4_t ucs4, enum keytype keytype)
{
    voia_compose = on;
    voia_compose_char = ucs4;
    voia_compose_keytype = keytype;

    status_compose(on, ucs4, keytype);
}

void
vstatus_ctlr_done(void)
{
    voia_undera = true;

    status_ctlr_done();
}

void
vstatus_insert_mode(bool on)
{
    voia_im = on;

    status_insert_mode(on);
}

void
vstatus_keyboard_disable_flash(void)
{
    status_keyboard_disable_flash();
}

void
vstatus_lu(const char *lu)
{
    if (lu != NULL) {
        strncpy(voia_lu, lu, LUCNT);
        voia_lu[LUCNT] = '\0';
    } else {
        memset(voia_lu, '\0', sizeof(voia_lu));
    }

    status_lu(lu);
}

void
vstatus_minus(void)
{
    voia_msg = "X -f";
    voia_msg_color = HOST_COLOR_RED;
    status_minus();
}

void
vstatus_oerr(int error_type)
{
    switch (error_type) {
    case KL_OERR_PROTECTED:
        voia_msg = "X Protected";
        break;
    case KL_OERR_NUMERIC:
        voia_msg = "X NUM";
        break;
    case KL_OERR_OVERFLOW:
        voia_msg = "X Overflow";
        break;
    }
    voia_msg_color = HOST_COLOR_RED;
    status_oerr(error_type);
}

void
vstatus_reset(void)
{
    vstatus_connect(PCONNECTED);
    status_reset();
}

void
vstatus_reverse_mode(bool on)
{
    voia_rm = on;
    status_reverse_mode(on);
}

void
vstatus_screentrace(int n)
{
    voia_screentrace = (n < 0)? 0: ((n < 9)? "123456789"[n]: '+');
    status_screentrace(n);
}

void
vstatus_script(bool on)
{
    voia_script = on? 's': 0;
    status_script(on);
}

void
vstatus_scrolled(int n)
{
    if (n) {
        Replace(voia_scrolled_msg, Asprintf("X Scrolled %d", n));
    } else {
        Replace(voia_scrolled_msg, NULL);
    }
    status_scrolled(n);
}

void
vstatus_syswait(void)
{
    voia_msg = "X SYSTEM";
    voia_msg_color = HOST_COLOR_WHITE;
    status_syswait();
}

void
vstatus_timing(struct timeval *t0, struct timeval *t1)
{
    static char no_time[] = ":??.?";

    if (t1->tv_sec - t0->tv_sec > (99*60)) {
        strcpy(voia_timing, no_time);
    } else {
        unsigned long cs;       /* centiseconds */

        cs = (t1->tv_sec - t0->tv_sec) * 10 +
             (t1->tv_usec - t0->tv_usec + 50000) / 100000;
        if (cs < CM) {
            snprintf(voia_timing, sizeof(voia_timing),
                    ":%02ld.%ld", cs / 10, cs % 10);
        } else {
            snprintf(voia_timing, sizeof(voia_timing),
                    "%02ld:%02ld", cs / CM, (cs % CM) / 10);
        }
    }
    status_timing(t0, t1);
}

void
vstatus_twait(void)
{
    voia_undera = false;
    voia_msg = "X Wait";
    voia_msg_color = HOST_COLOR_WHITE;
    status_twait();
}

void
vstatus_typeahead(bool on)
{
    voia_ta = on;

    status_typeahead(on);
}

static void
vstatus_untiming_internal(void)
{
    voia_timing[0] = '\0';
}

void
vstatus_untiming(void)
{
    vstatus_untiming_internal();

    status_untiming();
}

static void
vstatus_connect(bool connected)
{
    if (connected) {
        voia_boxsolid = IN_3270 && !IN_SSCP;
        if (cstate == RECONNECTING) {
            voia_msg = "X Reconnecting";
        } else if (cstate == RESOLVING) {
            voia_msg = "X [DNS]";
        } else if (cstate == TCP_PENDING) {
            voia_msg = "X [TCP]";
            voia_boxsolid = false;
            voia_secure = SS_INSECURE;
        } else if (cstate == TLS_PENDING) {
            voia_msg = "X [TLS]";
            voia_boxsolid = false;
            voia_secure = SS_INSECURE;
        } else if (cstate == PROXY_PENDING) {
            voia_msg = "X [Proxy]";
            voia_boxsolid = false;
            voia_secure = SS_INSECURE;
        } else if (cstate == TELNET_PENDING) {
            voia_msg = "X [TELNET]";
            voia_boxsolid = false;
            voia_secure = SS_INSECURE;
        } else if (cstate == CONNECTED_UNBOUND) {
            voia_msg = "X [TN3270E]";
        } else if (kybdlock & KL_AWAITING_FIRST) {
            voia_msg = "X [Field]";
	} else if (kybdlock & KL_ENTER_INHIBIT) {
	    voia_msg = "X Inhibit";
	} else if (kybdlock & KL_BID) {
	    voia_msg = "X Wait";
	} else if (kybdlock & KL_FT) {
	    voia_msg = "X File Transfer";
	} else if (kybdlock & KL_DEFERRED_UNLOCK) {
	    voia_msg = "X";
	} else {
            voia_msg = NULL;
        }
        if (net_secure_connection()) {
            if (net_secure_unverified()) {
                voia_secure = SS_UNVERIFIED;
            } else {
                voia_secure = SS_SECURE;
            }
        } else {
            voia_secure = SS_INSECURE;
        }
    } else {
        voia_boxsolid = false;
        voia_msg = "X Not Connected";
        voia_secure = SS_INSECURE;
    }
    voia_msg_color = HOST_COLOR_WHITE;
    vstatus_untiming_internal();
}

static void
vstatus_3270_mode(bool on)
{
    voia_boxsolid = IN_3270 && !IN_SSCP;
    if (voia_boxsolid) {
        voia_undera = true;
    }
    vstatus_connect(PCONNECTED);
}

static void
vstatus_printer(bool on)
{
    voia_printer = on;
}

/**
 * Returns the virtual status line, which is effectively the c3270 status
 * line.
 *
 * @param[out] ea	Returned text and attributes
 */
void
vstatus_line(struct ea *ea)
{
    int i;
    int rmargin = COLS - 1;
    char *cursor;
    struct ea *ea2;

    /* Begin with nothing. */
    memset(ea, 0, 2 * COLS * sizeof(struct ea));
    for (i = 0; i < 2 * COLS; i++) {
	ea[i].fg = mode3279? HOST_COLOR_BLUE: HOST_COLOR_GREEN;
    }

    /* Ignore any previous field attributes. */
    ea[0].gr = GR_RESET;

    /* Create the dividing line. */
    for (i = 0; i < COLS; i++) {
	ea[i].ucs4 = ' ';
	ea[i].gr = GR_UNDERLINE;
    }

/* The OIA looks like (in Model 2/3/4 mode):

          1         2         3         4         5         6         7
01234567890123456789012345678901234567890123456789012345678901234567890123456789
4AN    Status-Message--------------------- Cn TRIPS+s LU-Name-   :ss.s  000/000
         7         6         5         4         3         2         1
98765432109876543210987654321098765432109876543210987654321098765432109876543210
                                                                        ^ -7
                                                                 ^ -14
                                                      ^-25

   On wider displays, there is a bigger gap between TRIPS+s and LU-Name.

*/

    ea2 = ea + COLS;
    ea2[0].gr = GR_REVERSE;
    ea2[0].ucs4 = '4';
    ea2[1].gr = GR_UNDERLINE;
    if (voia_undera) {
	ea2[1].ucs4 = IN_E? 'B': 'A';
    }
    ea2[2].gr = GR_REVERSE;
    ea2[2].ucs4 = IN_NVT? 'N': (voia_boxsolid? 0 :(IN_SSCP? 'S': '?'));

    /* Display the status message. */
    if (voia_msg != NULL) {
	const char *msg = voia_msg;

	for (i = 0; i < 35 && *msg; i++) {
	    ea2[7 + i].ucs4 = *msg++;
	    ea2[7 + i].fg = mode3279? voia_msg_color: HOST_COLOR_GREEN;
	    ea2[7 + i].gr = GR_INTENSIFY;
	}
    }

    /* Display the miscellaneous state. */
    i = rmargin - 35;
    if (voia_compose) {
	ea2[i++].ucs4 = voia_compose? 'C': 0;
	ea2[i++].ucs4 = voia_compose? voia_compose_char: 0;
    }
    i++;
    ea2[i++].ucs4 = voia_ta? 'T': 0;
    ea2[i++].ucs4 = voia_rm? 'R': 0;
    ea2[i++].ucs4 = voia_im? 'I': 0;
    ea2[i++].ucs4 = voia_printer? 'P': 0;
    if (voia_secure != SS_INSECURE) {
	ea2[i].fg = (voia_secure == SS_SECURE)?
	    HOST_COLOR_GREEN: HOST_COLOR_YELLOW;
	ea2[i++].ucs4 = 'S';
    } else {
	i++;
    }
    ea2[i++].ucs4 = voia_screentrace;
    ea2[i++].ucs4 = voia_script;

    /* Logical unit name. */
    i = rmargin - 25;
    if (voia_lu[0]) {
	char *lu = voia_lu;

	while (*lu) {
	    ea2[i++].ucs4 = *lu++;
	}
    }

    /* Timing. */
    i = rmargin - 14;
    if (voia_timing[0]) {
	char *timing = voia_timing;

	while (*timing) {
	    ea2[i++].ucs4 = *timing++;
	}
    }

    /* Cursor. */
    cursor = txAsprintf("%03d/%03d ", ((cursor_addr / COLS) + 1) % 1000,
	    ((cursor_addr % COLS) + 1) % 1000);
    for (i = 0; cursor[i]; i++) {
	ea2[rmargin - 7 + i].ucs4 = cursor[i];
    }
}

/**
 * Virtual status line module registration.
 */
void
vstatus_register(void)
{
    /* Register for state changes. */
    register_schange(ST_NEGOTIATING, vstatus_connect);
    register_schange(ST_CONNECT, vstatus_connect);
    register_schange(ST_3270_MODE, vstatus_3270_mode);
    register_schange(ST_PRINTER, vstatus_printer);
}
