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
 *		maintenance of the 3270 device state.  It was split out from
 *		screen.c, which handles X operations.
 *
 */

#include "globals.h"

#include <errno.h>
#include <assert.h>

#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "screen.h"
#include "resources.h"
#include "toggles.h"

#include "actions.h"
#include "codepage.h"
#include "ctlrc.h"
#include "unicodec.h"
#include "ft.h"
#include "ft_cut.h"
#include "ft_dft.h"
#include "host.h"
#include "kybd.h"
#include "popups.h"
#include "screen.h"
#include "scroll.h"
#include "see.h"
#include "selectc.h"
#include "sf.h"
#include "tables.h"
#include "task.h"
#include "telnet_core.h"
#include "telnet.h"
#include "trace.h"
#include "txa.h"
#include "screentrace.h"
#include "utils.h"
#include "vstatus.h"

/* Globals */
int ROWS, COLS;
int maxROWS, maxCOLS;
int defROWS, defCOLS;
int altROWS, altCOLS;
int ov_rows, ov_cols;
bool ov_auto;
int model_num = 4;
bool mode3279 = true;
int cursor_addr, buffer_addr;
bool screen_alt = false;	/* alternate screen? */
bool is_altbuffer = false;
struct ea *ea_buf;		/* 3270 device buffer */
				/* ea_buf[-1] is the dummy default field
				   attribute */
struct ea *aea_buf;	/* alternate 3270 extended attribute buffer */
#if defined(CHECK_AEA_BUF) /*[*/
unsigned long ea_sum, aea_sum;
#endif /*]*/
bool formatted = false;	/* set in screen_disp */
bool screen_changed = false;
int first_changed = -1;
int last_changed = -1;
unsigned char reply_mode = SF_SRM_FIELD;
int crm_nattr = 0;
unsigned char crm_attr[16];
bool dbcs = false;

/* Statics */
static unsigned char *zero_buf;	/* empty buffer, for area clears */
static void set_formatted(void);
static void ctlr_blanks(void);
static bool trace_primed = false;
static unsigned char default_fg;
static unsigned char default_bg;
static unsigned char default_gr;
static unsigned char default_cs;
static unsigned char default_ic;
static void ctlr_negotiating(bool ignored);
static void ctlr_connect(bool ignored);
static int sscp_start;
static void ctlr_add_ic(int baddr, unsigned char ic);
static bool ctlr_initted = false;

static void ticking_stop(struct timeval *tp);

/*
 * code_table is used to translate buffer addresses and attributes to the 3270
 * datastream representation
 */
static unsigned char code_table[64] = {
    0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
};

#define IsBlank(c)	((c == EBC_null) || (c == EBC_space))

#define ALL_CHANGED	{ \
	screen_changed = true; \
	if (IN_NVT) { first_changed = 0; last_changed = ROWS*COLS; } }
#define REGION_CHANGED(f, l)	{ \
	screen_changed = true; \
	if (IN_NVT) { \
	    if (first_changed == -1 || f < first_changed) first_changed = f; \
	    if (last_changed == -1 || l > last_changed) last_changed = l; } }
#define ONE_CHANGED(n)	REGION_CHANGED(n, n+1)

#define DECODE_BADDR(c1, c2) \
	((((c1) & 0xC0) == 0x00) ? \
	(((c1) & 0x3F) << 8) | (c2) : \
	(((c1) & 0x3F) << 6) | ((c2) & 0x3F))

#define ENCODE_BADDR(ptr, addr) { \
    	if ((ROWS * COLS) > 0x1000) { \
		*(ptr)++ = ((addr) >> 8) & 0x3F; \
		*(ptr)++ = (addr) & 0xFF; \
	} else { \
		*(ptr)++ = code_table[((addr) >> 6) & 0x3F]; \
		*(ptr)++ = code_table[(addr) & 0x3F]; \
	} \
    }

/**
 * Controller module registration.
 */
void
ctlr_register(void)
{
    /* Register callback routines. */
    register_schange(ST_NEGOTIATING, ctlr_negotiating);
    register_schange(ST_CONNECT, ctlr_connect);
    register_schange(ST_3270_MODE, ctlr_connect);
}

/*
 * Initialize the emulated 3270 hardware.
 */
void
ctlr_init(unsigned cmask)
{
    ctlr_reinit(cmask);
}

/*
 * Reinitialize the emulated 3270 hardware.
 */
void
ctlr_reinit(unsigned cmask)
{
    static struct ea *real_ea_buf = NULL;
    static struct ea *real_aea_buf = NULL;

    ctlr_initted = true;
    if (cmask & MODEL_CHANGE) {
	/* Allocate buffers */
	if (real_ea_buf) {
	    Free((char *)real_ea_buf);
	}
	real_ea_buf = (struct ea *)Calloc(sizeof(struct ea),
		(maxROWS * maxCOLS) + 1);
	ea_buf = real_ea_buf + 1;
	if (real_aea_buf) {
	    Free((char *)real_aea_buf);
	}
	real_aea_buf = (struct ea *)Calloc(sizeof(struct ea),
		(maxROWS * maxCOLS) + 1);
	aea_buf = real_aea_buf + 1;
#if defined(CHECK_AEA_BUF) /*[*/
	ea_sum = 0;
	aea_sum = 0;
#endif /*]*/
	Replace(zero_buf, (unsigned char *)Calloc(sizeof(struct ea),
		    maxROWS * maxCOLS));
	cursor_addr = 0;
	buffer_addr = 0;

	ea_buf[-1].fa  = FA_PRINTABLE | FA_MODIFY;
	ea_buf[-1].ic  = 1;
	aea_buf[-1].fa = FA_PRINTABLE | FA_MODIFY;
	aea_buf[-1].ic = 1;
    }
}

/*
 * Checks a model number and oversize rows and columns.
 * Ideally this should be called by set_rows_cols() below.
 */
bool
check_rows_cols(int mn, unsigned ovc, unsigned ovr)
{
    unsigned mxc, mxr; /* Maximum rows, columns */

    switch (mn) {
    case 2:
	mxc = MODEL_2_COLS;
	mxr = MODEL_2_ROWS; 
	break;
    case 3:
	mxc = MODEL_3_COLS;
	mxr = MODEL_3_ROWS; 
	break;
    case 4:
	mxc = MODEL_4_COLS;
	mxr = MODEL_4_ROWS; 
	break;
    case 5:
	mxc = MODEL_5_COLS;
	mxr = MODEL_5_ROWS; 
	break;
    default:
	popup_an_error("Unknown model: %d", mn);
	return false;
    }

    /* Check oversize. */
    if (ovc > 0 || ovr > 0) {
	if (ovc == 0) {
	    popup_an_error("Invalid %s %dx%d columns:\nzero", ResOversize, ovc, ovr);
	    return false;
	} else if (ovr == 0) {
	    popup_an_error("Invalid %s %dx%d rows:\nzero", ResOversize, ovc, ovr);
	    return false;
	} else if (ovc > MAX_ROWS_COLS || ovr > MAX_ROWS_COLS || ovc * ovr > MAX_ROWS_COLS) {
	    popup_an_error("Invalid %s %dx%d:\nExceeds protocol limit", ResOversize, ovc, ovr);
	    return false;
	} else if (ovc > 0 && ovc < mxc) {
	    popup_an_error("Invalid %s columns (%d):\nLess than model %d columns (%d)", ResOversize, ovc, mn, mxc);
	    return false;
	} else if (ovr > 0 && ovr < mxr) {
	    popup_an_error("Invalid %s rows (%d):\nLess than model %d rows (%d)", ResOversize, ovr, mn, mxr);
	    return false;
	}
    }
    return true;
}

/*
 * Deal with the relationships between model numbers and rows/cols.
 */
void
set_rows_cols(int mn, int ovc, int ovr)
{
    int defmod;

    if (ovc < 0 || ovr < 0) {
	ov_auto = true;
	ovc = 0;
	ovr = 0;
    }

    switch (mn) {
    case 2:
	maxCOLS = MODEL_2_COLS;
	maxROWS = MODEL_2_ROWS; 
	model_num = 2;
	break;
    case 3:
	maxCOLS = MODEL_3_COLS;
	maxROWS = MODEL_3_ROWS; 
	model_num = 3;
	break;
    case 4:
	maxCOLS = MODEL_4_COLS;
	maxROWS = MODEL_4_ROWS; 
	model_num = 4;
	break;
    case 5:
	maxCOLS = MODEL_5_COLS;
	maxROWS = MODEL_5_ROWS; 
	model_num = 5;
	break;
    default:
	defmod = 4;
	popup_an_error("Unknown model: %d\nDefaulting to %d", mn, defmod);
	set_rows_cols(defmod, ovc, ovr);
	return;
    }

    /* Apply oversize. */
    ov_cols = 0;
    ov_rows = 0;
    if (ovc != 0 || ovr != 0) {
	if (ovc <= 0 || ovr <= 0) {
	    popup_an_error("Invalid %s %dx%d:\nNegative or zero", ResOversize, ovc, ovr);
	} else if (ovc > MAX_ROWS_COLS || ovr > MAX_ROWS_COLS || ovc * ovr > MAX_ROWS_COLS) {
	    popup_an_error("Invalid %s %dx%d:\nExceeds protocol limit", ResOversize, ovc, ovr);
	} else if (ovc > 0 && ovc < maxCOLS) {
	    popup_an_error("Invalid %s cols (%d):\nLess than model %d cols (%d)", ResOversize, ovc, model_num, maxCOLS);
	} else if (ovr > 0 && ovr < maxROWS) {
	    popup_an_error("Invalid %s rows (%d):\nLess than model %d rows (%d)", ResOversize, ovr, model_num, maxROWS);
	} else {
	    ov_cols = maxCOLS = ovc;
	    ov_rows = maxROWS = ovr;
	}
    }

    /* Make sure that the current rows/cols are still 24x80. */
    COLS = defCOLS = MODEL_2_COLS;
    ROWS = defROWS = MODEL_2_ROWS;
    screen_alt = false;

    /* Set the defaults for the alternate screen size. */
    altROWS = maxROWS;
    altCOLS = maxCOLS;

    /* The model changed. */
    st_changed(ST_REMODEL, true);
    if (ctlr_initted) {
	ctlr_reinit(MODEL_CHANGE);
    }
}

/*
 * Stop the timeout in the OIA.
 * Called when there is an explicit Reset().
 */
void
ctlr_reset(void)
{
    ticking_stop(NULL);
    vstatus_untiming();
}

/*
 * Set the formatted screen flag.  A formatted screen is a screen that
 * has at least one field somewhere on it.
 */
static void
set_formatted(void)
{
    int baddr;

    formatted = false;
    baddr = 0;
    do {
	if (ea_buf[baddr].fa) {
	    formatted = true;
	    break;
	}
	INC_BA(baddr);
    } while (baddr != 0);
}

/*
 * Called when protocol negotiation is in progress.
 */
static void
ctlr_negotiating(bool ignored _is_unused)
{
    ticking_start(true);
}


/*
 * Called when a host connects, disconnects, or changes NVT/3270 modes.
 */
static void
ctlr_connect(bool ignored _is_unused)
{
    ticking_stop(NULL);
    vstatus_untiming();

    if (!IN_3270 || (IN_SSCP && (kybdlock & KL_OIA_TWAIT))) {
	kybdlock_clr(KL_OIA_TWAIT, "ctlr_connect");
	vstatus_reset();
    }

    default_fg = 0x00;
    default_bg = 0x00;
    default_gr = 0x00;
    default_cs = 0x00;
    default_ic = 0x00;
    reply_mode = SF_SRM_FIELD;
    crm_nattr = 0;

    /* On disconnect, reset the default and alternate dimensions. */
    if (CONNECTED) {
	ctlr_enable_cursor(true, EC_CONNECT);
    } else {
	ctlr_enable_cursor(false, EC_CONNECT);
	defROWS = MODEL_2_ROWS;
	defCOLS = MODEL_2_COLS;
	altROWS = maxROWS;
	altCOLS = maxCOLS;
    }
}

