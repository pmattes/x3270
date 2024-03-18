/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ctlr.c
 *		This module handles interpretation of the 3270 data stream and
 *		maintenance of the 3270 device state.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#if !defined(_MSC_VER) /*[*/
#include <unistd.h>
#endif /*]*/
#include <stdlib.h>
#include <sys/types.h>
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#endif /*]*/
#include <signal.h>
#include "globals.h"
#include "pr3287.h"
#include "3270ds.h"
#include "codepage.h"
#include "ctlrc.h"
#include "trace.h"
#include "sf.h"
#include "tables.h"
#include "unicodec.h"
#include "xtablec.h"
#if defined(_WIN32) /*[*/
#include "wsc.h"
#include <windows.h>
#endif /*]*/

#define CS_GE 0x04	/* hack */

#define WCC_LINE_LENGTH(c)		((c) & 0x30)
#define WCC_132				0x00
#define WCC_40				0x10
#define WCC_64				0x20
#define WCC_80				0x30

#define MAX_BUF				(MAX_UNF_MPP * MAX_UNF_MPP) /* swag */

#define VISIBLE		0x01	/* visible field */
#define INVISIBLE	0x02	/* invisible field */

#define BUFSZ		4096

#define FCORDER_NOP	0x0001	/* dummy filler for DBCS right half */

static const char *ll_name[] = { "unformatted132", "formatted40", "formatted64", "formatted80" };
static int ll_len[] = { 132, 40, 64, 80 };

/* 3270 (formatted mode) data */
static unsigned char default_gr;
static unsigned char default_cs;
static int line_length;
static ucs4_t page_buf[MAX_BUF];
static unsigned char *xlate_buf[MAX_BUF];
int xlate_len[MAX_BUF];
static int baddr = 0;
static bool page_buf_initted = false;
static bool any_3270_printable = false;
static int any_3270_output = 0;
#if !defined(_WIN32) /*[*/
static FILE *prfile = NULL;
static int prpid = -1;
#else /*][*/
static int ws_initted = 0;
static int ws_needpre = 1;
#endif /*]*/
static unsigned char wcc_line_length;

static int ctlr_erase(void);
static int dump_formatted(void);
static int dump_unformatted(void);
static int stash(unsigned char c);
static int prflush(void);
static int copyfile(const char *filename);

#define DECODE_BADDR(c1, c2) \
    ((((c1) & 0xC0) == 0x00) ? \
    (((c1) & 0x3F) << 8) | (c2) : \
    (((c1) & 0x3F) << 6) | ((c2) & 0x3F))

/* SCS constants and data. */
#define MAX_MPP	132
#define MAX_MPL	108

static ucs4_t linebuf[MAX_MPP+1];
static struct {
    unsigned malloc_len;
    unsigned data_len;
    char *buf;
} trnbuf[MAX_MPP+1];
static char htabs[MAX_MPP+1];
static char vtabs[MAX_MPL+1];
static int lm, tm, bm, mpp, mpl, scs_any;
static int pp;
static int line;
static bool scs_initted = false;
static bool any_scs_output = false;
static size_t scs_leftover_len = 0;
static int scs_leftover_buf[256];
static int scs_dbcs_subfield = 0;
static unsigned char scs_dbcs_c1 = 0;
static unsigned scs_cs = 0;
static bool ffeoj_last = false;

/*
* Interpret an incoming 3270 command.
*/
enum pds
process_ds(unsigned char *buf, size_t buflen)
{
    if (!buflen) {
	return PDS_OKAY_NO_OUTPUT;
    }

    trace_ds("< ");

    switch (buf[0]) {	/* 3270 command */
    case CMD_EAU:	/* erase all unprotected */
    case SNA_CMD_EAU:
	trace_ds("EraseAllUnprotected\n");
	if (ctlr_erase() < 0 || prflush() < 0) {
	    return PDS_FAILED;
	}
	return PDS_OKAY_NO_OUTPUT;
    case CMD_EWA:	/* erase/write alternate */
    case SNA_CMD_EWA:
	trace_ds("EraseWriteAlternate");
	if (ctlr_erase() < 0 || prflush() < 0) {
	    return PDS_FAILED;
	}
	baddr = 0;
	ctlr_write(buf, buflen, true);
	return PDS_OKAY_NO_OUTPUT;
    case CMD_EW:	/* erase/write */
    case SNA_CMD_EW:
	trace_ds("EraseWrite");
	if (ctlr_erase() < 0 || prflush() < 0) {
	    return PDS_FAILED;
	}
	baddr = 0;
	ctlr_write(buf, buflen, true);
	return PDS_OKAY_NO_OUTPUT;
    case CMD_W:	/* write */
    case SNA_CMD_W:
	trace_ds("Write");
	ctlr_write(buf, buflen, false);
	return PDS_OKAY_NO_OUTPUT;
    case CMD_RB:	/* read buffer */
    case SNA_CMD_RB:
	trace_ds("ReadBuffer\n");
	return PDS_BAD_CMD;
    case CMD_RM:	/* read modifed */
    case SNA_CMD_RM:
	trace_ds("ReadModified\n");
	return PDS_BAD_CMD;
    case CMD_RMA:	/* read modifed all */
    case SNA_CMD_RMA:
	trace_ds("ReadModifiedAll\n");
	return PDS_BAD_CMD;
    case CMD_WSF:	/* write structured field */
    case SNA_CMD_WSF:
	trace_ds("WriteStructuredField");
	return write_structured_field(buf, buflen);
    case CMD_NOP:	/* no-op */
	trace_ds("NoOp\n");
	return PDS_OKAY_NO_OUTPUT;
    default:
	/* unknown 3270 command */
	errmsg("Unknown 3270 Data Stream command: 0x%X", buf[0]);
	return PDS_BAD_CMD;
    }
}

/*
 * Process a 3270 Write command.
 */
