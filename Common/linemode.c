/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES,
 * DON RUSSELL, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	linemode.c
 *		TELNET NVT line-mode processing.
 */

#include "globals.h"

#include "appres.h"
#include "linemode.h"
#include "nvt.h"
#include "unicodec.h"
#include "utils.h"
#include "telnet.h"

#define LM_BUFSZ	16384

typedef struct {
    ucs4_t ucs4;
    int mb_len;
    int echo_len;
    bool dbcs;
} width_t;

/* Globals */

/* Statics */
static unsigned char *lbuf = NULL; /* line-mode input buffer */
static unsigned char *lbptr;
static width_t *widths = NULL;
static bool lnext = false;
static bool backslashed = false;
static bool t_valid = false;
static char vintr;
static char vquit;
static char verase;
static char vkill;
static char veof;
static char vwerase;
static char vrprnt;
static char vlnext;

static void do_data(char c);
static void do_intr(char c);
static void do_quit(char c);
static void do_cerase(char c);
static void do_werase(char c);
static void do_kill(char c);
static void do_rprnt(char c);
static void do_eof(char c);
static void do_eol(char c);
static void do_lnext(char c);

/**
 * Expand a character into a displayable string, which means expanding DEL to
 * "^?" and codes 0x00 through 0x1f to "^X" notation.
 *
 * @param[in] c   character to expand
 *
 * @return String representation of c.
 */
static const char *
just_ctl_see(int c)
{
    static char buf[3];
    unsigned char uc = c & 0xff;

    if (uc == 0x7f) {
	return "^?";
    }

    if (uc < ' ') {
	buf[0] = '^';
	buf[1] = uc + '@';
	buf[2] = '\0';
    } else {
	buf[0] = uc;
	buf[1] = '\0';
    }
    return buf;
}

/**
 * Translate the input buffer into UCS4 characters and the number of positions
 * to back up per UCS4 character.
 *
 * @return number of UCS4 characters.
 */
static int
expand_lbuf(void)
{
    size_t len = lbptr - lbuf;
    unsigned char *xbptr = lbuf;
    int nx = 0;

    if (len == 0) {
	return 0;
    }
    if (widths != NULL) {
	Free(widths);
    }
    widths = (width_t *)Malloc(len * sizeof(width_t));

    while (len) {
	int consumed;
	enum me_fail f;
	ucs4_t u;

	/* Handle nulls separately. */
	if (*xbptr == '\0') {
	    widths[nx].ucs4 = 0;
	    widths[nx].mb_len = 1;
	    widths[nx].echo_len = 2; /* ^@ */
	    widths[nx].dbcs = false;
	    nx++;
	    len--;
	    xbptr++;
	    continue;
	}

	u = multibyte_to_unicode((char *)xbptr, len, &consumed, &f);
	if (u == 0) {
	    /* If we get an error, punt. */
	    len--;
	    xbptr++;
	    continue;
	}

	widths[nx].ucs4 = u;
	widths[nx].mb_len = consumed;
	if (u < ' ' || u == 0x7f) {
	    widths[nx].echo_len = 2; /* ^X */
	    widths[nx].dbcs = false;
	} else if (u >= 0x2e80 && u <= 0xd7ff) {
	    widths[nx].echo_len = 1; /* DBCS */
	    widths[nx].dbcs = true;
	} else {
	    widths[nx].echo_len = 1;
	    widths[nx].dbcs = false;
	}
	nx++;
	len -= consumed;
	xbptr += consumed;
    }

    return nx;
}

/*
 * parse_ctlchar
 *	Parse an stty control-character specification.
 *	A cheap, non-complaining implementation.
 */
static char
parse_ctlchar(char *s)
{
    if (!s || !*s) {
	return 0;
    }
    if ((int) strlen(s) > 1) {
	if (*s != '^') {
	    return 0;
	} else if (*(s + 1) == '?') {
	    return 0177;
	} else {
	    return *(s + 1) - '@';
	}
    } else {
	return *s;
    }
}

/**
 * Initialize the control characters for line mode.
 */
void
linemode_init(void)
{
    if (t_valid) {
	return;
    }

    vintr   = parse_ctlchar(appres.linemode.intr);
    vquit   = parse_ctlchar(appres.linemode.quit);
    verase  = parse_ctlchar(appres.linemode.erase);
    vkill   = parse_ctlchar(appres.linemode.kill);
    veof    = parse_ctlchar(appres.linemode.eof);
    vwerase = parse_ctlchar(appres.linemode.werase);
    vrprnt  = parse_ctlchar(appres.linemode.rprnt);
    vlnext  = parse_ctlchar(appres.linemode.lnext);

    t_valid = true;
}