/*
 * Find the buffer address of the field attribute for a given buffer address.
 * Returns -1 if the screen isn't formatted.
 */
int
find_field_attribute_ea(int baddr, struct ea *ea)
{
    int sbaddr;

    sbaddr = baddr;    
    do {   
	if (ea[baddr].fa) {
	    return baddr;
	}
	DEC_BA(baddr);
    } while (baddr != sbaddr);
    return -1;
}

/*
 * Find the buffer address of the field attribute for a given buffer address.
 * Returns -1 if the screen isn't formatted.
 */
int
find_field_attribute(int baddr)
{
    if (!formatted) {
	return -1;
    }
    return find_field_attribute_ea(baddr, ea_buf);
}

/*
 * Find the field attribute for the given buffer address.  Return its address
 * rather than its value.
 */
unsigned char
get_field_attribute(register int baddr)
{
    return ea_buf[find_field_attribute(baddr)].fa;
}

/*
 * Find the field attribute for the given buffer address, bounded by another
 * buffer address.  Return the attribute in a parameter.
 *
 * Returns true if an attribute is found, false if boundary hit.
 */
bool
get_bounded_field_attribute(register int baddr, register int bound,
	unsigned char *fa_out)
{
    int sbaddr;

    if (!formatted) {
	*fa_out = ea_buf[-1].fa;
	return true;
    }

    sbaddr = baddr;
    do {
	if (ea_buf[baddr].fa) {
	    *fa_out = ea_buf[baddr].fa;
	    return true;
	}
	DEC_BA(baddr);
    } while (baddr != sbaddr && baddr != bound);

    /* Screen is unformatted (and 'formatted' is inaccurate). */
    if (baddr == sbaddr) {
	*fa_out = ea_buf[-1].fa;
	return true;
    }

    /* Wrapped to boundary. */
    return false;
}

/*
 * Given the address of a field attribute, return the address of the
 * extended attribute structure.
 */
struct ea *
fa2ea(int baddr)
{
    return &ea_buf[baddr];
}

/*
 * Find the next unprotected field.  Returns the address following the
 * unprotected attribute byte, or 0 if no nonzero-width unprotected field
 * can be found.
 */
int
next_unprotected(int baddr0)
{
    register int baddr, nbaddr;

    nbaddr = baddr0;
    do {
	baddr = nbaddr;
	INC_BA(nbaddr);
	if (ea_buf[baddr].fa &&
		!FA_IS_PROTECTED(ea_buf[baddr].fa) &&
		!ea_buf[nbaddr].fa) {
	    return nbaddr;
	}
    } while (nbaddr != baddr0);
    return 0;
}

/*
 * Perform an erase command, which may include changing the (virtual) screen
 * size.
 */
void
ctlr_erase(bool alt)
{
    int newROWS, newCOLS;

    kybd_inhibit(false);

    ctlr_clear(true);

    /* Let a blocked task go. */
    task_host_output();

    if (alt) {
	newROWS = altROWS;
	newCOLS = altCOLS;
    } else {
	newROWS = defROWS;
	newCOLS = defCOLS;
    }

    if (alt == screen_alt && ROWS == newROWS && COLS == newCOLS) {
	return;
    }

    screen_disp(true);
    if (visible_control) {
	/* Blank the entire display. */
	ctlr_blanks();
	ROWS = maxROWS;
	COLS = maxCOLS;
	screen_disp(false);
    }

    ROWS = newROWS;
    COLS = newCOLS;
    if (visible_control) {
	/* Fill the active part of the screen with NULLs again. */
	ctlr_clear(false);
	screen_disp(false);
    }

    screen_alt = alt;
}

/* Restore the keyboard. */
static void
restore_keyboard(void)
{
    aid = AID_NO;
    do_reset(false);
    ticking_stop(&net_last_recv_ts);
}

/*
 * Interpret an incoming 3270 command.
 */
enum pds
process_ds(unsigned char *buf, size_t buflen, bool kybd_restore)
{
    enum pds rv = PDS_OKAY_NO_OUTPUT;

    if (buflen) {
	scroll_to_bottom();

	switch (buf[0]) {	/* 3270 command */
	case CMD_EAU:	/* erase all unprotected */
	case SNA_CMD_EAU:
	    ctlr_erase_all_unprotected();
	    trace_ds("< EraseAllUnprotected\n");
	    break;
	case CMD_EWA:	/* erase/write alternate */
	case SNA_CMD_EWA:
	    ctlr_erase(true);
	    trace_ds("< EraseWriteAlternate");
	    rv = ctlr_write(buf, buflen, true);
	    break;
	case CMD_EW:	/* erase/write */
	case SNA_CMD_EW:
	    ctlr_erase(false);
	    trace_ds("< EraseWrite");
	    rv = ctlr_write(buf, buflen, true);
	    break;
	case CMD_W:	/* write */
	case SNA_CMD_W:
	    trace_ds("< Write");
	    rv = ctlr_write(buf, buflen, false);
	    break;
	case CMD_RB:	/* read buffer */
	case SNA_CMD_RB:
	    trace_ds("< ReadBuffer\n");
	    ctlr_read_buffer(aid);
	    rv = PDS_OKAY_OUTPUT;
	    break;
	case CMD_RM:	/* read modifed */
	case SNA_CMD_RM:
	    trace_ds("< ReadModified\n");
	    ctlr_read_modified(aid, false);
	    rv = PDS_OKAY_OUTPUT;
	    break;
	case CMD_RMA:	/* read modifed all */
	case SNA_CMD_RMA:
	    trace_ds("< ReadModifiedAll\n");
	    ctlr_read_modified(aid, true);
	    rv = PDS_OKAY_OUTPUT;
	    break;
	case CMD_WSF:	/* write structured field */
	case SNA_CMD_WSF:
	    trace_ds("< WriteStructuredField");
	    rv = write_structured_field(buf, buflen);
	    break;
	case CMD_NOP:	/* no-op */
	    trace_ds("< NoOp\n");
	    break;
	default:
	    /* unknown 3270 command */
	    popup_an_error("Unknown 3270 Data Stream command: X'%X'\n", buf[0]);
	    rv = PDS_BAD_CMD;
	    break;
	}
    }

    if (kybd_restore) {
	restore_keyboard();
    }
    return rv;
}

/*
 * Functions to insert SA attributes into the inbound data stream.
 */
static void
insert_sa1(unsigned char attr, unsigned char value, unsigned char *currentp,
	bool *anyp)
{
    if (value == *currentp) {
	return;
    }
    *currentp = value;
    space3270out(3);
    *obptr++ = ORDER_SA;
    *obptr++ = attr;
    *obptr++ = value;
    if (*anyp) {
	trace_ds("'");
    }
    trace_ds(" SetAttribute(%s)", see_efa(attr, value));
    *anyp = false;
}

/*
 * Translate an internal character set number to a 3270DS characte set number.
 */
static unsigned char
host_cs(unsigned char cs)
{
    switch (cs & CS_MASK) {
    case CS_APL:
    case CS_LINEDRAW:
	return 0xf0 | (cs & CS_MASK);
    case CS_DBCS:
	return 0xf8;
    default:
	return 0;
    }
}

static void
insert_sa(int baddr, unsigned char *current_fgp, unsigned char *current_bgp,
	unsigned char *current_grp, unsigned char *current_csp,
	unsigned char *current_icp, bool *anyp)
{
    if (reply_mode != SF_SRM_CHAR) {
	return;
    }

    if (memchr((char *)crm_attr, XA_FOREGROUND, crm_nattr)) {
	insert_sa1(XA_FOREGROUND, ea_buf[baddr].fg, current_fgp, anyp);
    }
    if (memchr((char *)crm_attr, XA_BACKGROUND, crm_nattr)) {
	insert_sa1(XA_BACKGROUND, ea_buf[baddr].bg, current_bgp, anyp);
    }
    if (memchr((char *)crm_attr, XA_HIGHLIGHTING, crm_nattr)) {
	unsigned char gr;

	gr = ea_buf[baddr].gr;
	if (gr) {
	    gr |= 0xf0;
	}
	insert_sa1(XA_HIGHLIGHTING, gr, current_grp, anyp);
    }
    if (memchr((char *)crm_attr, XA_CHARSET, crm_nattr)) {
	insert_sa1(XA_CHARSET, host_cs(ea_buf[baddr].cs), current_csp, anyp);
    }
}


/*
 * Process a 3270 Read-Modified command and transmit the data back to the
 * host.
 */
void
ctlr_read_modified(unsigned char aid_byte, bool all)
{
    int baddr, sbaddr;
    bool send_data = true;
    bool short_read = false;
    unsigned char current_fg = 0x00;
    unsigned char current_bg = 0x00;
    unsigned char current_gr = 0x00;
    unsigned char current_cs = 0x00;
    unsigned char current_ic = 0x00;

    if (IN_SSCP && aid_byte != AID_ENTER) {
	return;
    }

    if (aid_byte == AID_SF) {
	dft_read_modified();
	return;
    }

    trace_ds("> ");
    obptr = obuf;

    switch (aid_byte) {

    case AID_SYSREQ:			/* test request */
	space3270out(4);
	*obptr++ = EBC_soh;
	*obptr++ = EBC_percent;
	*obptr++ = EBC_slash;
	*obptr++ = EBC_stx;
	trace_ds("SysReq");
	break;

	case AID_PA1:			/* short-read AIDs */
	case AID_PA2:
	case AID_PA3:
	case AID_CLEAR:
	    if (!all) {
		short_read = true;
	    }
	    /* fall through... */

	case AID_SELECT:		/* No data on READ MODIFIED */
	    if (!all) {
		send_data = false;
	    }
	    /* fall through... */

	default:			/* ordinary AID */
	    if (!IN_SSCP) {
		space3270out(3);
		*obptr++ = aid_byte;
		trace_ds("%s", see_aid(aid_byte));
		if (short_read) {
		    goto rm_done;
		}
		ENCODE_BADDR(obptr, cursor_addr);
		trace_ds("%s", rcba(cursor_addr));
	    } else {
		space3270out(1);	/* just in case */
	    }
	    break;
    }

    baddr = 0;
    if (formatted) {
	/* find first field attribute */
	do {
	    if (ea_buf[baddr].fa) {
		break;
	    }
	    INC_BA(baddr);
	} while (baddr != 0);
	sbaddr = baddr;
	do {
	    if (FA_IS_MODIFIED(ea_buf[baddr].fa)) {
		bool any = false;

		INC_BA(baddr);
		space3270out(3);
		*obptr++ = ORDER_SBA;
		ENCODE_BADDR(obptr, baddr);
		trace_ds(" SetBufferAddress%s", rcba(baddr));
		while (!ea_buf[baddr].fa) {
		    if (send_data && ea_buf[baddr].ec) {
			insert_sa(baddr,
			    &current_fg,
			    &current_bg,
			    &current_gr,
			    &current_cs,
			    &current_ic,
			    &any);
			if (ea_buf[baddr].cs & CS_GE) {
			    space3270out(1);
			    *obptr++ = ORDER_GE;
			    if (any) {
				trace_ds("'");
			    }
			    trace_ds(" GraphicEscape");
			    any = false;
			}
			space3270out(1);
			*obptr++ = ea_buf[baddr].ec;
			if (ea_buf[baddr].ec <= 0x3f ||
			    ea_buf[baddr].ec == 0xff) {
			    if (any) {
				trace_ds("'");
			    }

			    trace_ds(" %s", see_ebc(ea_buf[baddr].ec));
			    any = false;
			} else {
			    if (!any) {
				trace_ds(" '");
			    }
			    trace_ds("%s", see_ebc(ea_buf[baddr].ec));
			    any = true;
			}
		    }
		    INC_BA(baddr);
		}
		if (any) {
		    trace_ds("'");
		}
	    } else {	/* not modified - skip */
		do {
		    INC_BA(baddr);
		} while (!ea_buf[baddr].fa);
	    }
	} while (baddr != sbaddr);
    } else {
	bool any = false;
	int nbytes = 0;

	/*
	 * If we're in SSCP-LU mode, the starting point is where the
	 * host left the cursor.
	 */
	if (IN_SSCP) {
	    baddr = sscp_start;
	}

	do {
	    if (ea_buf[baddr].ec) {
		insert_sa(baddr,
		    &current_fg,
		    &current_bg,
		    &current_gr,
		    &current_cs,
		    &current_ic,
		    &any);
		if (ea_buf[baddr].cs & CS_GE) {
		    space3270out(1);
		    *obptr++ = ORDER_GE;
		    if (any) {
			trace_ds("' ");
		    }
		    trace_ds(" GraphicEscape ");
		    any = false;
		}
		space3270out(1);
		*obptr++ = ea_buf[baddr].ec;
		if (ea_buf[baddr].ec <= 0x3f ||
		    ea_buf[baddr].ec == 0xff) {
		    if (any) {
			trace_ds("'");
		    }

		    trace_ds(" %s", see_ebc(ea_buf[baddr].ec));
		    any = false;
		} else {
		    if (!any) {
			trace_ds(" '");
		    }
		    trace_ds("%s", see_ebc(ea_buf[baddr].ec));
		    any = true;
		}
		nbytes++;
	    }
	    INC_BA(baddr);

	    /*
	     * If we're in SSCP-LU mode, end the return value at
	     * 255 bytes, or where the screen wraps.
	     */
	    if (IN_SSCP && (nbytes >= 255 || !baddr)) {
		break;
	    }
	} while (baddr != 0);
	if (any) {
	    trace_ds("'");
	}
    }

rm_done:
    trace_ds("\n");
    net_output();
}