void
ctlr_write(unsigned char buf[], size_t buflen, bool erase)
{
    unsigned char *cp;
    bool wcc_keyboard_restore, wcc_sound_alarm;
    bool wcc_start_printer;
    bool ra_ge;
    int i;
    unsigned char na;
    int any_fa;
    ucs4_t ra_xlate = 0;
    const char *paren = "(";
    int xbaddr;
    enum { NONE, ORDER, SBA, TEXT, NULLCH } previous = NONE;

#define END_TEXT0	{ if (previous == TEXT) trace_ds("'"); }
#define END_TEXT(cmd)	{ END_TEXT0; trace_ds(" %s", cmd); }

#define START_FIELD(fa) { \
	    ctlr_add(0, FA_IS_ZERO(fa)?INVISIBLE:VISIBLE, 0, default_gr); \
	    trace_ds(see_attr(fa)); \
	}

    if (buflen < 2) {
	return;
    }

    if (!page_buf_initted) {
	memset(page_buf, '\0', MAX_BUF * sizeof(ucs4_t));
	memset(xlate_buf, '\0', MAX_BUF * sizeof(unsigned char *));
	memset(xlate_len, '\0', MAX_BUF * sizeof(int));
	page_buf_initted = true;
	baddr = 0;
    }

    default_gr = 0;
    default_cs = 0;

    if (WCC_RESET(buf[1])) {
	trace_ds("%sreset", paren);
	paren = ",";
    }
    wcc_line_length = WCC_LINE_LENGTH(buf[1]);
    if (wcc_line_length) {
	trace_ds("%s%s", paren, ll_name[wcc_line_length >> 4]);
	paren = ",";
    } else {
	trace_ds("%sunformatted", paren);
	paren = ",";
    }
    line_length = ll_len[wcc_line_length >> 4];
    wcc_sound_alarm = WCC_SOUND_ALARM(buf[1]);
    if (wcc_sound_alarm) {
	trace_ds("%salarm", paren);
	paren = ",";
    }
    wcc_keyboard_restore = WCC_KEYBOARD_RESTORE(buf[1]);
    if (wcc_keyboard_restore) {
	trace_ds("%srestore", paren);
	paren = ",";
    }

    if (WCC_RESET_MDT(buf[1])) {
	trace_ds("%sresetMDT", paren);
	paren = ",";
    }
    wcc_start_printer = WCC_START_PRINTER(buf[1]);
    if (wcc_start_printer) {
	trace_ds("%sstartprinter", paren);
	paren = ",";
    }
    if (strcmp(paren, "(")) {
	trace_ds(")");
    }

    for (cp = &buf[2]; cp < (buf + buflen); cp++) {
	switch (*cp) {
	case ORDER_SF:	/* start field */
	    END_TEXT("StartField");
	    previous = ORDER;
	    cp++;		/* skip field attribute */
	    START_FIELD(*cp);
	    break;
	case ORDER_SBA:	/* set buffer address */
	    cp += 2;	/* skip buffer address */
	    xbaddr = DECODE_BADDR(*(cp - 1), *cp);
	    END_TEXT("SetBufferAddress");
	    if (wcc_line_length) {
		trace_ds("(%d,%d)", 1 + (xbaddr / line_length),
			1 + (xbaddr % line_length));
	    } else {
		    trace_ds("(%d[%+d])", xbaddr, xbaddr - baddr);
	    }
	    if (xbaddr >= MAX_BUF) {
		/* Error! */
		baddr = 0;
		return;
	    }
	    if (wcc_line_length) {
		/* Formatted. */
		baddr = xbaddr;
	    } else if (xbaddr > baddr) {
		/* Unformatted. */
		while (baddr < xbaddr) {
		    ctlr_add(0, ' ', default_cs, default_gr);
		}
	    }
	    previous = SBA;
	    break;
	case ORDER_IC:	/* insert cursor */
	    END_TEXT("InsertCursor");
	    previous = ORDER;
	    break;
	case ORDER_PT:	/* program tab */
	    END_TEXT("ProgramTab");
	    previous = ORDER;
	    break;
	case ORDER_RA:	/* repeat to address */
	    cp += 2;	/* skip buffer address */
	    xbaddr = DECODE_BADDR(*(cp-1), *cp);
	    END_TEXT("RepeatToAddress");
	    if (wcc_line_length) {
		trace_ds("(%d,%d)", 1 + (xbaddr / line_length),
			1 + (xbaddr % line_length));
	    } else {
		trace_ds("(%d[%+d])", xbaddr, xbaddr - baddr);
	    }
	    cp++;		/* skip char to repeat */
	    if (*cp == ORDER_GE){
		ra_ge = true;
		trace_ds("GraphicEscape");
		cp++;
	    } else {
		ra_ge = false;
	    }
	    trace_ds("'%s'", see_ebc(*cp));
	    previous = ORDER;
	    if (xbaddr > MAX_BUF || xbaddr < baddr) {
		baddr = 0;
		return;
	    }
	    /* Translate '*cp' once. */
	    switch (*cp) {
	    case FCORDER_FF:
	    case FCORDER_CR:
	    case FCORDER_NL:
	    case FCORDER_EM:
		ra_xlate = *cp;
		break;
	    default:
		if (*cp <= 0x3F) {
		    ra_xlate = '\0';
		} else {
		    ra_xlate = ebcdic_to_unicode(*cp, ra_ge? CS_GE: CS_BASE,
			    EUO_NONE);
		}
		break;
	    }
	    while (baddr < xbaddr) {
		ctlr_add(ra_ge? 0: *cp, ra_xlate, ra_ge? CS_GE: default_cs,
			default_gr);
	    }
	    break;
	case ORDER_EUA:	/* erase unprotected to address */
	    cp += 2;	/* skip buffer address */
	    xbaddr = DECODE_BADDR(*(cp-1), *cp);
	    END_TEXT("EraseUnprotectedAll");
	    previous = ORDER;
	    break;
	case ORDER_GE:	/* graphic escape */
	    END_TEXT("GraphicEscape ");
	    cp++;		/* skip char */
	    previous = ORDER;
	    if (*cp) {
		trace_ds("'");
	    }
	    trace_ds("%s", see_ebc(*cp));
	    if (*cp) {
		trace_ds("'");
	    }
	    ctlr_add(0, ebcdic_to_unicode(*cp, CS_GE, EUO_NONE), CS_GE,
		    default_gr);
	    break;
	case ORDER_MF:	/* modify field */
	    END_TEXT("ModifyField");
	    previous = ORDER;
	    cp++;
	    na = *cp;
	    cp += na * 2;
	    break;
	case ORDER_SFE:	/* start field extended */
	    END_TEXT("StartFieldExtended");
	    previous = ORDER;
	    cp++;	/* skip order */
	    na = *cp;
	    any_fa = 0;

	    for (i = 0; i < (int)na; i++) {
		cp++;
		if (*cp == XA_3270) {
		    trace_ds(" 3270");
		    cp++;
		    START_FIELD(*cp);
		    any_fa++;
		} else if (*cp == XA_FOREGROUND) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		} else if (*cp == XA_HIGHLIGHTING) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		} else if (*cp == XA_CHARSET) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		} else if (*cp == XA_ALL) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		} else {
		    trace_ds("%s[unsupported]", see_efa(*cp, *(cp + 1)));
		    cp++;
		}
	    }
	    if (!any_fa) {
		START_FIELD(0);
	    }
	    ctlr_add(0, '\0', 0, default_gr);
	    break;
	case ORDER_SA:	/* set attribute */
	    END_TEXT("SetAttribtue");
	    previous = ORDER;
	    cp++;
	    if (*cp == XA_FOREGROUND)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
	    } else if (*cp == XA_HIGHLIGHTING)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		default_gr = *(cp + 1) & 0x07;
	    } else if (*cp == XA_ALL)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		default_gr = 0;
		default_cs = 0;
	    } else if (*cp == XA_CHARSET) {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		default_cs = (*(cp + 1) == 0xf1) ? 1 : 0;
	    } else {
		trace_ds("%s[unsupported]", see_efa(*cp, *(cp + 1)));
	    }
	    cp++;
	    break;
	case FCORDER_FF:	/* Form Feed */
	    END_TEXT("FF");
	    previous = ORDER;
	    ctlr_add(0, FCORDER_FF, default_cs, default_gr);
	    break;
	case FCORDER_CR:	/* Carriage Return */
	    END_TEXT("CR");
	    previous = ORDER;
	    ctlr_add(0, FCORDER_CR, default_cs, default_gr);
	    break;
	case FCORDER_NL:	/* New Line */
	    END_TEXT("NL");
	    previous = ORDER;
	    ctlr_add(0, FCORDER_NL, default_cs, default_gr);
	    break;
	case FCORDER_EM:	/* End of Media */
	    END_TEXT("EM");
	    previous = ORDER;
	    ctlr_add(0, FCORDER_EM, default_cs, default_gr);
	    break;
	case FCORDER_DUP:	/* Visible control characters */
	case FCORDER_FM:
	    END_TEXT(see_ebc(*cp));
	    previous = ORDER;
	    ctlr_add(0, ebc2asc0[*cp], default_cs, default_gr);
	    break;
	case FCORDER_SUB:	/* misc format control orders */
	case FCORDER_EO:
	    END_TEXT(see_ebc(*cp));
	    previous = ORDER;
	    ctlr_add(0, '\0', default_cs, default_gr);
	    break;
	case FCORDER_NULL:
	    END_TEXT("NULL");
	    previous = NULLCH;
	    ctlr_add(0, '\0', default_cs, default_gr);
	    break;
	default:	/* enter character */
	    if (*cp <= 0x3F) {
		END_TEXT("ILLEGAL-ORDER ");
		previous = ORDER;
		ctlr_add(0, '\0', default_cs, default_gr);
		trace_ds("%s", see_ebc(*cp));
		break;
	    }
	    if (previous != TEXT) {
		trace_ds(" '");
	    }
	    previous = TEXT;
	    trace_ds("%s", see_ebc(*cp));
	    ctlr_add(*cp, ebcdic_to_unicode(*cp, default_cs, EUO_NONE),
		    default_cs, default_gr);
	    break;
	}
    }

    trace_ds("\n");
}

