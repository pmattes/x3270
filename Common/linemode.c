/*
 * Copyright (c) 1993-2014, Paul Mattes.
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
#include "utils.h"
#include "telnet.h"

#define LM_BUFSZ	16384

/* Globals */

/* Statics */
static unsigned char *lbuf = NULL; /* line-mode input buffer */
static unsigned char *lbptr;
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
	if (c == '\t') {
	    nvt_process((unsigned int)c);
	} else {
	    nvt_process_s(ctl_see((int)c));
	}
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
    net_interrupt();
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
    net_break();
}

static void
do_cerase(char c)
{
    size_t len;

    if (backslashed) {
	lbptr--;
	nvt_process_s("\b");
	do_data(c);
	return;
    }
    if (lnext) {
	do_data(c);
	return;
    }
    if (lbptr > lbuf) {
	len = strlen(ctl_see((int) *--lbptr));

	while (len--) {
	    nvt_process_s("\b \b");
	}
    }
}

static void
do_werase(char c)
{
    int any = 0;
    size_t len;

    if (lnext) {
	do_data(c);
	return;
    }
    while (lbptr > lbuf) {
	char ch;

	ch = *--lbptr;

	if (ch == ' ' || ch == '\t') {
	    if (any) {
		++lbptr;
		break;
	    }
	} else {
	    any = 1;
	}
	len = strlen(ctl_see((int) ch));

	while (len--) {
	    nvt_process_s("\b \b");
	}
    }
}

static void
do_kill(char c)
{
    size_t i, len;

    if (backslashed) {
	lbptr--;
	nvt_process_s("\b");
	do_data(c);
	return;
    }
    if (lnext) {
	do_data(c);
	return;
    }
    while (lbptr > lbuf) {
	len = strlen(ctl_see((int) *--lbptr));

	for (i = 0; i < len; i++) {
	    nvt_process_s("\b \b");
	}
    }
}

static void
do_rprnt(char c)
{
    unsigned char *p;

    if (lnext) {
	do_data(c);
	return;
    }
    nvt_process_s(ctl_see((int) c));
    nvt_process_s("\r\n");
    for (p = lbuf; p < lbptr; p++) {
	nvt_process_s(ctl_see((int) *p));
    }
}

static void
do_eof(char c)
{
    if (backslashed) {
	lbptr--;
	nvt_process_s("\b");
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
    nvt_process_s("^\b");
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