/*
 * Process a 3270 Read-Buffer command and transmit the data back to the
 * host.
 */
void
ctlr_read_buffer(unsigned char aid_byte)
{
    int baddr;
    unsigned char fa;
    bool any = false;
    size_t attr_count = 0;
    unsigned char current_fg = 0x00;
    unsigned char current_bg = 0x00;
    unsigned char current_gr = 0x00;
    unsigned char current_cs = 0x00;
    unsigned char current_ic = 0x00;

    if (aid_byte == AID_SF) {
	dft_read_modified();
	return;
    }

    trace_ds("> ");
    obptr = obuf;

    space3270out(3);
    *obptr++ = aid_byte;
    ENCODE_BADDR(obptr, cursor_addr);
    trace_ds("%s%s", see_aid(aid_byte), rcba(cursor_addr));

    baddr = 0;
    do {
	if (ea_buf[baddr].fa) {
	    if (reply_mode == SF_SRM_FIELD) {
		space3270out(2);
		*obptr++ = ORDER_SF;
	    } else {
		space3270out(4);
		*obptr++ = ORDER_SFE;
		attr_count = obptr - obuf;
		*obptr++ = 1; /* for now */
		*obptr++ = XA_3270;
	    }
	    fa = ea_buf[baddr].fa & ~FA_PRINTABLE;
	    *obptr++ = code_table[fa];
	    if (any) {
		trace_ds("'");
	    }
	    trace_ds(" StartField%s%s%s",
		    (reply_mode == SF_SRM_FIELD) ? "" : "Extended",
		    rcba(baddr), see_attr(fa));
	    if (reply_mode != SF_SRM_FIELD) {
		if (ea_buf[baddr].fg) {
		    space3270out(2);
		    *obptr++ = XA_FOREGROUND;
		    *obptr++ = ea_buf[baddr].fg;
		    trace_ds("%s", see_efa(XA_FOREGROUND, ea_buf[baddr].fg));
		    (*(obuf + attr_count))++;
		}
		if (ea_buf[baddr].bg) {
		    space3270out(2);
		    *obptr++ = XA_BACKGROUND;
		    *obptr++ = ea_buf[baddr].bg;
		    trace_ds("%s", see_efa(XA_BACKGROUND, ea_buf[baddr].bg));
		    (*(obuf + attr_count))++;
		}
		if (ea_buf[baddr].gr) {
		    space3270out(2);
		    *obptr++ = XA_HIGHLIGHTING;
		    *obptr++ = ea_buf[baddr].gr | 0xf0;
		    trace_ds("%s", see_efa(XA_HIGHLIGHTING,
				ea_buf[baddr].gr | 0xf0));
		    (*(obuf + attr_count))++;
		}
		if (ea_buf[baddr].cs & CS_MASK) {
		    space3270out(2);
		    *obptr++ = XA_CHARSET;
		    *obptr++ = host_cs(ea_buf[baddr].cs);
		    trace_ds("%s", see_efa(XA_CHARSET,
				host_cs(ea_buf[baddr].cs)));
		    (*(obuf + attr_count))++;
		}
	    }
	    any = false;
	} else {
	    insert_sa(baddr,
		&current_fg,
		&current_bg,
		&current_gr,
		&current_cs,
		&current_ic,
		&any);
	    if (ea_buf[baddr].cs & CS_GE) {
		space3270out(1);
		*obptr++ = ORDER_GE;
		if (any) {
		    trace_ds("'");
		}
		trace_ds(" GraphicEscape");
		any = false;
	    }
	    space3270out(1);
	    *obptr++ = ea_buf[baddr].ec;
	    if (ea_buf[baddr].ec <= 0x3f ||
		ea_buf[baddr].ec == 0xff) {
		if (any) {
		    trace_ds("'");
		}

		trace_ds(" %s", see_ebc(ea_buf[baddr].ec));
		any = false;
	    } else {
		if (!any) {
		    trace_ds(" '");
		}
		trace_ds("%s", see_ebc(ea_buf[baddr].ec));
		any = true;
	    }
	}
	INC_BA(baddr);
    } while (baddr != 0);
    if (any) {
	trace_ds("'");
    }

    trace_ds("\n");
    net_output();
}

/*
 * Construct a 3270 command to reproduce the current state of the display, if
 * formatted.
 */
void
ctlr_snap_buffer(void)
{
    int baddr = 0;
    size_t attr_count;
    unsigned char current_fg = 0x00;
    unsigned char current_bg = 0x00;
    unsigned char current_gr = 0x00;
    unsigned char current_cs = 0x00;
    unsigned char current_ic = 0x00;
    unsigned char av;

    space3270out(2);
    *obptr++ = screen_alt ? CMD_EWA : CMD_EW;
    *obptr++ = code_table[(kybdlock &
		(KL_OERR_MASK | KL_OIA_TWAIT | KL_OIA_LOCKED))? 0:
	    WCC_KEYBOARD_RESTORE_BIT];

    do {
	if (ea_buf[baddr].fa) {
	    space3270out(4);
	    *obptr++ = ORDER_SFE;
	    attr_count = obptr - obuf;
	    *obptr++ = 1; /* for now */
	    *obptr++ = XA_3270;
	    *obptr++ = code_table[ea_buf[baddr].fa & ~FA_PRINTABLE];
	    if (ea_buf[baddr].fg) {
		space3270out(2);
		*obptr++ = XA_FOREGROUND;
		*obptr++ = ea_buf[baddr].fg;
		(*(obuf + attr_count))++;
	    }
	    if (ea_buf[baddr].bg) {
		space3270out(2);
		*obptr++ = XA_BACKGROUND;
		*obptr++ = ea_buf[baddr].fg;
		(*(obuf + attr_count))++;
	    }
	    if (ea_buf[baddr].gr) {
		space3270out(2);
		*obptr++ = XA_HIGHLIGHTING;
		*obptr++ = ea_buf[baddr].gr | 0xf0;
		(*(obuf + attr_count))++;
	    }
	    if (ea_buf[baddr].cs & CS_MASK) {
		space3270out(2);
		*obptr++ = XA_CHARSET;
		*obptr++ = host_cs(ea_buf[baddr].cs);
		(*(obuf + attr_count))++;
	    }
	} else {
	    av = ea_buf[baddr].fg;
	    if (current_fg != av) {
		current_fg = av;
		space3270out(3);
		*obptr++ = ORDER_SA;
		*obptr++ = XA_FOREGROUND;
		*obptr++ = av;
	    }
	    av = ea_buf[baddr].bg;
	    if (current_bg != av) {
		current_bg = av;
		space3270out(3);
		*obptr++ = ORDER_SA;
		*obptr++ = XA_BACKGROUND;
		*obptr++ = av;
	    }
	    av = ea_buf[baddr].gr;
	    if (av) {
		av |= 0xf0;
	    }
	    if (current_gr != av) {
		current_gr = av;
		space3270out(3);
		*obptr++ = ORDER_SA;
		*obptr++ = XA_HIGHLIGHTING;
		*obptr++ = av;
	    }
	    av = ea_buf[baddr].cs & CS_MASK;
	    if (av) {
		av = host_cs(av);
	    }
	    if (current_cs != av) {
		current_cs = av;
		space3270out(3);
		*obptr++ = ORDER_SA;
		*obptr++ = XA_CHARSET;
		*obptr++ = av;
	    }
	    av = ea_buf[baddr].ic;
	    if (current_ic != av) {
		current_ic = av;
		space3270out(3);
		*obptr++ = ORDER_SA;
		*obptr++ = XA_INPUT_CONTROL;
		*obptr++ = av;
	    }
	    if (ea_buf[baddr].cs & CS_GE) {
		space3270out(1);
		*obptr++ = ORDER_GE;
	    }
	    space3270out(1);
	    *obptr++ = ea_buf[baddr].ec;
	}
	INC_BA(baddr);
    } while (baddr != 0);

    space3270out(4);
    *obptr++ = ORDER_SBA;
    ENCODE_BADDR(obptr, cursor_addr);
    *obptr++ = ORDER_IC;
}

/*
 * Construct a 3270 command to reproduce the reply mode.
 * Returns a bool indicating if one is necessary.
 */
bool
ctlr_snap_modes(void)
{
    int i;

    if (!IN_3270 || reply_mode == SF_SRM_FIELD) {
	return false;
    }

    space3270out(6 + crm_nattr);
    *obptr++ = CMD_WSF;
    *obptr++ = 0x00;	/* implicit length */
    *obptr++ = 0x00;
    *obptr++ = SF_SET_REPLY_MODE;
    *obptr++ = 0x00;	/* partition 0 */
    *obptr++ = reply_mode;
    if (reply_mode == SF_SRM_CHAR) {
	for (i = 0; i < crm_nattr; i++) {
	    *obptr++ = crm_attr[i];
	}
    }
    return true;
}

/*
 * Construct a 3270 command to reproduce the current state of the display
 * in SSCP-LU mode.
 */
void
ctlr_snap_buffer_sscp_lu(void)
{
    int baddr = 0;

    /* Write out the screen contents once. */
    do {
	if (ea_buf[baddr].ec == 0xff) {
	    space3270out(1);
	    *obptr++ = 0xff;
	}
	space3270out(1);
	*obptr++ = ea_buf[baddr].ec;
	INC_BA(baddr);
    } while (baddr != 0);

    /* Write them out again, until we hit where the cursor is. */
    if (cursor_addr != baddr) {
	do {
	    if (ea_buf[baddr].ec == 0xff) {
		space3270out(1);
		*obptr++ = 0xff;
	    }
	    space3270out(1);
	    *obptr++ = ea_buf[baddr].ec;
	    INC_BA(baddr);
	} while (baddr != cursor_addr);
    }
}