/*
 * linemode_out
 *	Send output in NVT line mode.
 */
void
linemode_out(const char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
	char c = buf[i];

	/* Input conversions. */
	if (!lnext && c == '\r' && appres.linemode.icrnl) {
	    c = '\n';
	} else if (!lnext && c == '\n' && appres.linemode.inlcr) {
	    c = '\r';
	}

	/* Backslashes. */
	if (c == '\\' && !backslashed) {
	    backslashed = true;
	} else {
	    backslashed = false;
	}

	/* Control chars. */
	if (c == '\n') {
	    do_eol(c);
	} else if (c == vintr) {
	    do_intr(c);
	} else if (c == vquit) {
	    do_quit(c);
	} else if (c == verase) {
	    do_cerase(c);
	} else if (c == vkill) {
	    do_kill(c);
	} else if (c == vwerase) {
	    do_werase(c);
	} else if (c == vrprnt) {
	    do_rprnt(c);
	} else if (c == veof) {
	    do_eof(c);
	} else if (c == vlnext) {
	    do_lnext(c);
	} else if (c == 0x08 || c == 0x7f) { /* Yes, a hack. */
	    do_cerase(c);
	} else {
	    do_data(c);
	}
    }
}

void
linemode_buf_init(void)
{
    if (lbuf == NULL) {
	lbuf = (unsigned char *)Malloc(LM_BUFSZ);
    }
    lbptr = lbuf;
    lnext = false;
    backslashed = false;
}

static void
nvt_process_s(const char *data)
{
    while (*data) {
	nvt_process((unsigned int)*data++);
    }
}

static void
nvt_backspace(bool dbcs)
{
    nvt_wrapping_backspace();
    if (dbcs) {
	nvt_wrapping_backspace();
    }
    nvt_process_s(" ");
    if (dbcs) {
	nvt_process_s(" ");
    }
    nvt_wrapping_backspace();
    if (dbcs) {
	nvt_wrapping_backspace();
    }
}

static void
forward_data(void)
{
    net_cookedout((char *)lbuf, lbptr - lbuf);
    linemode_buf_init();
}

static void
do_data(char c)
{
    if (lbptr + 1 < lbuf + LM_BUFSZ) {
	*lbptr++ = c;
	if (c == '\r') {
	    *lbptr++ = '\0';
	}
	nvt_process_s(just_ctl_see((int)c));
    } else {
	nvt_process_s("\007");
    }
    lnext = false;
    backslashed = false;
}

static void
do_intr(char c)
{
    if (lnext) {
	do_data(c);
	return;
    }
    nvt_process_s(ctl_see((int)c));
    linemode_buf_init();
    net_interrupt(c);
}

static void
do_quit(char c)
{
    if (lnext) {
	do_data(c);
	return;
    }
    nvt_process_s(ctl_see((int)c));
    linemode_buf_init();
    net_break(c);
}

/**
 * Erase a character.
 *
 * @param[in] c Input character that triggered the character erase.
 */
static void
do_cerase(char c)
{
    int n_ucs4;
    size_t len;

    if (backslashed) {
	lbptr--;
	nvt_wrapping_backspace();
	do_data(c);
	return;
    }

    if (lnext) {
	do_data(c);
	return;
    }

    if (!(n_ucs4 = expand_lbuf())) {
	return;
    }

    lbptr -= widths[n_ucs4 - 1].mb_len;
    len = widths[n_ucs4 - 1].echo_len;
    while (len--) {
	nvt_backspace(widths[n_ucs4 - 1].dbcs);
    }
}

/**
 * Erase a word.
 *
 * @param[in] c Input character that triggered the word erase.
 */
static void
do_werase(char c)
{
    bool any = false;
    int n_ucs4;
    int ix;

    if (lnext) {
	do_data(c);
	return;
    }

    if (!(n_ucs4 = expand_lbuf())) {
	return;
    }

    for (ix = n_ucs4 - 1; ix >= 0; ix--) {
	ucs4_t ch = widths[ix].ucs4;
	size_t len;

	if (ch == ' ' || ch == '\t') {
	    if (any) {
		break;
	    }
	} else {
	    any = true;
	}

	lbptr -= widths[ix].mb_len;
	len = widths[ix].echo_len;
	while (len--) {
	    nvt_backspace(widths[ix].dbcs);
	}

    }
}