#undef START_FIELDx
#undef START_FIELD0
#undef START_FIELD
#undef END_TEXT0
#undef END_TEXT

/*
 * Process SCS (SNA Character Stream) data.
 */

/* Reinitialize the SCS virtual 3287. */
static void
init_scs_horiz(void)
{
    int i;

    mpp = MAX_MPP;
    lm = 1;
    htabs[1] = 1;
    for (i = 2; i <= MAX_MPP; i++) {
	htabs[i] = 0;
    }
}

static void
init_scs_vert(void)
{
    int i;

    mpl = 1;
    tm = 1;
    bm = mpl;
    vtabs[1] = 1;
    for (i = 0; i <= MAX_MPL; i++) {
	vtabs[i] = 0;
    }
}

static void
init_scs(void)
{
    int i;

    if (scs_initted) {
	return;
    }

    trace_ds("Initializing SCS virtual 3287.\n");
    init_scs_horiz();
    init_scs_vert();
    pp = 1;
    line = 1;
    scs_any = 0;
    for (i = 0; i < MAX_MPP+1; i++) {
	linebuf[i] = ' ';
    }
    for (i = 0; i < MAX_MPP+1; i++) {
	if (trnbuf[i].malloc_len != 0) {
	    Free(trnbuf[i].buf);
	    trnbuf[i].buf = NULL;
	    trnbuf[i].malloc_len = 0;
	}
	trnbuf[i].data_len = 0;
    }
    scs_leftover_len = 0;
    scs_dbcs_subfield = 0;
    scs_dbcs_c1 = 0;
    scs_cs = 0;

    scs_initted = true;
}

#if defined(_WIN32) /*[*/
static int
unicode_to_printer(ucs4_t u, char mb[], int mb_len)
{
    int nc;
    wchar_t wuc = u;

    nc = WideCharToMultiByte(options.printercp, 0, &wuc, 1, mb, mb_len, NULL,
	    NULL);
    if (nc > 0) {
	mb[nc++] = '\0';
    }
    return nc;
}
#endif /*[*/

/*
 * Our philosophy for automatic newlines and formfeeds is that we generate them
 * only if the user attempts to put data outside the MPP/MPL-defined area.
 * Therefore, the user can put a byte on the last column of each line, and on
 * the last column of the last line of the page, and not need to worry about 
 * suppressing their own NL or FF.
 */

/*
 * Dump and reset the current line.
 * This will always result in at least one byte of output to the printer (a
 * newline).  The 'line' variable is always incremented, and may end up
 * pointing past the bottom margin.  The 'pp' variable is set to the left
 * margin.
 *
 * We do not observe the -skipcc option with SCS data.
 */
static int
dump_scs_line(bool reset_pp, bool always_nl)
{
    int i;
    bool any_data = false;

    /* Find the last non-space character in the line buffer. */
    for (i = mpp; i >= 1; i--) {
	if (trnbuf[i].data_len != 0 || linebuf[i] != ' ') {
	    break;
	}
    }

    /*
     * If there is data there, print it with a trailing newline and
     * clear out the line buffer for next time.  If not, just print the
     * newline.
     */
    if (i >= 1) {
	int j;
#if defined(DEBUG_FF) /*[*/
	int n_data = 0;
	int n_trn = 0;
#endif /*]*/
	int k;

	for (j = 1; j <= i; j++) {
	    /*
	     * Dump and transparent data that precedes this
	     * character.
	     */
	    if (trnbuf[j].data_len) {
		unsigned k;

#if defined(DEBUG_FF) /*[*/
		n_trn += trnbuf[j].data_len;
#endif /*]*/
		for (k = 0; k < trnbuf[j].data_len; k++) {
		    if (stash(trnbuf[j].buf[k]) < 0) {
			return -1;
		    }
		}
		trnbuf[j].data_len = 0;
	    }
	    if (j < i || linebuf[j] != ' ') {
		char mb[16];
		int len;

		if (linebuf[j] == FCORDER_NOP) {
		    continue;
		}
#if defined(DEBUG_FF) /*[*/
		n_data++;
#endif /*]*/
		any_data = true;
		scs_any = true;
#if !defined(_WIN32) /*[*/
		len = unicode_to_multibyte(linebuf[j], mb, sizeof(mb));
#else /*][*/
		len = unicode_to_printer(linebuf[j], mb, sizeof(mb));
#endif /*]*/
		if (len == 0) {
		    mb[0] = ' ';
		    len = 1;
		} else {
		    len--;
		}
		for (k = 0; k < len; k++) {
		    if (stash(mb[k]) < 0) {
			return -1;
		    }	
		}
	    }
	}
#if defined(DEBUG_FF) /*[*/
	trace_ds(" [dumping %d+%dt]", n_data, n_trn);
#endif /*]*/
	for (k = 0; k < MAX_MPP+1; k++) {
	    linebuf[k] = ' ';
	}
    }
    if (any_data || always_nl) {
	if (options.crlf) {
	    if (stash('\r') < 0) {
		return -1;
	    }
	}
	if (stash('\n') < 0)
	return -1;
	line++;
    }
#if defined(DEBUG_FF) /*[*/
    trace_ds(" [line=%d]", line);
#endif /*]*/
    if (reset_pp) {
	pp = lm;
    }
    any_scs_output = false;
    return 0;
}

/* SCS formfeed. */
static int
scs_formfeed(bool explicit)
{
#if defined(DEBUG_FF) /*[*/
    int nls = 0;
#endif /*]*/

    /*
     * In ffskip mode, if it's an explicit formfeed, and we haven't
     * printed any non-transparent data, do nothing.
     */
    if (options.ffskip && explicit && !scs_any) {
	return 0;
    }

    /*
     * In ffthru mode, pass through a \f, but only if it's explicit.
     */
    if (options.ffthru) {
	if (explicit) {
	    if (stash('\f') < 0) {
		return -1;
	    }
	    scs_any = 0;
	}
	line = 1;
	return 0;
    }

    if (explicit) {
	scs_any = 0;
    }

    if (mpl > 1) {
	/* Skip to the end of the physical page. */
	while (line <= mpl) {
	    if (options.crlf) {
		if (stash('\r') < 0) {
		    return -1;
		}
	    }
	    if (stash('\n') < 0) {
		return -1;
	    }
#if defined(DEBUG_FF) /*[*/
	    nls++;
#endif /*]*/
	    line++;
	}
	line = 1;

	/* Skip the top margin. */
	while (line < tm) {
	    if (options.crlf) {
		if (stash('\r') < 0) {
		    return -1;
		}
	    }
	    if (stash('\n') < 0) {
		return -1;
	    }
#if defined(DEBUG_FF) /*[*/
	    nls++;
#endif /*]*/
	    line++;
	}
#if defined(DEBUG_FF) /*[*/
	if (nls) {
	    trace_ds(" [formfeed %s %d]", explicit?  "explicit": "implicit",
		    nls);
	}
#endif /*]*/
    } else {
	line = 1;
    }
    return 0;
}