/*
 * Process a 3270 Erase All Unprotected command.
 */
void
ctlr_erase_all_unprotected(void)
{
    int baddr, sbaddr;
    unsigned char fa;
    bool f;

    kybd_inhibit(false);

    ALL_CHANGED;
    if (formatted) {
	/* find first field attribute */
	baddr = 0;
	do {
	    if (ea_buf[baddr].fa) {
		break;
	    }
	    INC_BA(baddr);
	} while (baddr != 0);
	sbaddr = baddr;
	f = false;
	do {
	    fa = ea_buf[baddr].fa;
	    if (!FA_IS_PROTECTED(fa)) {
		mdt_clear(baddr);
		do {
		    INC_BA(baddr);
		    if (!f) {
			cursor_move(baddr);
			    f = true;
		    }
		    if (!ea_buf[baddr].fa) {
			ctlr_add(baddr, EBC_null, 0);
		    }
		} while (!ea_buf[baddr].fa);
	    } else {
		do {
		    INC_BA(baddr);
		} while (!ea_buf[baddr].fa);
	    }
	} while (baddr != sbaddr);
	if (!f) {
	    cursor_move(0);
	}
    } else {
	ctlr_clear(true);
    }
    aid = AID_NO;
    do_reset(false);
}

/*
 * Process a 3270 Write command.
 */
enum pds
ctlr_write(unsigned char buf[], size_t buflen, bool erase)
{
    unsigned char *cp;
    int baddr;
    unsigned char current_fa;
    bool last_cmd;
    bool last_zpt;
    bool wcc_keyboard_restore, wcc_sound_alarm;
    bool ra_ge;
    int i;
    unsigned char na;
    int any_fa;
    unsigned char efa_fg;
    unsigned char efa_bg;
    unsigned char efa_gr;
    unsigned char efa_cs;
    unsigned char efa_ic;
    const char *paren = "(";
    enum { NONE, ORDER, SBA, TEXT, NULLCH } previous = NONE;
    enum pds rv = PDS_OKAY_NO_OUTPUT;
    int fa_addr;
    bool add_dbcs;
    unsigned char add_c1, add_c2 = 0;
    enum dbcs_state d;
    enum dbcs_why why = DBCS_FIELD;
    bool aborted = false;
    char mb[16];
    bool insert_cursor = false;
    int ic_baddr = 0;

#define END_TEXT0	{ if (previous == TEXT) trace_ds("'"); }
#define END_TEXT(cmd)	{ END_TEXT0; trace_ds(" %s", cmd); }

/* XXX: Should there be a ctlr_add_cs call here? */
#define START_FIELD(fa) { \
			current_fa = fa; \
			ctlr_add_fa(buffer_addr, fa, 0); \
			ctlr_add_cs(buffer_addr, 0); \
			ctlr_add_fg(buffer_addr, 0); \
			ctlr_add_bg(buffer_addr, 0); \
			ctlr_add_gr(buffer_addr, 0); \
			ctlr_add_ic(buffer_addr, 0); \
			trace_ds("%s", see_attr(fa)); \
			formatted = true; \
		}

#define WRITE_ERROR "Host write error:\n"
#define TOO_SHORT "Record too short, "

    kybd_inhibit(false);

    if (buflen < 2) {
	/* Need flags at minimum. */
	popup_an_error(WRITE_ERROR TOO_SHORT "missing write flags");
	return PDS_BAD_ADDR;
    }

    default_fg = 0;
    default_bg = 0;
    default_gr = 0;
    default_cs = 0;
    default_ic = 0;
    trace_primed = true;
    buffer_addr = cursor_addr;
    if (WCC_RESET(buf[1])) {
	if (erase) {
	    reply_mode = SF_SRM_FIELD;
	}
	trace_ds("%sreset", paren);
	paren = ",";
    }
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
	baddr = 0;
	if (appres.modified_sel) {
	    ALL_CHANGED;
	}
	do {
	    if (ea_buf[baddr].fa) {
		mdt_clear(baddr);
	    }
	    INC_BA(baddr);
	} while (baddr != 0);
    }
    if (strcmp(paren, "(")) {
	trace_ds(")");
    }

    last_cmd = true;
    last_zpt = false;
    current_fa = get_field_attribute(buffer_addr);