/**
 * Erase the whole input buffer.
 *
 * @param[in] c Input character that triggered the buffer kill.
 */
static void
do_kill(char c)
{
    int n_ucs4;
    int ix;

    if (backslashed) {
	lbptr--;
	nvt_wrapping_backspace();
	do_data(c);
	return;
    }

    if (lnext) {
	do_data(c);
	return;
    }

    if (!(n_ucs4 = expand_lbuf())) {
	return;
    }

    for (ix = n_ucs4 - 1; ix >= 0; ix--) {
	int len = widths[ix].echo_len;

	while (len--) {
	    nvt_backspace(widths[ix].dbcs);
	}
    }

    lbptr = lbuf;
}

/**
 * Reprint the input buffer.
 *
 * @param[in] c Input character that triggered the reprint.
 */
static void
do_rprnt(char c)
{
    unsigned char *p;
    int n_ucs4;
    int ix;

    if (lnext) {
	do_data(c);
	return;
    }

    nvt_process_s(just_ctl_see((int) c));
    nvt_process_s("\r\n");

    if (!(n_ucs4 = expand_lbuf())) {
	return;
    }

    p = lbuf;
    for (ix = 0; ix < n_ucs4; ix++) {
	ucs4_t ch = widths[ix].ucs4;
	if (ch < ' ') {
	    nvt_process((unsigned int)'^');
	    nvt_process((unsigned int)(ch + '@'));
	} else if (ch == 0x7f) {
	    nvt_process_s("^?");
	} else {
	    int i;

	    for (i = 0; i < widths[ix].mb_len; i++) {
	    	nvt_process((unsigned int)(*(p + i)));
	    }
	}
	p += widths[ix].mb_len;
    }
}

static void
do_eof(char c)
{
    if (backslashed) {
	lbptr--;
	nvt_wrapping_backspace();
	do_data(c);
	return;
    }
    if (lnext) {
	do_data(c);
	return;
    }
    do_data(c);
    forward_data();
}

static void
do_eol(char c)
{
    if (lnext) {
	do_data(c);
	return;
    }
    if (lbptr + 2 >= lbuf + LM_BUFSZ) {
	nvt_process_s("\007");
	return;
    }
    *lbptr++ = '\r';
    *lbptr++ = '\n';
    nvt_process_s("\r\n");
    forward_data();
}

static void
do_lnext(char c)
{
    if (lnext) {
	do_data(c);
	return;
    }
    lnext = true;
    nvt_process_s("^");
    nvt_wrapping_backspace();
}

/*
 * linemode_chars
 *	Report line-mode characters.
 */
struct ctl_char *
linemode_chars(void)
{
    static struct ctl_char c[9];

    c[0].name = "intr";		strcpy(c[0].value, ctl_see(vintr));
    c[1].name = "quit";		strcpy(c[1].value, ctl_see(vquit));
    c[2].name = "erase";	strcpy(c[2].value, ctl_see(verase));
    c[3].name = "kill";		strcpy(c[3].value, ctl_see(vkill));
    c[4].name = "eof";		strcpy(c[4].value, ctl_see(veof));
    c[5].name = "werase";	strcpy(c[5].value, ctl_see(vwerase));
    c[6].name = "rprnt";	strcpy(c[6].value, ctl_see(vrprnt));
    c[7].name = "lnext";	strcpy(c[7].value, ctl_see(vlnext));
    c[8].name = NULL;

    return c;
}

/*
 * linemode_send_erase
 *	Send the linemode ERASE character.
 */
void
linemode_send_erase(void)
{
    net_cookout(&verase, 1);
}

/*
 * linemode_send_kill
 *      Send the linemode KILL character.
 */
void
linemode_send_kill(void)
{
    net_cookout(&vkill, 1);
}

/*
 * linemode_send_werase
 *      Send the linemode WERASE character.
 */
void
linemode_send_werase(void)
{
    net_cookout(&vwerase, 1);
}

/*
 * linemode_dump()
 * 	Transition from line mode to character-at-a-time mode.
 *
 * 	Dump whatever is in the line mode buffer to the host.
 * 	This might result in double-echoing, but at least we won't lose any
 * 	input data.
 */
void
linemode_dump(void)
{
    forward_data();
}