/*
 * Add a printable character to the SCS virtual 3287.
 * If the line position is past the bottom margin, we will skip to the top of
 * the next page.  If the character position is past the MPP, we will skip to
 * the left margin of the next line.
 */
static int
add_scs(ucs4_t c)
{
    /*
     * They're about to print something.
     * If the line is past the bottom margin, we need to skip to the
     * MPL, and then past the top margin.
     */
    if (line > bm) {
	if (scs_formfeed(false) < 0) {
	    return -1;
	}
    }

    /*
     * If this character would overflow the line, then dump the current
     * line and start over at the left margin.
     */
    if (pp > mpp) {
	if (dump_scs_line(true, true) < 0) {
	    return -1;
	}
    }

    /*
     * Store this character in the line buffer and advance the print
     * position.
     */
    if (c != ' ') {
	linebuf[pp++] = c;
    } else {
	pp++;
    }
    any_scs_output = true;
    ffeoj_last = false;
    return 0;
}

/*
 * Add a string of transparent data to the SCS virtual 3287.
 * Transparent data lives between the 'counted' 3287 characters.  Really.
 */
static void
add_scs_trn(unsigned char *cp, int cnt)
{
    int i;
    unsigned new_malloc_len;

    for (i = 0; i < cnt; i++) {
	trace_ds(" %02x", cp[i]);
    }

    new_malloc_len = trnbuf[pp].data_len + cnt;
    while (trnbuf[pp].malloc_len < new_malloc_len) {
	trnbuf[pp].malloc_len += BUFSZ;
	trnbuf[pp].buf = Realloc(trnbuf[pp].buf, trnbuf[pp].malloc_len);
    }
    memcpy(trnbuf[pp].buf + trnbuf[pp].data_len, cp, cnt);
    trnbuf[pp].data_len += cnt;
    any_scs_output = true;
    ffeoj_last = true;
}

/*
 * Process a bufferful of SCS data.
 *
 * Note that unlike a 3270 Write command, even though the record is bounded
 * by an EOR, the SCS data are not guaranteed to be complete.
 * 
 * Rather than have a full FSM for every byte of every SCS order, we resort
 * to the rather inefficient method of concatenating the previous, incomplete
 * record with a copy of the new record, processing it as a contiguous
 * buffer, and saving any incomplete order for next time.
 */

/*
 * 'Internal' SCS function, called by process_scs() below with the previous
 * leftover data plus the current buffer.
 *
 * If an incomplete order is detected, saves it in scs_leftover_buf for
 * next time.
 */
static enum pds
process_scs_contig(unsigned char *buf, size_t buflen)
{
    unsigned char *cp;
    int i;
    int cnt;
    int tab;
    ucs4_t uc;
    enum { NONE, DATA, ORDER } last = NONE;
#   define END_TEXT(s) { \
	    if (last == DATA) { \
		    trace_ds("'"); \
	    } \
	    trace_ds(" " s); \
	    last = ORDER; \
    }