#define END_WRITE END_TEXT("\n")
#define ABORT_WRITEx { \
    rv = PDS_BAD_ADDR; \
    aborted = true; \
    break; \
}
#define ABORT_WRITE(s) { \
    END_WRITE; \
    popup_an_error(WRITE_ERROR s); \
    ABORT_WRITEx; \
}

    for (cp = &buf[2]; !aborted && cp < (buf + buflen); cp++) {
	switch (*cp) {
	case ORDER_SF:	/* start field */
	    END_TEXT("StartField");
	    if (previous != SBA) {
		trace_ds("%s", rcba(buffer_addr));
	    }
	    previous = ORDER;
	    cp++;		/* skip field attribute */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing SF attributes");
	    }
	    START_FIELD(*cp);
	    ctlr_add_fg(buffer_addr, 0);
	    ctlr_add_bg(buffer_addr, 0);
	    INC_BA(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_SBA:	/* set buffer address */
	    END_TEXT("SetBufferAddress");
	    cp += 2;	/* skip buffer address */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing SBA address");
	    }
	    buffer_addr = DECODE_BADDR(*(cp-1), *cp);
	    previous = SBA;
	    trace_ds("%s", rcba(buffer_addr));
	    if (buffer_addr >= COLS * ROWS) {
		END_WRITE;
		popup_an_error(WRITE_ERROR "SBA address %d > maximum %d",
			buffer_addr, (COLS * ROWS) - 1);
		ABORT_WRITEx;
	    }
	    current_fa = get_field_attribute(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_IC:	/* insert cursor */
	    END_TEXT("InsertCursor");
	    if (previous != SBA) {
		trace_ds("%s", rcba(buffer_addr));
	    }
	    previous = ORDER;
	    insert_cursor = true;
	    ic_baddr = buffer_addr;
	    last_cmd = true;
	    last_zpt = false;
	    break;
    case ORDER_PT:	/* program tab */
	    END_TEXT("ProgramTab");
	    previous = ORDER;
	    /*
	     * If the buffer address is the field attribute of
	     * of an unprotected field, simply advance one
	     * position.
	     */
	    if (ea_buf[buffer_addr].fa &&
		!FA_IS_PROTECTED(ea_buf[buffer_addr].fa)) {
		INC_BA(buffer_addr);
		last_zpt = false;
		last_cmd = true;
		break;
	    }
	    /*
	     * Otherwise, advance to the first position of the
	     * next unprotected field.
	     */
	    baddr = next_unprotected(buffer_addr);
	    if (baddr < buffer_addr) {
		baddr = 0;
	    }
	    /*
	     * Null out the remainder of the current field -- even
	     * if protected -- if the PT doesn't follow a command
	     * or order, or (honestly) if the last order we saw was
	     * a null-filling PT that left the buffer address at 0.
	     * XXX: There's some funky DBCS rule here.
	     */
	    if (!last_cmd || last_zpt) {
		trace_ds("(nulling)");
		while ((buffer_addr != baddr) && (!ea_buf[buffer_addr].fa)) {
		    ctlr_add(buffer_addr, EBC_null, 0);
		    ctlr_add_cs(buffer_addr, 0);
		    ctlr_add_fg(buffer_addr, 0);
		    ctlr_add_bg(buffer_addr, 0);
		    ctlr_add_gr(buffer_addr, 0);
		    ctlr_add_ic(buffer_addr, 0);
		    INC_BA(buffer_addr);
		}
		if (baddr == 0) {
		    last_zpt = true;
		}
	    } else {
		last_zpt = false;
	    }
	    buffer_addr = baddr;
	    last_cmd = true;
	    break;
	case ORDER_RA:	/* repeat to address */
	    END_TEXT("RepeatToAddress");
	    cp += 2;	/* skip buffer address */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing RA address");
	    }
	    baddr = DECODE_BADDR(*(cp-1), *cp);
	    trace_ds("%s", rcba(baddr));
	    if (baddr >= COLS * ROWS) {
		END_WRITE;
		popup_an_error(WRITE_ERROR "RA address %d > maximum %d",
			baddr, (COLS * ROWS) - 1);
		ABORT_WRITEx;
	    }
	    cp++;		/* skip char to repeat */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing RA character");
	    }
	    add_dbcs = false;
	    ra_ge = false;
	    previous = ORDER;
	    if (dbcs) {
		d = ctlr_lookleft_state(buffer_addr, &why);
		if (d == DBCS_RIGHT) {
		    ABORT_WRITE("RA over right half of DBCS character");
		}
		if (default_cs == CS_DBCS || d == DBCS_LEFT) {
		    add_dbcs = true;
		}
	    }
	    if (add_dbcs) {
		if ((baddr - buffer_addr) % 2) {
		    ABORT_WRITE("DBCS RA with odd length");
		}
		add_c1 = *cp;
		cp++;
		if (cp >= buf + buflen) {
		    ABORT_WRITE(TOO_SHORT "missing second half of RA DBCS "
			    "character");
		}
		add_c2 = *cp;
		if (add_c1 == EBC_null) {
		    switch (add_c2) {
		    case EBC_null:
		    case EBC_nl:
		    case EBC_em:
		    case EBC_ff:
		    case EBC_cr:
		    case EBC_dup:
		    case EBC_fm:
			break;
		    default:
			END_WRITE;
			popup_an_error(WRITE_ERROR "Invalid DBCS RA "
				"control character X'%02X%02X'", add_c1,
				add_c2);
			ABORT_WRITEx;
		    }
		} else if (add_c1 < 0x40 || add_c1 > 0xfe || add_c2 < 0x40 ||
			add_c2 > 0xfe) {
		    END_WRITE;
		    popup_an_error(WRITE_ERROR "Invalid DBCS RA "
			    "character X'%02X%02X'", add_c1, add_c2);
		    ABORT_WRITEx;
		}
		ebcdic_to_multibyte((add_c1 << 8) | add_c2, mb, sizeof(mb));
		trace_ds("'%s'", mb);
	    } else {
		if (*cp == ORDER_GE) {
		    ra_ge = true;
		    trace_ds("GraphicEscape");
		    cp++;
		    if (cp >= buf + buflen) {
			ABORT_WRITE(TOO_SHORT "missing RA GE character");
		    }
		}
		add_c1 = *cp;
		if (add_c1) {
		    trace_ds("'");
		}
		trace_ds("%s", see_ebc(add_c1));
		if (add_c1) {
		    trace_ds("'");
		}
	    }
	    do {
		if (add_dbcs) {
		    ctlr_add(buffer_addr, add_c1, default_cs);
		} else {
		    if (ra_ge) {
			ctlr_add(buffer_addr, add_c1, CS_GE);
		    } else if (default_cs) {
			ctlr_add(buffer_addr, add_c1, default_cs);
		    } else {
			ctlr_add(buffer_addr, add_c1, 0);
		    }
		}
		ctlr_add_fg(buffer_addr, default_fg);
		ctlr_add_bg(buffer_addr, default_bg);
		ctlr_add_gr(buffer_addr, default_gr);
		ctlr_add_ic(buffer_addr, default_ic);
		INC_BA(buffer_addr);
		if (add_dbcs) {
		    ctlr_add(buffer_addr, add_c2, default_cs);
		    ctlr_add_fg(buffer_addr, default_fg);
		    ctlr_add_bg(buffer_addr, default_bg);
		    ctlr_add_gr(buffer_addr, default_gr);
		    ctlr_add_ic(buffer_addr, default_ic);
		    INC_BA(buffer_addr);
		}
	    } while (buffer_addr != baddr);
	    current_fa = get_field_attribute(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_EUA:	/* erase unprotected to address */
	    END_TEXT("EraseUnprotectedAll");
	    cp += 2;	/* skip buffer address */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing EUA address");
	    }
	    baddr = DECODE_BADDR(*(cp-1), *cp);
	    trace_ds("%s", rcba(baddr));
	    previous = ORDER;
	    if (baddr >= COLS * ROWS) {
		END_WRITE;
		popup_an_error(WRITE_ERROR "EUA address %d > maximum %d",
			baddr, (COLS * ROWS) - 1);
		ABORT_WRITEx;
	    }
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (d == DBCS_RIGHT) {
		ABORT_WRITE("EUA overwriting right half of DBCS character");
	    }
	    d = ctlr_lookleft_state(baddr, &why);
	    if (d == DBCS_LEFT) {
		ABORT_WRITE("EUA overwriting left half of DBCS character");
	    }
	    do {
		if (ea_buf[buffer_addr].fa) {
		    current_fa = ea_buf[buffer_addr].fa;
		} else if (!FA_IS_PROTECTED(current_fa)) {
		    ctlr_add(buffer_addr, EBC_null, CS_BASE);
		}
		INC_BA(buffer_addr);
	    } while (buffer_addr != baddr);
	    current_fa = get_field_attribute(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_GE:	/* graphic escape */
	    /* XXX: DBCS? */
	    END_TEXT("GraphicEscape ");
	    cp++;		/* skip char */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing GE character");
	    }
	    previous = ORDER;
	    if (*cp) {
		trace_ds("'");
	    }
	    trace_ds("%s", see_ebc(*cp));
	    if (*cp) {
		trace_ds("'");
	    }

	    ctlr_add(buffer_addr, *cp, CS_GE);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    current_fa = get_field_attribute(buffer_addr);
	    last_cmd = false;
	    last_zpt = false;
	    break;
	case ORDER_MF:	/* modify field */
	    END_TEXT("ModifyField");
	    if (previous != SBA) {
		trace_ds("%s", rcba(buffer_addr));
	    }
	    previous = ORDER;
	    cp++;
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing MF count");
	    }
	    na = *cp;
	    if (ea_buf[buffer_addr].fa) {
		for (i = 0; i < (int)na; i++) {
		    cp++;
		    if (cp + 1 >= buf + buflen) {
			ABORT_WRITE(TOO_SHORT "missing MF attribute");
		    }
		    if (*cp == XA_3270) {
			trace_ds(" 3270");
			cp++;
			ctlr_add_fa(buffer_addr, *cp, ea_buf[buffer_addr].cs);
			trace_ds("%s", see_attr(*cp));
		    } else if (*cp == XA_FOREGROUND) {
			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			cp++;
			if (mode3279) {
			    ctlr_add_fg(buffer_addr, *cp);
			}
		    } else if (*cp == XA_BACKGROUND) {
			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			cp++;
			if (mode3279) {
			    ctlr_add_bg(buffer_addr, *cp);
			}
		    } else if (*cp == XA_HIGHLIGHTING) {
			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			cp++;
			ctlr_add_gr(buffer_addr, *cp & 0x0f);
		    } else if (*cp == XA_CHARSET) {
			int cs = 0;

			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			cp++;
			if (*cp == 0xf1) {
			    cs = CS_APL;
			} else if (*cp == 0xf8) {
			    cs = CS_DBCS;
			}
			ctlr_add_cs(buffer_addr, cs);
		    } else if (*cp == XA_ALL) {
			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			cp++;
		    } else if (*cp == XA_INPUT_CONTROL) {
			trace_ds("%s", see_efa(*cp, *(cp + 1)));
			ctlr_add_ic(buffer_addr, (*(cp + 1) == 1));
			cp++;
		    } else {
			trace_ds("%s[unsupported]", see_efa(*cp, *(cp + 1)));
			cp++;
		    }
		}
		INC_BA(buffer_addr);
	    } else {
		cp += na * 2;
	    }
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_SFE:	/* start field extended */
	    END_TEXT("StartFieldExtended");
	    if (previous != SBA) {
		trace_ds("%s", rcba(buffer_addr));
	    }
	    previous = ORDER;
	    cp++;	/* skip order */
	    if (cp >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing SFE count");
	    }
	    na = *cp;
	    any_fa = 0;
	    efa_fg = 0;
	    efa_bg = 0;
	    efa_gr = 0;
	    efa_cs = 0;
	    efa_ic = 0;
	    for (i = 0; i < (int)na; i++) {
		cp++;
		if (cp + 1 >= buf + buflen) {
		    ABORT_WRITE(TOO_SHORT "missing SFE attribute");
		}
		if (*cp == XA_3270) {
		    trace_ds(" 3270");
		    cp++;
		    START_FIELD(*cp);
		    any_fa++;
		} else if (*cp == XA_FOREGROUND) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		    if (mode3279) {
			efa_fg = *cp;
		    }
		} else if (*cp == XA_BACKGROUND) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		    if (mode3279) {
			efa_bg = *cp;
		    }
		} else if (*cp == XA_HIGHLIGHTING) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		    efa_gr = *cp & 0x07;
		} else if (*cp == XA_CHARSET) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		    if (*cp == 0xf1) {
			efa_cs = CS_APL;
		    } else if (dbcs && (*cp == 0xf8)) {
			efa_cs = CS_DBCS;
		    } else {
			efa_cs = CS_BASE;
		    }
		} else if (*cp == XA_ALL) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    cp++;
		} else if (*cp == XA_INPUT_CONTROL) {
		    trace_ds("%s", see_efa(*cp, *(cp + 1)));
		    if (dbcs) {
			efa_ic = (*(cp + 1) == 1);
		    }
		    cp++;
		} else {
		    trace_ds("%s[unsupported]", see_efa(*cp, *(cp + 1)));
		    cp++;
		}
	    }
	    if (!any_fa) {
		START_FIELD(0);
	    }
	    ctlr_add_cs(buffer_addr, efa_cs);
	    ctlr_add_fg(buffer_addr, efa_fg);
	    ctlr_add_bg(buffer_addr, efa_bg);
	    ctlr_add_gr(buffer_addr, efa_gr);
	    ctlr_add_ic(buffer_addr, efa_ic);
	    INC_BA(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case ORDER_SA:	/* set attribute */
	    END_TEXT("SetAttribute");
	    previous = ORDER;
	    cp++;
	    if (cp + 1 >= buf + buflen) {
		ABORT_WRITE(TOO_SHORT "missing SA attribute");
	    }
	    if (*cp == XA_FOREGROUND)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		if (mode3279) {
		    default_fg = *(cp + 1);
		}
	    } else if (*cp == XA_BACKGROUND)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		if (mode3279) {
		    default_bg = *(cp + 1);
		}
	    } else if (*cp == XA_HIGHLIGHTING)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		default_gr = *(cp + 1) & 0x0f;
	    } else if (*cp == XA_ALL)  {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		default_fg = 0;
		default_bg = 0;
		default_gr = 0;
		default_cs = 0;
		default_ic = 0;
	    } else if (*cp == XA_CHARSET) {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		switch (*(cp + 1)) {
		case 0xf1:
		    default_cs = CS_APL;
		    break;
		case 0xf8:
		    default_cs = CS_DBCS;
		    break;
		default:
		    default_cs = CS_BASE;
		    break;
		}
	    } else if (*cp == XA_INPUT_CONTROL) {
		trace_ds("%s", see_efa(*cp, *(cp + 1)));
		if (*(cp + 1) == 1) {
		    default_ic = 1;
		} else {
		    default_ic = 0;
		}
	    } else {
		trace_ds("%s[unsupported]", see_efa(*cp, *(cp + 1)));
	    }
	    cp++;
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case FCORDER_SUB:	/* format control orders */
	case FCORDER_DUP:
	case FCORDER_FM:
	case FCORDER_FF:
	case FCORDER_CR:
	case FCORDER_NL:
	case FCORDER_EM:
	case FCORDER_LF:
	case FCORDER_EO:
	    END_TEXT(see_ebc(*cp));
	    previous = ORDER;
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (default_cs == CS_DBCS || d != DBCS_NONE) {
		ABORT_WRITE("Invalid format control order in DBCS field");
	    }
	    ctlr_add(buffer_addr, *cp, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case FCORDER_SO:
	    /* Look left for errors. */
	    END_TEXT(see_ebc(*cp));
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (d == DBCS_RIGHT) {
		ABORT_WRITE("SO overwriting right half of DBCS character");
	    }
	    if (d != DBCS_NONE && why == DBCS_FIELD) {
		ABORT_WRITE("SO in DBCS field");
	    }
	    if (d != DBCS_NONE && why == DBCS_SUBFIELD) {
		ABORT_WRITE("Double SO");
	    }
	    /* All is well. */
	    previous = ORDER;
	    ctlr_add(buffer_addr, *cp, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case FCORDER_SI:
	    /* Look left for errors. */
	    END_TEXT(see_ebc(*cp));
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (d == DBCS_RIGHT) {
		ABORT_WRITE("SI overwriting right half of DBCS character");
	    }
	    if (d != DBCS_NONE && why == DBCS_FIELD) {
		ABORT_WRITE("SI in DBCS field");
	    }
	    fa_addr = find_field_attribute(buffer_addr);
	    baddr = buffer_addr;
	    DEC_BA(baddr);
	    while (!aborted &&
		   ((fa_addr >= 0 && baddr != fa_addr) ||
		    (fa_addr < 0 && baddr != ROWS*COLS - 1))) {
		if (ea_buf[baddr].ec == FCORDER_SI) {
		    ABORT_WRITE("Double SI");
		}
		if (ea_buf[baddr].ec == FCORDER_SO) {
		    break;
		}
		DEC_BA(baddr);
	    }
	    if (aborted) {
		break;
	    }
	    if (ea_buf[baddr].ec != FCORDER_SO) {
		ABORT_WRITE("SI without SO");
	    }
	    /* All is well. */
	    previous = ORDER;
	    ctlr_add(buffer_addr, *cp, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    last_cmd = true;
	    last_zpt = false;
	    break;
	case FCORDER_NULL:	/* NULL or DBCS control char */
	    add_dbcs = false;
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (d == DBCS_RIGHT) {
		ABORT_WRITE("NULL overwriting right half of DBCS character");
	    }
	    if (d != DBCS_NONE || default_cs == CS_DBCS) {
		add_c1 = EBC_null;
		cp++;
		if (cp >= buf + buflen) {
		    ABORT_WRITE("Missing second half of DBCS character");
		}
		add_c2 = *cp;
		switch (add_c2) {
		case EBC_null:
		case EBC_nl:
		case EBC_em:
		case EBC_ff:
		case EBC_cr:
		case EBC_dup:
		case EBC_fm:
		    /* DBCS control code */
		    END_TEXT(see_ebc(add_c2));
		    add_dbcs = true;
		    break;
		case ORDER_SF:
		case ORDER_SFE:
		    /* Dead position */
		    END_TEXT("DeadNULL");
		    cp--;
		    break;
		default:
		    END_WRITE;
		    popup_an_error(WRITE_ERROR "Invalid DBCS control "
			    "character X'%02X%02X'", add_c1, add_c2);
		    ABORT_WRITEx;
		    break;
		}
		if (aborted) {
		    break;
		}
	    } else {
		END_TEXT("NULL");
		add_c1 = *cp;
	    }
	    previous = NULLCH;
	    ctlr_add(buffer_addr, add_c1, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    if (add_dbcs) {
		ctlr_add(buffer_addr, add_c2, default_cs);
		ctlr_add_fg(buffer_addr, default_fg);
		ctlr_add_bg(buffer_addr, default_bg);
		ctlr_add_gr(buffer_addr, default_gr);
		ctlr_add_ic(buffer_addr, default_ic);
		INC_BA(buffer_addr);
	    }
	    last_cmd = false;
	    last_zpt = false;
	    break;
	default:	/* enter character */
	    if (*cp <= 0x3F) {
		END_TEXT("UnsupportedOrder");
		trace_ds("(%02X)", *cp);
		previous = ORDER;
		last_cmd = true;
		last_zpt = false;
		break;
	    }
	    if (previous != TEXT) {
		trace_ds(" '");
	    }
	    previous = TEXT;
	    add_dbcs = false;
	    d = ctlr_lookleft_state(buffer_addr, &why);
	    if (d == DBCS_RIGHT) {
		ABORT_WRITE("Overwriting right half of DBCS character");
	    }
	    if (d != DBCS_NONE || default_cs == CS_DBCS) {
		add_c1 = *cp;
		cp++;
		if (cp >= buf + buflen) {
		    ABORT_WRITE("Missing second half of DBCS character");
		}
		add_c2 = *cp;
		if (add_c1 < 0x40 || add_c1 > 0xfe ||
		    add_c2 < 0x40 || add_c2 > 0xfe) {
		    END_WRITE;
		    popup_an_error(WRITE_ERROR "Invalid DBCS character "
			    "X'%02X%02X'", add_c1, add_c2);
		    ABORT_WRITEx;
		}
		add_dbcs = true;
		ebcdic_to_multibyte((add_c1 << 8) | add_c2, mb, sizeof(mb));
		trace_ds("%s", mb);
	    } else {
		add_c1 = *cp;
		trace_ds("%s", see_ebc(*cp));
	    }
	    ctlr_add(buffer_addr, add_c1, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    if (add_dbcs) {
		ctlr_add(buffer_addr, add_c2, default_cs);
		ctlr_add_fg(buffer_addr, default_fg);
		ctlr_add_bg(buffer_addr, default_bg);
		ctlr_add_gr(buffer_addr, default_gr);
		ctlr_add_ic(buffer_addr, default_ic);
		INC_BA(buffer_addr);
	    }
	    last_cmd = false;
	    last_zpt = false;
	    break;
	}
    }
    set_formatted();
    END_TEXT0;
    trace_ds("\n");
    if (insert_cursor) {
	cursor_move(ic_baddr);
    }
    kybdlock_clr(KL_AWAITING_FIRST, "ctlr_write");
    if (wcc_keyboard_restore) {
	aid = AID_NO;
	do_reset(false);
    } else if (kybdlock & KL_OIA_TWAIT) {
	kybdlock_clr(KL_OIA_TWAIT, "ctlr_write");
	vstatus_syswait();
    }
    if (wcc_sound_alarm) {
	ring_bell();
    }
    if (wcc_keyboard_restore) {
	ticking_stop(&net_last_recv_ts);
    }

    /* Set up the DBCS state. */
    if (ctlr_dbcs_postprocess() < 0 && rv == PDS_OKAY_NO_OUTPUT) {
	rv = PDS_BAD_ADDR;
    }

    trace_primed = false;

    ps_process();

    /* Let a blocked task go. */
    task_host_output();

    /* Tell 'em what happened. */
    return rv;
}

#undef START_FIELDx
#undef START_FIELD0
#undef START_FIELD
#undef END_TEXT0
#undef END_TEXT
#undef WRITE_ERROR
#undef ABORT_WRITEx
#undef ABORT_WRITE

/*
 * Write SSCP-LU data, which is quite a bit dumber than regular 3270
 * output.
 */
void
ctlr_write_sscp_lu(unsigned char buf[], size_t buflen)
{
    size_t i;
    unsigned char *cp = buf;
    int s_row;
    unsigned char c;
    int baddr;
    int text = false;

    /*
     * The 3174 Functionl Description says that anything but NL, NULL, FM,
     * or DUP is to be displayed as a graphic.  However, to deal with
     * badly-behaved hosts, we filter out SF, IC and SBA sequences, and
     * we display other control codes as spaces.
     */

    trace_ds("SSCP-LU data\n<");
    for (i = 0; i < buflen; cp++, i++) {
	switch (*cp) {
	case FCORDER_NL:
	    /*
	     * Insert NULLs to the end of the line and advance to
	     * the beginning of the next line.
	     */
	    if (text) {
		trace_ds("'");
		text = false;
	    }
	    trace_ds(" NL");
	    s_row = buffer_addr / COLS;
	    while ((buffer_addr / COLS) == s_row) {
		ctlr_add(buffer_addr, EBC_null, default_cs);
		ctlr_add_fg(buffer_addr, default_fg);
		ctlr_add_bg(buffer_addr, default_bg);
		ctlr_add_gr(buffer_addr, default_gr);
		ctlr_add_ic(buffer_addr, default_ic);
		INC_BA(buffer_addr);
	    }
	    if (buffer_addr == 0) {
		ctlr_scroll(0, 0);
		buffer_addr = (ROWS - 1) * COLS;
	    }
	    break;

	case ORDER_SF:
	    /* Some hosts forget they're talking SSCP-LU. */
	    cp++;
	    i++;
	    if (text) {
		trace_ds("'");
		text = false;
	    }
	    if (cp >= buf + buflen) {
		trace_ds(" SF%s [translated to space]\n", rcba(buffer_addr));
	    } else {
		trace_ds(" SF%s %s [translated to space]\n", rcba(buffer_addr),
			see_attr(*cp));
	    }
	    ctlr_add(buffer_addr, EBC_space, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    if (buffer_addr == 0) {
		ctlr_scroll(0, 0);
		buffer_addr = (ROWS - 1) * COLS;
	    }
	    break;
	case ORDER_IC:
	    if (text) {
		trace_ds("'");
		text = false;
	    }
	    trace_ds(" IC%s [ignored]\n", rcba(buffer_addr));
	    break;
	case ORDER_SBA:
	    if (cp + 2 >= buf + buflen) {
		trace_ds(" SBA [ignored]\n");
	    } else {
		baddr = DECODE_BADDR(*(cp+1), *(cp+2));
		trace_ds(" SBA%s [ignored]\n", rcba(baddr));
	    }
	    cp += 2;
	    i += 2;
	    break;

	case ORDER_GE:
	    cp++;
	    if (++i >= buflen) {
		if (text) {
		    trace_ds("'");
		    text = false;
		}
		trace_ds(" GE");
		break;
	    }
	    if (*cp <= 0x40) {
		c = EBC_space;
	    } else {
		c = *cp;
	    }
	    if (text) {
		trace_ds("'");
		text = false;
	    }
	    trace_ds(" GE '%s'", see_ebc(c));
	    ctlr_add(buffer_addr, c, CS_GE);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    if (buffer_addr == 0) {
		ctlr_scroll(0, 0);
		buffer_addr = (ROWS - 1) * COLS;
		}
	    break;

	default:
	    if (!text) {
		trace_ds(" '");
		text = true;
	    }
	    trace_ds("%s", see_ebc(*cp));
	    ctlr_add(buffer_addr, *cp, default_cs);
	    ctlr_add_fg(buffer_addr, default_fg);
	    ctlr_add_bg(buffer_addr, default_bg);
	    ctlr_add_gr(buffer_addr, default_gr);
	    ctlr_add_ic(buffer_addr, default_ic);
	    INC_BA(buffer_addr);
	    if (buffer_addr == 0) {
		ctlr_scroll(0, 0);
		buffer_addr = (ROWS - 1) * COLS;
	    }
	    break;
	}
    }
    if (text) {
	trace_ds("'");
    }
    trace_ds("\n");
    cursor_move(buffer_addr);
    sscp_start = buffer_addr;

    /* Unlock the keyboard. */
    aid = AID_NO;
    do_reset(false);

    /* Let a blocked task go. */
    task_host_output();
}

void
ctlr_sscp_up(void)
{
    if (sscp_start > COLS) {
	sscp_start -= COLS;
    }
}

/*
 * Determine the DBCS state of a buffer location strictly by looking left.
 * Used only to validate write operations.
 * Returns only DBCS_LEFT, DBCS_RIGHT or DBCS_NONE.
 * Also returns whether the location is part of a DBCS field (SFE with the
 *  DBCS character set), DBCS subfield (to the right of an SO within a non-DBCS
 *  field), or DBCS attribute (has the DBCS character set extended attribute
 *  within a non-DBCS field).
 *
 * This function should be used only to determine the legality of adding a
 * DBCS or SBCS character at baddr.
 */
enum dbcs_state
ctlr_lookleft_state(int baddr, enum dbcs_why *why)
{
    int faddr;
    int fdist;
    int xaddr;
    bool si = false;
#define	AT_END(f, b) \
    (((f) < 0 && (b) == ROWS*COLS - 1) || \
     ((f) >= 0 && (b) == (f)))

     /* If we're not in DBCS state, everything is DBCS_NONE. */
     if (!dbcs) {
	return DBCS_NONE;
     }

    /* Find the field attribute, if any. */
    faddr = find_field_attribute(baddr);

    /*
     * First in precedence is a DBCS field.
     * DBCS SA and SO/SI inside a DBCS field are errors, but are considered
     * defective DBCS characters.
     */
    if (ea_buf[faddr].cs == CS_DBCS) {
	*why = DBCS_FIELD;
	fdist = (baddr + ROWS*COLS) - faddr;
	return (fdist % 2)? DBCS_LEFT: DBCS_RIGHT;
    }

    /*
     * The DBCS attribute takes precedence next.
     * SO and SI can appear within such a region, but they are single-byte
     * characters which effectively split it.
     */
    if (ea_buf[baddr].cs == CS_DBCS) {
	if (ea_buf[baddr].ec == EBC_so || ea_buf[baddr].ec == EBC_si) {
	    return DBCS_NONE;
	}
	xaddr = baddr;
	while (!AT_END(faddr, xaddr) &&
	       ea_buf[xaddr].cs == CS_DBCS &&
	       ea_buf[xaddr].ec != EBC_so &&
	       ea_buf[xaddr].ec != EBC_si) {
	    DEC_BA(xaddr);
	}
	*why = DBCS_ATTRIBUTE;
	fdist = (baddr + ROWS*COLS) - xaddr;
	return (fdist % 2)? DBCS_LEFT: DBCS_RIGHT;
    }

    /*
     * Finally, look for a SO not followed by an SI.
     */
    xaddr = baddr;
    DEC_BA(xaddr);
    while (!AT_END(faddr, xaddr)) {
	if (ea_buf[xaddr].ec == EBC_si) {
	    si = true;
	} else if (ea_buf[xaddr].ec == EBC_so) {
	    if (si) {
		si = false;
	    } else {
		*why = DBCS_SUBFIELD;
		fdist = (baddr + ROWS*COLS) - xaddr;
		return (fdist % 2)? DBCS_LEFT: DBCS_RIGHT;
	    }
	}
	DEC_BA(xaddr);
    }

    /* Nada. */
    return DBCS_NONE;
}

static bool
valid_dbcs_char(unsigned char c1, unsigned char c2)
{
    if (c1 >= 0x40 && c1 < 0xff && c2 >= 0x40 && c2 < 0xff) {
	return true;
    }
    if (c1 != 0x00 || c2 < 0x40 || c2 >= 0xff) {
	return false;
    }
    switch (c2) {
    case EBC_null:
    case EBC_nl:
    case EBC_em:
    case EBC_ff:
    case EBC_cr:
    case EBC_dup:
    case EBC_fm:
	return true;
    default:
	return false;
    }
}

/*
 * Post-process DBCS state in the buffer.
 * This has two purposes:
 *
 * - Required post-processing validation, per the data stream spec, which can
 *   cause the write operation to be rejected.
 * - Setting up the value of the all the db fields in ea_buf.
 *
 * This function is called at the end of every 3270 write operation, and also
 * after each batch of NVT write operations.  It could also be called after
 * significant keyboard operations, but that might be too expensive.
 *
 * Returns 0 for success, -1 for failure.
 */
int
ctlr_dbcs_postprocess(void)
{
    int baddr;		/* current buffer address */
    int faddr0;		/* address of first field attribute */
    int faddr;		/* address of current field attribute */
    int last_baddr;	/* last buffer address to search */
    int pbaddr = -1;	/* previous buffer address */
    int dbaddr = -1;	/* first data position of current DBCS (sub-)
		           field */
    bool so = false, si = false;
    bool dbcs_field = false;
    int rc = 0;

    /* If we're not in DBCS mode, do nothing. */
    if (!dbcs) {
	return 0;
    }

    /*
     * Find the field attribute for location 0.  If unformatted, it's the
     * dummy at -1.  Also compute the starting and ending points for the
     * scan: the first location after that field attribute.
     */
    faddr0 = find_field_attribute(0);
    baddr = faddr0;
    INC_BA(baddr);
    if (faddr0 < 0) {
	last_baddr = 0;
    } else {
	last_baddr = faddr0;
    }
    faddr = faddr0;
    dbcs_field = (ea_buf[faddr].cs & CS_MASK) == CS_DBCS;

    do {
	if (ea_buf[baddr].fa) {
	    faddr = baddr;
	    ea_buf[faddr].db = DBCS_NONE;
	    dbcs_field = (ea_buf[faddr].cs & CS_MASK) == CS_DBCS;
	    if (dbcs_field) {
		dbaddr = baddr;
		INC_BA(dbaddr);
	    } else {
		dbaddr = -1;
	    }
	    /*
	     * An SI followed by a field attribute shouldn't be
	     * displayed with a wide cursor.
	     */
	    if (pbaddr >= 0 && ea_buf[pbaddr].db == DBCS_SI) {
		ea_buf[pbaddr].db = DBCS_NONE;
	    }
	} else {
	    switch (ea_buf[baddr].ec) {
	    case EBC_so:
		/* Two SO's or SO in DBCS field are invalid. */
		if (so || dbcs_field) {
		    trace_ds("DBCS postprocess: invalid SO found at %s\n",
			    rcba(baddr));
		    rc = -1;
		} else {
		    dbaddr = baddr;
		    INC_BA(dbaddr);
		}
		ea_buf[baddr].db = DBCS_NONE;
		so = true;
		si = false;
		break;
	    case EBC_si:
		/* Two SI's or SI in DBCS field are invalid. */
		if (si || dbcs_field) {
		    trace_ds("Postprocess: Invalid SO found at %s\n",
			    rcba(baddr));
		    rc = -1;
		    ea_buf[baddr].db = DBCS_NONE;
		} else {
		    ea_buf[baddr].db = DBCS_SI;
		}
		dbaddr = -1;
		si = true;
		so = false;
		break;
	    default:
		/* Non-base CS in DBCS subfield is invalid. */
		if (so && ea_buf[baddr].cs != CS_BASE) {
		    trace_ds("DBCS postprocess: invalid character set found "
			    "at %s\n", rcba(baddr));
		    rc = -1;
		    ea_buf[baddr].cs = CS_BASE;
		}
		if ((ea_buf[baddr].cs & CS_MASK) == CS_DBCS) {
		    /* Beginning or continuation of an SA DBCS subfield. */
		    if (dbaddr < 0) {
			dbaddr = baddr;
		    }
		} else if (!so && !dbcs_field) {
		    /* End of SA DBCS subfield. */
		    dbaddr = -1;
		}
		if (dbaddr >= 0) {
		    /* Turn invalid characters into spaces, silently. */
		    if ((baddr + ROWS*COLS - dbaddr) % 2) {
			if (!valid_dbcs_char( ea_buf[pbaddr].ec,
				    ea_buf[baddr].ec)) {
			    ea_buf[pbaddr].ec = EBC_space;
			    ea_buf[baddr].ec = EBC_space;
			}
			MAKE_RIGHT(baddr);
		    } else {
			MAKE_LEFT(baddr);
		    }
		} else {
		    ea_buf[baddr].db = DBCS_NONE;
		}
		break;
	    }
	}

	/*
	 * Check for dead positions.
	 * Turn them into NULLs, silently.
	 */
	if (pbaddr >= 0 &&
		IS_LEFT(ea_buf[pbaddr].db) &&
		!IS_RIGHT(ea_buf[baddr].db) &&
		ea_buf[pbaddr].db != DBCS_DEAD) {
	    if (!ea_buf[baddr].fa) {
		trace_ds("DBCS postprocess: dead position at %s\n",
			rcba(pbaddr));
		rc = -1;
	    }
	    ea_buf[pbaddr].ec = EBC_null;
	    ea_buf[pbaddr].db = DBCS_DEAD;
	}

	/* Check for SB's, which follow SIs. */
	if (pbaddr >= 0 && ea_buf[pbaddr].db == DBCS_SI) {
	    ea_buf[baddr].db = DBCS_SB;
	}

	/* Save this position as the previous and increment. */
	pbaddr = baddr;
	INC_BA(baddr);

    } while (baddr != last_baddr);

    return rc;
}

/*
 * Process pending input.
 */
void
ps_process(void)
{
    while (run_ta())
	;

    /* Process file transfers. */
    if (ft_state != FT_NONE &&		/* transfer in progress */
	    formatted &&		/* screen is formatted */
	    !(kybdlock & ~KL_FT)) {	/* keyboard not locked */
	ft_cut_data();
    }
}

/*
 * Tell me if there is any data on the screen.
 */
bool
ctlr_any_data(void)
{
    int i;

    if (ea_buf == NULL) {
	return false;
    }

    for (i = 0; i < ROWS*COLS; i++) {
	if (!IsBlank(ea_buf[i].ec) ||
		(ea_buf[i].ucs4 != 0 &&
		 ea_buf[i].ucs4 != ' ' &&
		 ea_buf[i].ucs4 != 0x3000)) {
	    return true;
	}
    }
    return false;
}

/*
 * Clear the text (non-status) portion of the display.  Also resets the cursor
 * and buffer addresses and extended attributes.
 */
void
ctlr_clear(bool can_snap)
{
    /* Snap any data that is about to be lost into the trace file. */
    if (ctlr_any_data()) {
	if (can_snap && !trace_skipping && toggled(SCREEN_TRACE)) {
	    trace_screen(true);
	}
	scroll_save(maxROWS);
    }
    trace_skipping = false;

    /* Clear the screen. */
    memset((char *)ea_buf, 0, ROWS*COLS*sizeof(struct ea));
    ALL_CHANGED;
    cursor_move(0);
    buffer_addr = 0;
    unselect(0, ROWS*COLS);
    formatted = false;
    default_fg = 0;
    default_bg = 0;
    default_gr = 0;
    default_ic = 0;
    sscp_start = 0;
}

/*
 * Fill the screen buffer with blanks.
 */
static void
ctlr_blanks(void)
{
    int baddr;

    for (baddr = 0; baddr < maxROWS*maxCOLS; baddr++) {
	ea_buf[baddr].ec = EBC_space;
    }
    ALL_CHANGED;
    cursor_move(0);
    buffer_addr = 0;
    unselect(0, ROWS*COLS);
    formatted = false;
}

/*
 * Change a character in the 3270 buffer, EBCDIC mode.
 * Removes any field attribute defined at that location.
 */
void
ctlr_add(int baddr, unsigned char c, unsigned char cs)
{
    unsigned char oc = 0;

    if (ea_buf[baddr].fa ||
	ea_buf[baddr].ucs4 ||
	((oc = ea_buf[baddr].ec) != c || ea_buf[baddr].cs != cs)) {
	if (trace_primed && !IsBlank(oc)) {
	    if (toggled(SCREEN_TRACE)) {
		trace_screen(false);
	    }
	    scroll_save(maxROWS);
	    trace_primed = false;
	}
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].ec = c;
	ea_buf[baddr].cs = cs;
	ea_buf[baddr].fa = 0;
	ea_buf[baddr].ucs4 = 0;
    }
}

/*
 * Change a character in the 3270 buffer, NVT mode.
 * Removes any field attribute defined at that location.
 */
void
ctlr_add_nvt(int baddr, ucs4_t ucs4, unsigned char cs)
{
    if (ea_buf[baddr].fa ||
	ea_buf[baddr].ucs4 != ucs4 ||
	ea_buf[baddr].ec != 0 ||
	ea_buf[baddr].cs != cs) {
	if (trace_primed && !IsBlank(ea_buf[baddr].ec)) {
	    if (toggled(SCREEN_TRACE)) {
		trace_screen(false);
	    }
	    scroll_save(maxROWS);
	    trace_primed = false;
	}
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].ucs4 = ucs4;
	ea_buf[baddr].ec = 0;
	ea_buf[baddr].cs = cs;
	ea_buf[baddr].fa = 0;

	if (cs == CS_DBCS) {
	    ea_buf[baddr].db = ucs4 == ' '? DBCS_RIGHT: DBCS_LEFT;
	}
    }
}

/* 
 * Set a field attribute in the 3270 buffer.
 */
void
ctlr_add_fa(int baddr, unsigned char fa, unsigned char cs)
{
    /* Put a null in the display buffer. */
    ctlr_add(baddr, EBC_null, cs);

    /*
     * Store the new attribute, setting the 'printable' bits so that the
     * value will be non-zero.
     */
    ea_buf[baddr].fa = FA_PRINTABLE | (fa & FA_MASK);
}

/* 
 * Change the character set for a field in the 3270 buffer.
 */
void
ctlr_add_cs(int baddr, unsigned char cs)
{
    if (ea_buf[baddr].cs != cs) {
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].cs = cs;
    }
}

/*
 * Change the graphic rendition of a character in the 3270 buffer.
 */
void
ctlr_add_gr(int baddr, unsigned char gr)
{
    if (ea_buf[baddr].gr != gr) {
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].gr = gr;
	if (gr & GR_BLINK) {
	    blink_start();
	}
    }
}

/*
 * Change the foreground color for a character in the 3270 buffer.
 */
void
ctlr_add_fg(int baddr, unsigned char color)
{
    if (!mode3279) {
	return;
    }
    if ((color & 0xf0) != 0xf0) {
	color = 0;
    }
    if (ea_buf[baddr].fg != color) {
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].fg = color;
    }
}

/*
 * Change the background color for a character in the 3270 buffer.
 */
void
ctlr_add_bg(int baddr, unsigned char color)
{
    if (!mode3279) {
	return;
    }
    if ((color & 0xf0) != 0xf0) {
	color = 0;
    }
    if (ea_buf[baddr].bg != color) {
	if (screen_selected(baddr)) {
	    unselect(baddr, 1);
	}
	ONE_CHANGED(baddr);
	ea_buf[baddr].bg = color;
    }
}

/*
 * Change the input control bit for a character in the 3270 buffer.
 */
static void
ctlr_add_ic(int baddr, unsigned char ic)
{
	ea_buf[baddr].ic = ic;
}