#   define LEFTOVER { \
	    trace_ds(" [pending]"); \
	    scs_leftover_len = buflen - (cp - buf); \
	    memcpy(scs_leftover_buf, cp, scs_leftover_len); \
	    cp = buf + buflen; \
    }

    trace_ds("< ");

    init_scs();

    for (cp = &buf[0]; cp < (buf + buflen); cp++) {
	switch (*cp) {
	case SCS_BS:	/* back space */
	    END_TEXT("BS");
	    if (pp != 1) {
		pp--;
	    }
	    if (scs_dbcs_subfield && pp != 1) {
		pp--;
	    }
	    break;
	case SCS_CR:	/* carriage return */
	    END_TEXT("CR");
	    pp = lm;
	    break;
	case SCS_ENP:	/* enable presentation */
	    END_TEXT("ENP");
	    /* No-op. */
	    break;
	case SCS_FF:	/* form feed */
	    END_TEXT("FF");
	    /* Dump any pending data, and go to the next line. */
	    if (dump_scs_line(true, false) < 0) {
		return PDS_FAILED;
	    }
	    /*
	     * If there is a max page length, skip to the next
	     * page.
	     */
	    if (scs_formfeed(true) < 0) {
		return PDS_FAILED;
	    }
	    break;
	case SCS_HT:	/* horizontal tab */
	    END_TEXT("HT");
	    for (i = pp + 1; i <= mpp; i++) {
		if (htabs[i]) {
		    break;
		}
	    }
	    if (i <= mpp) {
		pp = i;
	    } else {
		if (add_scs(' ') < 0) {
		    return PDS_FAILED;
		}
	    }
	    break;
	case SCS_INP:	/* inhibit presentation */
	    END_TEXT("INP");
	    /* No-op. */
	    break;
	case SCS_IRS:	/* inter-record separator */
	    END_TEXT("IRS");
	case SCS_NL:	/* new line */
	    if (*cp == SCS_NL) {
		END_TEXT("NL");
	    }
	    if (dump_scs_line(true, true) < 0) {
		return PDS_FAILED;
	    }
	    break;
	case SCS_VT:	/* vertical tab */
	    END_TEXT("VT");
	    for (i = line + 1; i <= MAX_MPL; i++){
		if (vtabs[i]) {
		    break;
		}
	    }
	    if (i <= MAX_MPL) {
		if (dump_scs_line(false, true) < 0) {
		    return PDS_FAILED;
		}
		while (line < i) {
		    if (options.crlf) {
			if (stash('\r') < 0) {
			    return PDS_FAILED;
			}
		    }
		    if (stash('\n') < 0) {
			return PDS_FAILED;
		    }
		    line++;
		}
		break;
	    } else {
		/* fall through... */
	    }
	case SCS_VCS:	/* vertical channel select */
	    if (*cp == SCS_VCS) {
		END_TEXT("VCS");
	    }
	    /* fall through... */
	case SCS_LF:	/* line feed */
	    if (*cp == SCS_LF)
		    END_TEXT("LF");
	    if (dump_scs_line(false, true) < 0)
		    return PDS_FAILED;
	    break;
	case SCS_GE:	/* graphic escape */
	    END_TEXT("GE");
	    if ((cp + 1) >= buf + buflen) {
		LEFTOVER;
		break;
	    }
	    /* Skip over the order. */
	    cp++;
	    /* No support, so all characters are spaces. */
	    trace_ds(" %02x", *cp);
	    if (add_scs(' ') < 0) {
		return PDS_FAILED;
	    }
	    break;
	case SCS_SA:	/* set attribute */
	    END_TEXT("SA");
	    if ((cp + 2) >= buf + buflen) {
		LEFTOVER;
		break;
	    }
	    switch (*(cp + 1)) {
	    case SCS_SA_RESET:
		trace_ds(" Reset(%02x)", *(cp + 2));
		scs_dbcs_subfield = 0;
		scs_cs = 0;
		break;
	    case SCS_SA_HIGHLIGHT:
		trace_ds(" Highlight(%02x)", *(cp + 2));
		break;
	    case SCS_SA_CS:
		trace_ds(" CharacterSet(%02x)", *(cp + 2));
		if (scs_cs != *(cp + 2)) {
		    if (scs_cs == 0xf8) {
			scs_dbcs_subfield = 0;
		    } else if (*(cp + 2) == 0xf8) {
			scs_dbcs_subfield = 1;
		    }
		    scs_cs = *(cp + 2);
		}
		break;
	    case SCS_SA_GRID:
		trace_ds(" Grid(%02x)", *(cp + 2));
		break;
	    default:
		trace_ds(" Unknown(%02x %02x)", *(cp + 1), *(cp + 2));
		break;
	    }
	    /* Skip it. */
	    cp += 2;
	    break;
	case SCS_TRN:	/* transparent */
	    END_TEXT("TRN");
	    /* Make sure a length byte is present. */
	    if ((cp + 1) >= buf + buflen) {
		LEFTOVER;
		break;
	    }
	    /* Skip over the order. */
	    cp++;
	    /*
	     * Next byte is the length of the transparent data,
	     * not including the length byte itself.
	     */
	    cnt = *cp;
	    if (cp + cnt - 1 >= buf + buflen) {
		cp--;
		LEFTOVER;
		break;
	    }
	    trace_ds("(%d)", cnt);
	    /* Copy out the data literally. */
	    add_scs_trn(cp+1, cnt);
	    cp += cnt;
	    scs_dbcs_subfield = 0;
	    break;
	case SCS_SET:	/* set... */
	    /* Skip over the first byte of the order. */
	    if (cp + 2 >= buf + buflen ||
		cp + *(cp + 2) - 1 >= buf + buflen) {
		END_TEXT("SET");
		LEFTOVER;
		break;
	    }
	    switch (*++cp) {
	    case SCS_SHF:	/* set horizontal format */
		END_TEXT("SHF");
		/* Take defaults first. */
		init_scs_horiz();
		/*
		 * The length is next.  It includes the
		 * length field itself.
		 */
		cnt = *++cp;
		trace_ds("(%d)", cnt);
		if (cnt < 2) {
		    break;	/* no more data */
		}
		/* Skip over the length byte. */
		if (!--cnt || cp + 1 >= buf + buflen) {
		    break;
		}
		/* The MPP is next. */
		mpp = *++cp;
		trace_ds(" mpp=%d", mpp);
		if (!mpp || mpp > MAX_MPP) {
		    mpp = MAX_MPP;
		}
		/* Skip over the MPP. */
		if (!--cnt || cp + 1 >= buf + buflen) {
		    break;
		}
		/* The LM is next. */
		lm = *++cp;
		trace_ds(" lm=%d", lm);
		if (lm < 1 || lm >= mpp) {
		    lm = 1;
		}
		/* Skip over the LM. */
		if (!--cnt || cp + 1 >= buf + buflen) {
		    break;
		}
		/* Skip over the RM. */
		cp++;
		trace_ds(" rm=%d", *cp);
		/* Next are the tab stops. */
		while (--cnt && cp + 1 < buf + buflen) {
		    tab = *++cp;
		    trace_ds(" tab=%d", tab);
		    if (tab >= 1 && tab <= mpp) {
			htabs[tab] = 1;
		    }
		}
		break;
	    case SCS_SLD:	/* set line density */
		END_TEXT("SLD");
		/*
		 * Skip over the second byte of the
		 * order.
		 */
		cp++;
		/*
		 * The length is next.  It does not
		 * include length field itself.
		 */
		if (cp >= buf + buflen) {
		    break;
		}
		cnt = *cp;
		trace_ds("(%d)", cnt);
		if (cnt != 2) {
		    break; /* be gentle */
		}
		cnt--;
		trace_ds(" %02x", *(cp + 1));
		cp += cnt;
		break;
	    case SCS_SVF:	/* set vertical format */
		END_TEXT("SVF");
		/* Take defaults first. */
		init_scs_vert();
		/*
		 * Skip over the second byte of the
		 * order.
		 */
		cp++;
		/*
		 * The length is next.  It includes the
		 * length field itself.
		 */
		if (cp >= buf + buflen) {
		    break;
		}
		cnt = *cp;
		trace_ds("(%d)", cnt);
		if (cnt < 2) {
		    break;	/* no more data */
		}
		/* Skip over the length byte. */
		cp++;
		cnt--;
		if (!cnt || cp >= buf + buflen) {
		    break;
		}
		/* The MPL is next. */
		mpl = *cp;
		trace_ds(" mpl=%d", mpl);
		if (!mpl || mpl > MAX_MPL) {
		    mpl = 1;
		}
		if (cnt < 2) {
		    bm = mpl;
		    break;
		}
		/* Skip over the MPL. */
		cp++;
		cnt--;
		if (!cnt || cp >= buf + buflen) {
		    break;
		}
		/* The TM is next. */
		tm = *cp;
		trace_ds(" tm=%d", tm);
		if (tm < 1 || tm >= mpl) {
		    tm = 1;
		}
		if (cnt < 2) {
		    break;
		}
		/* Skip over the TM. */
		cp++;
		cnt--;
		if (!cnt || cp >= buf + buflen) {
		    break;
		}
		/* The BM is next. */
		bm = *cp;
		trace_ds(" bm=%d", bm);
		if (bm < tm || bm >= mpl) {
		    bm = mpl;
		}
		if (cnt < 2) {
		    break;
		}
		/* Skip over the BM. */
		cp++;
		cnt--;
		/* Next are the tab stops. */
		while (cnt > 1 && cp < buf + buflen) {
		    tab = *cp;
		    trace_ds(" tab=%d", tab);
		    if (tab >= 1 && tab <= mpp) {
			vtabs[tab] = 1;
		    }
		    cp++;
		    cnt--;
		}
		break;
	    default:
		END_TEXT("SET(?");
		trace_ds("%02x)", *cp);
		cp += *(cp + 1);
		break;
	    }
	    break;
	case SCS_SO:	/* DBCS subfield start */
	    END_TEXT("SO");
	    scs_dbcs_subfield = 1;
	    break;
	case SCS_SI:	/* DBCS subfield end */
	    END_TEXT("SI");
	    scs_dbcs_subfield = 0;
	    break;
	default:
	    /*
	     * Stray control codes are spaces, all else gets
	     * translated from EBCDIC to ASCII.
	     */
	    if (*cp <= 0x3f) {
		END_TEXT("?");
		trace_ds("%02x", *cp);
		if (add_scs(' ') < 0) {
		    return PDS_FAILED;
		}
		break;
	    }

	    if (last == NONE) {
		trace_ds("'");
	    } else if (last == ORDER) {
		trace_ds(" '");
	    }
	    if (scs_dbcs_subfield && dbcs) {
		if (scs_dbcs_subfield % 2) {
		    scs_dbcs_c1 = *cp;
		} else {
		    uc = ebcdic_to_unicode( (scs_dbcs_c1 << 8) | *cp, CS_BASE,
			    EUO_NONE);
		    if (uc == 0) {
			/* No translation. */
			trace_ds("?DBCS(X'%02x%02x')", scs_dbcs_c1, *cp);
			if (add_scs(' ') < 0) {
			    return PDS_FAILED;
			}
			if (add_scs(' ') < 0) {
			    return PDS_FAILED;
			}
		    } else {
			/*
			 * Add the Unicode character
			 * and a no-op to account for
			 * the right-hand side.
			 */
			trace_ds("DBCS(X'%02x%02x')", scs_dbcs_c1, *cp);
			if (add_scs(uc) < 0) {
			    return PDS_FAILED;
			}
			if (add_scs(FCORDER_NOP) < 0) {
				return PDS_FAILED;
			}
		    }
		}
		scs_dbcs_subfield++;
		last = DATA;
		break;
	    }
	    uc = ebcdic_to_unicode(*cp, CS_BASE, EUO_NONE);
	    {
		char mb[16];

		unicode_to_multibyte(uc, mb, sizeof(mb));
		trace_ds("%s", mb);
	    }
	    if (add_scs(uc) < 0) {
		return PDS_FAILED;
	    }
	    last = DATA;
	    break;
	}
    }

    if (last == DATA) {
	trace_ds("'");
    }
    trace_ds("\n");
    if (prflush() < 0) {
	return PDS_FAILED;
    }
    return PDS_OKAY_NO_OUTPUT;
}

/*
 * 'External' SCS function.  Handles leftover data from any previous,
 * incomplete SCS record.
 */
enum pds
process_scs(unsigned char *buf, size_t buflen)
{
    enum pds r;

    if (scs_leftover_len) {
	unsigned char *contig = Malloc(scs_leftover_len + buflen);
	size_t total_len;

	memcpy(contig, scs_leftover_buf, scs_leftover_len);
	memcpy(contig + scs_leftover_len, buf, buflen);
	total_len = scs_leftover_len + buflen;
	scs_leftover_len = 0;
	r = process_scs_contig(contig, total_len);
	Free(contig);
    } else {
	r = process_scs_contig(buf, buflen);
    }
    return r;
}

#if !defined(_WIN32) /*[*/
/*
 * SIGCHLD handler.  Does nothing, but on systems that conform to the Single
 * Unix Specification, defining it ensures that the print command process will
 * become a zombie if it exits prematurely.
 */
static void
sigchld_handler(int sig)
{
}

/*
 * Special version of popen where the child ignores SIGINT.
 */
static FILE *
popen_no_sigint(const char *command)
{
    int fds[2];
    FILE *f;

    /* Create a pipe. */
    if (pipe(fds) < 0) {
	return NULL;
    }

    /* Create a stdio stream from the write end. */
    f = fdopen(fds[1], "w");
    if (f == NULL) {
	close(fds[0]);
	close(fds[1]);
	return NULL;
    }

    /* Handle SIGCHLD signals. */
    signal(SIGCHLD, sigchld_handler);

    /* Fork a child process. */
    switch ((prpid = fork())) {
    case 0:		/* child */
	fclose(f);
	dup2(fds[0], 0);
	close(fds[1]);
	signal(SIGINT, SIG_IGN);
	execl("/bin/sh", "sh", "-c", command, NULL);

	/* execl failed, return nonzero status */
	exit(1);
	break;
    case -1:	/* parent, error */
	fclose(f);
	close(fds[0]);
	return NULL;
    default:	/* parent, success */
	close(fds[0]);
	break;
    }

    return f;
}

static int
pclose_no_sigint(FILE *f)
{
    int rc;
    int status;

    fclose(f);
    do {
	rc = waitpid(prpid, &status, 0);
    } while (rc < 0 && errno == EINTR);
    prpid = -1;
    if (rc < 0) {
	return rc;
    } else {
	return status;
    }
}
#endif /*]*/

/*
 * Send a character to the printer.
 */
static int
stash(unsigned char c)
{
#if defined(_WIN32) /*[*/
    if (!ws_initted) {
	if (ws_start(options.printer) < 0) {
	    return -1;
	}
	ws_initted = 1;
    }
    if (ws_needpre) {
	if ((options.trnpre != NULL) && copyfile(options.trnpre) < 0) {
	    return -1;
	}
	ws_needpre = 0;
    }

    trace_pdc(c);
    if (ws_putc(c)) {
	return -1;
    }
#else /*][*/
    if (prfile == NULL) {
	prfile = popen_no_sigint(options.command);
	if (prfile == NULL) {
	    errmsg("%s: %s", options.command, strerror(errno));
	    return -1;
	}
	if ((options.trnpre != NULL) && copyfile(options.trnpre) < 0) {
	    pclose_no_sigint(prfile);
	    prfile = NULL;
	    return -1;
	}
    }

    trace_pdc(c);
    if (fputc(c, prfile) == EOF) {
	errmsg("Write error to '%s': %s", options.command, strerror(errno));
	pclose_no_sigint(prfile);
	prfile = NULL;
	return -1;
    }
#endif /*]*/

    return 0;
}

/*
 * Flush the pipe going to the printer process, to try to flush out any
 * pending errors.
 */
static int
prflush(void)
{
#if defined(_WIN32) /*[*/
    if (ws_initted && ws_flush() < 0) {
	return -1;
    }
#else /*][*/
    if (prfile != NULL) {
	if (fflush(prfile) < 0) {
	    errmsg("Flush error to '%s': %s", options.command, strerror(errno));
	    pclose_no_sigint(prfile);
	    prfile = NULL;
	    return -1;
	}
    }
#endif /*]*/
    return 0;
}

/*
 * Change a character in the 3270 buffer.
 */
void
ctlr_add(unsigned char ebc, ucs4_t c, unsigned char cs, unsigned char gr)
{
    /* Map control characters, according to the write mode. */
    if (c < ' ') {
	if (wcc_line_length) {
	    /*
	     * When formatted, all control characters but FFs and
	     * the funky VISIBLE/INVISIBLE controls are translated
	     * to NULLs, so they don't display, and don't
	     * contribute to empty lines.
	     */
	    if (c != FCORDER_FF && c != VISIBLE && c != INVISIBLE) {
		c = '\0';
	    }
	} else {
	    /*
	     * Unformatted, all control characters but CR/NL/FF/EM
	     * are displayed as spaces.
	     */
	    if (c != FCORDER_CR && c != FCORDER_NL &&
		c != FCORDER_FF && c != FCORDER_EM) {
		c = ' ';
	    }
	}
    }

    /* Add the character. */
    page_buf[baddr] = c;
    if (ebc >= 0x40)
	    xlate_len[baddr] = xtable_lookup(ebc, &xlate_buf[baddr]);
    baddr = (baddr + 1) % MAX_BUF;
    any_3270_output = 1;
    ffeoj_last = false;

    /* Implement -emflush mode. */
    if (options.emflush && !wcc_line_length && c == FCORDER_EM) {
	/* XXX: Unfortunately, we do not return error status here. */
	dump_unformatted();
	baddr = 1;
	any_3270_output = 0;
    }
}

static struct {
    char buf;			/* printable data */
    unsigned char *trn;		/* transparent data */
    unsigned trn_len;		/* length of transparent data */
} uo_data[MAX_UNF_MPP + 2];	/* room for full line plus carriage control */
static unsigned uo_col;		/* current output column */
static unsigned uo_maxcol;	/* maximum column buffered */
static bool uo_last_cr = false; /* last data was CR */

/*
 * Dump and free any transparent unformatted data at col.
 */