/*
 * Wrapping bersion of ctlr_bcopy.
 */
void
ctlr_wrapping_memmove(int baddr_to, int baddr_from, int count)
{
    /*
     * The 'to' region, the 'from' region, or both can wrap the screen,
     * and can overlap each other.  memmove() is smart enough to deal with
     * overlaps, but not across a screen wrap.
     *
     * It's faster to figure out if none of this is true, then do a slow
     * location-at-a-time version only if it happens.
     */
    if (baddr_from + count <= ROWS*COLS && baddr_to + count <= ROWS*COLS) {
	ctlr_bcopy(baddr_from, baddr_to, count, true);
    } else {
	int i, from, to;

	for (i = 0; i < count; i++) {
	    if (baddr_to > baddr_from) {
		/* Shifting right, move left. */
		to = (baddr_to + count - 1 - i) % ROWS*COLS;
		from = (baddr_from + count - 1 - i) % ROWS*COLS;
	    } else {
		/* Shifting left, move right. */
		to = (baddr_to + i) % ROWS*COLS;
		from = (baddr_from + i) % ROWS*COLS;
	    }
	    ctlr_bcopy(from, to, 1, true);
	}
    }
}

/*
 * Copy a block of characters in the 3270 buffer, optionally including all of
 * the extended attributes.  (The character set, which is actually kept in the
 * extended attributes, is considered part of the characters here.)
 */