static int
dump_uo_trn(unsigned col)
{
    unsigned i;
    int rv = 0;

    if (uo_data[col].trn != NULL) {
	for (i = 0; i < uo_data[col].trn_len; i++) {
	    if (stash(uo_data[col].trn[i]) < 0) {
		rv = -1;
		break;
	    }
	}
	Free(uo_data[col].trn);
	uo_data[col].trn = NULL;
	uo_data[col].trn_len = 0;
    }
    return rv;
}

/*
 * Dump pending unformatted output.
 */
static int
dump_uo(void)
{
    unsigned i;

    for (i = 0; i < uo_maxcol; i++) {
	if (dump_uo_trn(i) < 0) {
	    return -1;
	}
	if (!i && options.skipcc) {
	    continue;
	}
	if (stash(uo_data[i].buf) < 0) {
	    return -1;
	}
    }
    if (uo_maxcol < MAX_UNF_MPP + 2) {
	if (dump_uo_trn(uo_maxcol) < 0) {
	    return -1;
	}
    }
    return 0;
}

/*
 * Unformatted output function.  Processes one character of output data.
 *
 * This function will buffer up to MPP characters of output, until it is
 * passed a '\n' or '\f' character.
 *
 * By default, it will process '\r' characters like a printer, i.e., it will
 * not overwrite a buffered non-space character with a space character.  This
 * is how an output line can span multiple 3270 unformatted write commands.
 *
 * If 'crthru' is set, '\r' characters simply trigger a buffer flush.
 */
static int
uoutput(char c)
{
    switch (c) {
    case '\r':
	if (options.crthru) {
	    if (dump_uo() < 0) {
		return -1;
	    }
	    if (stash(c) < 0) {
		return -1;
	    }
	    uo_col = uo_maxcol = 0;
	    uo_last_cr = true;
	} else {
	    uo_col = 0;
	}
	break;
    case '\n':
	if (dump_uo() < 0) {
	    return -1;
	}
	if (options.crlf && !uo_last_cr) {
	    if (stash('\r') < 0) {
		return -1;
	    }
	}
	if (stash(c) < 0) {
	    return -1;
	}
	uo_col = uo_maxcol = 0;
	uo_last_cr = false;
	break;
    case '\f':
	uo_last_cr = false;
	if (any_3270_printable || !options.ffskip) {
	    if (dump_uo() < 0) {
		return -1;
	    }
	    if (stash(c) < 0) {
		return -1;
	    }
	}
	uo_col = uo_maxcol = 0;
	break;
    default:
	uo_last_cr = false;

	/* Don't overwrite with spaces. */
	if (c == ' ') {
	    if (uo_col >= uo_maxcol) {
		uo_data[uo_col++].buf = c;
	    } else {
		uo_col++;
	    }
	} else {
	    uo_data[uo_col++].buf = c;
	    any_3270_printable = true;
	}
	if (uo_col > uo_maxcol) {
	    uo_maxcol = uo_col;
	}
	break;
    }
    return 0;
}

/*
 * Add transparent data to the unformatted output buffer.
 */
static void
uoutput_trn(const unsigned char *s, int len)
{
    unsigned char *new;

    if (len <= 0) {
	return;
    }
    new = Realloc(uo_data[uo_col].trn, uo_data[uo_col].trn_len + len);
    if (uo_data[uo_col].trn != NULL) {
	memcpy(new, uo_data[uo_col].trn, uo_data[uo_col].trn_len);
    }
    memcpy(new + uo_data[uo_col].trn_len, s, len);
    uo_data[uo_col].trn = new;
    uo_data[uo_col].trn_len += len;
}

/*
 * Dump an unformatted output buffer.
 *
 * The buffer is treated as a sequence of characters, with control characters
 * for new line, carriage return, form feed and end of media.
 *
 * By definition, the "print position" is 0 when this function begins and ends.
 */
static int
dump_unformatted(void)
{
    int i;
    int prcol = 0;
    ucs4_t c;
    int done = 0;
    char mb[16];
    const char *mbp;
    int len;
    int j;

    if (!any_3270_output) {
	return 0;
    }

    for (i = 0; i < MAX_BUF && !done; i++) {
	switch (c = page_buf[i]) {
	case '\0':
	    break;
	case FCORDER_NOP:
	    break;
	case FCORDER_CR:
	    if (uoutput('\r') < 0) {
		return -1;
	    }
	    prcol = 0;
	    break;
	case FCORDER_NL:
	    if (uoutput('\n') < 0) {
		return -1;
	    }
	    prcol = 0;
	    break;
	case FCORDER_FF:
	    if (uoutput('\f') < 0) {
		return -1;
	    }
	    prcol = 0;
	    break;
	case FCORDER_EM:
	    if (prcol != 0) {
		if (uoutput('\n') < 0) {
		    return -1;
		}
	    }
	    done = 1;
	    break;
	default:	/* printable */
	    /*
	     * Insert a newline if they send a 133rd printable
	     * character on a line.
	     *
	     * If they specified '-skipcc', don't count the first
	     * character on the line as printable.
	     */
	    if (++prcol > options.mpp + (options.skipcc != 0)) {
		if (uoutput('\n') < 0) {
		    return -1;
		}
		prcol = 0;
	    }

	    /* Handle transparent data. */
	    if (xlate_buf[i] != NULL) {
		uoutput_trn(xlate_buf[i], xlate_len[i]);
		break;
	    }

#if !defined(_WIN32) /*[*/
	    len = unicode_to_multibyte(c, mb, sizeof(mb));
#else /*][*/
	    len = unicode_to_printer(c, mb, sizeof(mb));
#endif /*]*/
	    if (len == 0) {
		mb[0] = ' ';
		len = 1;
	    } else {
		len--;
	    }
	    mbp = mb;
	    for (j = 0; j < len; j++) {
		if (uoutput(mbp[j]) < 0) {
		    return -1;
		}
	    }

	    break;
	}
    }

    /* If the buffer didn't end with an EM, flush any pending line. */
    if (!done) {
	if (uoutput('\n') < 0) {
	    return -1;
	}
    }

    /* Clear out the buffer. */
    memset(page_buf, '\0', MAX_BUF * sizeof(ucs4_t));
    memset(xlate_buf, '\0', MAX_BUF * sizeof(unsigned char *));
    memset(xlate_len, '\0', MAX_BUF * sizeof(int));

    /* Clear the output state. */
    for (i = 0; i < MAX_UNF_MPP + 2; i++) {
	uo_data[i].buf = 0;
	if (uo_data[i].trn != NULL) {
	    Free(uo_data[i].trn);
	}
	uo_data[i].trn = NULL;
	uo_data[i].trn_len = 0;
    }
    uo_col = 0;
    uo_maxcol = 0;
    uo_last_cr = false;

    /* Flush buffered data. */
#if defined(_WIN32) /*[*/
    if (ws_initted) {
	ws_flush();
    }
#else /*][*/
    fflush(prfile);
#endif /*]*/
    any_3270_output = 0;

    return 0;
}

/*
 * Dump a formatted output buffer.
 *
 * The buffer is treated as a sequence of lines, with the length specified by
 * the write control character.  
 *
 * Each line is terminated by a newline, with trailing spaces and nulls
 * suppressed.
 * Nulls are displayed as spaces, except when they constitute an entire line,
 * in which case the line is suppressed.
 * Formfeeds are passed through, and otherwise treated like nulls.
 *
 * We do not observe the -skipcc option with formatted data.
 */