void
ctlr_bcopy(int baddr_from, int baddr_to, int count, int move_ea)
{
    /* Move the characters. */
    if (memcmp((char *) &ea_buf[baddr_from], (char *) &ea_buf[baddr_to],
		count * sizeof(struct ea))) {
	memmove(&ea_buf[baddr_to], &ea_buf[baddr_from],
		count * sizeof(struct ea));
	REGION_CHANGED(baddr_to, baddr_to + count);
	/*
	 * For the time being, if any selected text shifts around on
	 * the screen, unhighlight it.  Eventually there should be
	 * logic for preserving the highlight if the *all* of the
	 * selected text moves.
	 */
	if (area_is_selected(baddr_to, count)) {
	    unselect(baddr_to, count);
	}
    }
    /* XXX: What about move_ea? */
}

/*
 * Erase a region of the 3270 buffer, optionally clearing extended attributes
 * as well.
 */
void
ctlr_aclear(int baddr, int count, int clear_ea)
{
    if (memcmp((char *)&ea_buf[baddr], (char *)zero_buf,
		count * sizeof(struct ea))) {
	memset((char *) &ea_buf[baddr], 0, count * sizeof(struct ea));
	REGION_CHANGED(baddr, baddr + count);
	if (area_is_selected(baddr, count)) {
	    unselect(baddr, count);
	}
    }
    /* XXX: What about clear_ea? */
}

/*
 * Scroll the screen 1 row.
 *
 * This could be accomplished with ctlr_bcopy() and ctlr_aclear(), but this
 * operation is common enough to warrant a separate path.
 */
void
ctlr_scroll(unsigned char fg, unsigned char bg)
{
    int qty = (ROWS - 1) * COLS;
    bool obscured;
    int i;

    /* Make sure nothing is selected. (later this can be fixed) */
    unselect(0, ROWS*COLS);

    /* Synchronize pending changes prior to this. */
    obscured = screen_obscured();
    if (!obscured && screen_changed) {
	screen_disp(false);
    }

    /* Move ea_buf. */
    memmove(&ea_buf[0], &ea_buf[COLS], qty * sizeof(struct ea));

    /* Clear the last line. */
    memset((char *) &ea_buf[qty], 0, COLS * sizeof(struct ea));
    if ((fg & 0xf0) != 0xf0) {
	fg = 0;
    }
    if ((bg & 0xf0) != 0xf0) {
	bg = 0;
    }
    for (i = 0; i < COLS; i++) {
	ea_buf[qty + i].fg = fg;
	ea_buf[qty + i].bg = bg;
    }

    /* Update the screen. */
    if (obscured) {
	ALL_CHANGED;
    } else {
	screen_scroll(fg, bg);
    }
}

/*
 * Note that a particular region of the screen has changed.
 */
void
ctlr_changed(int bstart, int bend)
{
    REGION_CHANGED(bstart, bend);
}


#if defined(CHECK_AEA_BUF) /*[*/
/*
 * Compute a simple checksum.
 */
static unsigned long
csum(struct ea *ea)
{
    size_t i;
    unsigned long sum = 0;
    unsigned char *c = (unsigned char *)(void *)ea;

    for (i = 0; i < (ROWS * COLS) * sizeof(struct ea); i++) {
	sum += c[i];
    }
    return sum;
}
#endif /*]*/

/*
 * Swap the regular and alternate screen buffers
 */
void
ctlr_altbuffer(bool alt)
{
    if (alt != is_altbuffer) {
	struct ea *etmp;
#if defined(CHECK_AEA_BUF) /*[*/
	unsigned long stmp;
#endif /*]*/

	etmp = ea_buf;
	ea_buf = aea_buf;
	aea_buf = etmp;

#if defined(CHECK_AEA_BUF) /*[*/
	stmp = ea_sum;
	ea_sum = aea_sum;
	aea_sum = stmp;

	if (ea_sum != 0) {
	    assert(csum(ea_buf) == ea_sum);
	}
	aea_sum = csum(aea_buf);
#endif /*]*/

	is_altbuffer = alt;
	ALL_CHANGED;
	unselect(0, ROWS*COLS);

	/*
	 * There may be blinkers on the alternate screen; schedule one
	 * iteration just in case.
	 */
	blink_start();
    }
}

/*
 * Set or clear the MDT on an attribute
 */
void
mdt_set(int baddr)
{
    int faddr;

    faddr = find_field_attribute(baddr);
    if (faddr >= 0 && !(ea_buf[faddr].fa & FA_MODIFY)) {
	ea_buf[faddr].fa |= FA_MODIFY;
	if (appres.modified_sel) {
	    ALL_CHANGED;
	}
    }
}

void
mdt_clear(int baddr)
{
    int faddr;

    faddr = find_field_attribute(baddr);
    if (faddr >= 0 && (ea_buf[faddr].fa & FA_MODIFY)) {
	ea_buf[faddr].fa &= ~FA_MODIFY;
	if (appres.modified_sel) {
	    ALL_CHANGED;
	}
    }
}

/*
 * Support for screen-size swapping for scrolling
 */
void
ctlr_shrink(void)
{
    int baddr;

    for (baddr = 0; baddr < ROWS*COLS; baddr++) {
	if (!ea_buf[baddr].fa) {
	    ea_buf[baddr].ec = visible_control? EBC_space : EBC_null;
	}
    }
    ALL_CHANGED;
    screen_disp(false);
}

/*
 * DBCS state query.
 * Returns:
 *  DBCS_NONE:	Buffer position is SBCS.
 *  DBCS_LEFT:	Buffer position is left half of a DBCS character.
 *  DBCS_RIGHT:	Buffer position is right half of a DBCS character.
 *  DBCS_SI:    Buffer position is the SI terminating a DBCS subfield (treated
 *		as DBCS_LEFT for wide cursor tests)
 *  DBCS_SB:	Buffer position is an SBCS character after an SI (treated as
 *		DBCS_RIGHT for wide cursor tests)
 *
 * Takes line-wrapping into account, which probably isn't done all that well.
 */
enum dbcs_state
ctlr_dbcs_state_ea(int baddr, struct ea *ea)
{
    return (ea[baddr].ucs4 || dbcs)? ea[baddr].db: DBCS_NONE;
}

enum dbcs_state
ctlr_dbcs_state(int baddr)
{
    return ctlr_dbcs_state_ea(baddr, ea_buf);
}

/*
 * Transaction timing.  The time between sending an interrupt (PF, PA, Enter,
 * Clear) and the host unlocking the keyboard is indicated on the status line
 * to an accuracy of 0.1 seconds.  If we don't repaint the screen before we see
 * the unlock, the time should be fairly accurate.
 */
static struct timeval t_start;
static bool ticking = false;
static bool mticking = false;
static bool ticking_anyway = false;
static ioid_t tick_id;
static struct timeval t_want;

/* Return the difference in milliseconds between two timevals. */
static long
delta_msec(struct timeval *t1, struct timeval *t0)
{
    return (t1->tv_sec - t0->tv_sec) * 1000 +
	   (t1->tv_usec - t0->tv_usec + 500) / 1000;
}

static void
keep_ticking(ioid_t id _is_unused)
{
    struct timeval t1;
    long msec;

    do {
	gettimeofday(&t1, NULL);
	t_want.tv_sec++;
	msec = delta_msec(&t_want, &t1);
    } while (msec <= 0);
    tick_id = AddTimeOut(msec, keep_ticking);
    vstatus_timing(&t_start, &t1);
}

void
ticking_start(bool anyway)
{
    gettimeofday(&t_start, NULL);
    mticking = true;

    vstatus_untiming();
    if (ticking) {
	RemoveTimeOut(tick_id);
    }
    ticking = true;
    ticking_anyway = anyway;
    tick_id = AddTimeOut(1000, keep_ticking);
    t_want = t_start;
}

static void
ticking_stop(struct timeval *tp)
{
    struct timeval t1;
    unsigned long cs;

    if (tp == NULL) {
	gettimeofday(&t1, NULL);
	tp = &t1;
    }
    if (mticking) {
	mticking = false;
    } else {
	return;
    }

    if (!ticking) {
	return;
    }
    RemoveTimeOut(tick_id);
    ticking = false;

    if (toggled(SHOW_TIMING) || ticking_anyway) {
	vstatus_timing(&t_start, tp);
    }

    cs = ((tp->tv_sec - t_start.tv_sec) * 1000000L) +
	(tp->tv_usec - t_start.tv_usec);
    vtrace("Host %s took %ld.%06lds to complete\n",
	    ticking_anyway? "negotiation step": "operation",
	    cs / 1000000L,
	    cs % 1000000L);
    ticking_anyway = false;
}

/*
 * Queries.
 */
const char *
ctlr_query_cur_size(void)
{
    return txAsprintf("rows %u columns %u", ROWS, COLS);
}

const char *
ctlr_query_cur_size_old(void)
{
    return txAsprintf("%u %u", ROWS, COLS);
}

const char *
ctlr_query_cursor(void)
{
    return txAsprintf("%u %u", cursor_addr / COLS, cursor_addr % COLS);
}

const char *
ctlr_query_cursor1(void)
{
    return txAsprintf("row %u column %u offset %u",
	    (cursor_addr / COLS) + 1, (cursor_addr % COLS) + 1, cursor_addr);
}

const char *
ctlr_query_formatted(void)
{
    return formatted? "formatted": "unformatted";
}

const char *
ctlr_query_max_size(void)
{
    return txAsprintf("rows %u columns %u", maxROWS, maxCOLS);
}

const char *
ctlr_query_max_size_old(void)
{
    return txAsprintf("%u %u", maxROWS, maxCOLS);
}

/*
 * Cursor enable/disable.
 */
void
ctlr_enable_cursor(bool enable, unsigned source)
{
    static unsigned disables = 0;
    int new_disables;
    static const char *source_name[] = {
	NULL,
	"scroll",
	"nvt",
	NULL,
	"connect"
    };

    vtrace("ctlr_enable_cursor(%s, %s)\n", enable? ResTrue: ResFalse,
	    source_name[source]);

    /* Compute the new disable mask. */
    if (enable) {
	new_disables = disables & ~source;
    } else {
	new_disables = disables | source;
    }

    if (!!disables ^ !!new_disables) {
	/* Overall state change. */
	enable_cursor(!new_disables);
    }
    disables = new_disables;
}