static int
dump_formatted(void)
{
    int i;
    ucs4_t *cp = page_buf;
    int visible = 1;
    int newlines = 0;
    bool data_without_newline = false;

    if (!any_3270_output) {
	return 0;
    }
    for (i = 0; i < MAX_UNF_MPP; i++) {
	int blanks = 0;
	int any_data = 0;
	int j;

	for (j = 0; j < line_length && ((i * line_length) + j) < MAX_BUF; j++) {
	    char c = *cp++;

	    switch (c) {
	    case VISIBLE:	/* visible field */
		visible = 1;
		blanks++;
		break;
	    case INVISIBLE:	/* invisible field */
		visible = 0;
		blanks++;
		break;
	    case '\f':
		while (newlines) {
		    if (options.crlf) {
			if (stash('\r') < 0) {
			    return -1;
			}
		    }
		    if (stash('\n') < 0) {
			return -1;
		    }
		    newlines--;
		    data_without_newline = false;
		}
		if (any_3270_printable || !options.ffskip) {
		    if (stash('\f') < 0) {
			return -1;
		    }
		}
		blanks++;
		break;
	    case '\0':
		blanks++;
		break;
	    case ' ':
		blanks++;
		any_data++;
		data_without_newline = true;
		break;
	    default:
		while (newlines) {
		    if (options.crlf) {
			if (stash('\r') < 0) {
			    return -1;
			}
		    }
		    if (stash('\n') < 0) {
			return -1;
		    }
		    newlines--;
		    data_without_newline = false;
		}
		while (blanks) {
		    if (stash(' ') < 0) {
			return -1;
		    }
		    blanks--;
		}
		any_data++;
		data_without_newline = true;
		if (!visible) {
		    if (stash(' ') < 0) {
			return -1;
		    }
		} else {
		    char mb[16];
		    int len;
		    int j;

#if !defined(_WIN32) /*[*/
		    len = unicode_to_multibyte(c, mb, sizeof(mb));
#else /*][*/
		    len = unicode_to_printer(c, mb, sizeof(mb));
#endif /*]*/
		    if (len == 0) {
			mb[0] = ' ';
			len = 1;
		    } else {
			len--;
		    }
		    for (j = 0; j < len; j++) {
			if (stash(mb[j]) < 0) {
			    return -1;
			}
		    }

		}
		if (visible) {
		    any_3270_printable = true;
		}
		break;
	    }
	}
	if (any_data || options.blanklines) {
	    newlines++;
	}
    }

    /* If there was data on the last line, put out a newline. */
    if (data_without_newline) {
	if (options.crlf) {
	    if (stash('\r') < 0) {
		return -1;
	    }
	}
	if (stash('\n') < 0) {
	    return -1;
	}
    }

    /* Clear the buffer. */
    memset(page_buf, '\0', MAX_BUF * sizeof(ucs4_t));
#if defined(_WIN32) /*[*/
    if (ws_initted) {
	    ws_flush();
    }
#else /*][*/
    fflush(prfile);
#endif /*]*/
    any_3270_output = 0;

    return 0;
}

int
print_eoj(void)
{
    int rc = 0;

    /* Dump any pending 3270-mode output. */
    if (any_3270_output) {
	if (wcc_line_length) {
	    if (dump_formatted() < 0) {
		rc = -1;
	    }
	} else {
	    if (dump_unformatted() < 0) {
		rc = -1;
	    }
	}
    }

    /* Dump any pending SCS-mode output. */
    if (any_scs_output) {
	if (dump_scs_line(true, false) < 0) {
	    rc = -1;
	}
    }

    /* Handle -ffeoj, which blindly adds a formfeed to every page. */
    if (options.ffeoj && !ffeoj_last) {
	if (scs_any) {
	    trace_ds("Automatic SCS EOJ formfeed.\n");
	    scs_formfeed(true);
	    if (dump_scs_line(true, false) < 0) {
		rc = -1;
	    }
	} else {
	    trace_ds("Automatic 3270 %s EOJ formfeed.\n",
		    wcc_line_length? "formatted": "unformatted");
	    ctlr_add(0, FCORDER_FF, default_cs, default_gr);
	    if (wcc_line_length) {
		if (dump_formatted() < 0) {
		    rc = -1;
		}
	    } else {
		if (dump_unformatted() < 0) {
		    rc = -1;
		}
	    }
	}
	ffeoj_last = true;
    }

    /* Close the stream to the print process. */
#if defined(_WIN32) /*[*/
    if (ws_initted) {
	trace_ds("End of print job.\n");
	if (options.trnpost != NULL && copyfile(options.trnpost) < 0) {
	    rc = -1;
	}
	if (ws_endjob() < 0) {
	    rc = -1;
	}
	ws_needpre = 1;
    }
#else /*]*/
    if (prfile != NULL) {
	trace_ds("End of print job.\n");
	if (options.trnpost != NULL && copyfile(options.trnpost) < 0) {
	    rc = -1;
	}
	rc = pclose_no_sigint(prfile);
	if (rc) {
	    if (rc < 0) {
		errmsg("Close error on '%s': %s", options.command,
			strerror(errno));
	    } else if (WIFEXITED(rc)) {
		errmsg("'%s' exited with status %d", options.command,
			WEXITSTATUS(rc));
	    } else if (WIFSIGNALED(rc)) {
		errmsg("'%s' terminated by signal %d", options.command,
			WTERMSIG(rc));
	    } else {
		errmsg("'%s' returned status %d", options.command, rc);
	    }
	    rc = -1;
	}
	prfile = NULL;
    }
#endif /*]*/

    /* Make sure the next 3270 job starts with clean conditions. */
    page_buf_initted = 0;

    /* Reset the FF suprpession logic. */
    any_3270_printable = false;

    return rc;
}

void
print_unbind(void)
{
    /*
     * Make sure that the next SCS job starts with clean conditions.
     */
    scs_initted = false;
}

static int
ctlr_erase(void)
{
    /* Dump whatever we've got so far. */
    /* Dump any pending 3270-mode output. */
    if (wcc_line_length) {
	if (dump_formatted() < 0) {
		return -1;
	}
    } else {
	    if (dump_unformatted() < 0) {
		return -1;
	    }
    }

    /* Dump any pending SCS-mode output. */
    if (any_scs_output) {
	if (dump_scs_line(true, false) < 0) { /* XXX: 1st true? */
	    return -1;
	}
    }

    /* Make sure the buffer is clean. */
    memset(page_buf, '\0', MAX_BUF * sizeof(ucs4_t));
    any_3270_output = 0;
    baddr = 0;
    return 0;
}

/*
 * Copy a -trnpre/-trnpost file to the printer.  We open and read the file
 * for each print job, so someone can change their contents while we are
 * running (hopefully between print jobs).
 */
static int
copyfile(const char *filename)
{
    FILE *f;
    int c;
    int rc = 0;

    if ((f = fopen(filename, "rb")) == NULL) {
	errmsg("%s: %s", filename, strerror(errno));
	return -1;
    }
    while ((c = fgetc(f)) != EOF) {
	trace_pdc((unsigned char)c);
#if defined(_WIN32) /*[*/
	if (ws_putc(c) < 0) {
#else /*][*/
	if (fputc(c, prfile) < 0) {
	    errmsg("write(%s): %s", options.command, strerror(errno));
#endif /*]*/
	    rc = -1;
	    break;
	}
    }
    fclose(f);
    return rc;
}
