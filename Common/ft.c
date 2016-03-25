/*
 * Copyright (c) 1996-2015 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft.c
 *		Common IND$FILE file transfer logic.
 */

#include "globals.h"

#include <assert.h>
#include <errno.h>

#include "appres.h"
#include "actions.h"
#include "ft_cut.h"
#include "ft_dft.h"
#include "ft_private.h" /* must precede ft_gui.h */
#include "ft_gui.h"
#include "unicodec.h"
#include "ft.h"
#include "host.h"
#include "idle.h"
#include "kybd.h"
#include "macros.h"
#include "popups.h"
#include "resources.h"
#include "utils.h"
#include "varbuf.h"

/* Macros. */

/* Globals. */
enum ft_state ft_state = FT_NONE;	/* File transfer state */
ft_conf_t *ftc;				/* Current file transfer config */

/* Statics. */
static ft_conf_t transfer_ft_conf;	/* FT config for Transfer() action */
static ft_conf_t gui_ft_conf;		/* FT config for GUI (actually just
					   c3270; x3270 uses its own) */
static bool gui_conf_initted = false;

static struct timeval t0;		/* Starting time */

/* Translation table: "ASCII" to EBCDIC, as seen by IND$FILE. */
unsigned char i_asc2ft[256] = {
0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f,0x16,0x05,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26,0x18,0x19,0x3f,0x27,0x1c,0x1d,0x1e,0x1f,
0x40,0x5a,0x7f,0x7b,0x5b,0x6c,0x50,0x7d,0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f,
0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x4a,0xe0,0x4f,0x5f,0x6d,
0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xc0,0x6a,0xd0,0xa1,0x07,
0x20,0x21,0x22,0x23,0x24,0x15,0x06,0x17,0x28,0x29,0x2a,0x2b,0x2c,0x09,0x0a,0x1b,
0x30,0x31,0x1a,0x33,0x34,0x35,0x36,0x08,0x38,0x39,0x3a,0x3b,0x04,0x14,0x3e,0xe1,
0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
0x58,0x59,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x70,0x71,0x72,0x73,0x74,0x75,
0x76,0x77,0x78,0x80,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,0x90,0x9a,0x9b,0x9c,0x9d,0x9e,
0x9f,0xa0,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xca,0xcb,0xcc,0xcd,0xce,0xcf,0xda,0xdb,
0xdc,0xdd,0xde,0xdf,0xea,0xeb,0xec,0xed,0xee,0xef,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

/* Translation table: EBCDIC to "ASCII", as seen by IND$FILE. */
unsigned char i_ft2asc[256] = {
0x00,0x01,0x02,0x03,0x9c,0x09,0x86,0x7f,0x97,0x8d,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x9d,0x85,0x08,0x87,0x18,0x19,0x92,0x8f,0x1c,0x1d,0x1e,0x1f,
0x80,0x81,0x82,0x83,0x84,0x00,0x17,0x1b,0x88,0x89,0x8a,0x8b,0x8c,0x05,0x06,0x07,
0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,0x98,0x99,0x9a,0x9b,0x14,0x15,0x9e,0x1a,
0x20,0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0x5b,0x2e,0x3c,0x28,0x2b,0x5d,
0x26,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0x21,0x24,0x2a,0x29,0x3b,0x5e,
0x2d,0x2f,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0x7c,0x2c,0x25,0x5f,0x3e,0x3f,
0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0x60,0x3a,0x23,0x40,0x27,0x3d,0x22,
0xc3,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,
0xca,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0xcb,0xcc,0xcd,0xce,0xcf,0xd0,
0xd1,0x7e,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
0x7b,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0xe8,0xe9,0xea,0xeb,0xec,0xed,
0x7d,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0xee,0xef,0xf0,0xf1,0xf2,0xf3,
0x5c,0x9f,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

/* Keywords for the Transfer action. */
enum ft_parm_name {
    PARM_DIRECTION,
    PARM_HOST_FILE,
    PARM_LOCAL_FILE,
    PARM_HOST,
    PARM_MODE,
    PARM_CR,
    PARM_REMAP,
    PARM_EXIST,
    PARM_RECFM,
    PARM_LRECL,
    PARM_BLKSIZE,
    PARM_ALLOCATION,
    PARM_PRIMARY_SPACE,
    PARM_SECONDARY_SPACE,
    PARM_BUFFER_SIZE,
    PARM_AVBLOCK,
#if defined(_WIN32) /*[*/
    PARM_WINDOWS_CODEPAGE,
#endif /*]*/
    N_PARMS
};
static struct {
    const char *name;
    char *value;
    const char *keyword[4];
} tp[N_PARMS] = {
    { "Direction",	NULL, { "receive", "send" } },
    { "HostFile" },
    { "LocalFile" },
    { "Host",		NULL, { "tso", "vm", "cics" } },
    { "Mode",		NULL, { "ascii", "binary" } },
    { "Cr",		NULL, { "auto", "remove",	"add", "keep" } },
    { "Remap",		NULL, { "yes", "no" } },
    { "Exist",		NULL, { "keep", "replace", "append" } },
    { "Recfm",		NULL, { "default", "fixed", "variable", "undefined" } },
    { "Lrecl" },
    { "Blksize" },
    { "Allocation",	NULL, { "default", "tracks", "cylinders", "avblock" } },
    { "PrimarySpace" },
    { "SecondarySpace" },
    { "BufferSize" },
    { "Avblock" },
#if defined(_WIN32) /*[*/
    { "WindowsCodePage" },
#endif /*]*/
};
ft_tstate_t fts;

static ioid_t ft_start_id = NULL_IOID;

static void ft_connected(bool ignored);
static void ft_in3270(bool ignored);

static action_t Transfer_action;

/**
 * File transfer module registration.
 */
void
ft_register(void)
{
    static action_table_t ft_actions[] = {
	{ "Transfer",	Transfer_action,	ACTION_KE }
    };

    /* Register for state changes. */
    register_schange(ST_CONNECT, ft_connected);
    register_schange(ST_3270_MODE, ft_in3270);

    /* Register actions. */
    register_actions(ft_actions, array_count(ft_actions));
}

/* Encode/decode for host type. */
bool
ft_encode_host_type(const char *s, host_type_t *ht)
{
    int k;

    for (k = 0; tp[PARM_HOST].keyword[k] != NULL && k < 4; k++) {
	if (!strncasecmp(s, tp[PARM_HOST].keyword[k], strlen(s)))  {
	    *ht = (host_type_t)k;
	    return true;
	}
    }

    *ht = HT_TSO;
    return false;
}

const char *
ft_decode_host_type(host_type_t ht)
{
    if (ht < HT_TSO || ht > HT_CICS) {
	return "unknown";
    }
    return tp[PARM_HOST].keyword[(int)ht];
}

/* Encode/decode for recfm. */
bool
ft_encode_recfm(const char *s, recfm_t *recfm)
{
    int k;

    for (k = 0; tp[PARM_RECFM].keyword[k] != NULL && k < 4; k++) {
	if (!strncasecmp(s, tp[PARM_RECFM].keyword[k], strlen(s)))  {
	    *recfm = (recfm_t)k;
	    return true;
	}
    }

    *recfm = DEFAULT_RECFM;
    return false;
}

const char *
ft_decode_recfm(recfm_t recfm)
{
    if (recfm < DEFAULT_RECFM || recfm > RECFM_UNDEFINED) {
	return "unknown";
    }
    return tp[PARM_RECFM].keyword[(int)recfm];
}

/* Encode/decode for units (allocation). */
bool
ft_encode_units(const char *s, units_t *units)
{
    int k;

    for (k = 0; tp[PARM_ALLOCATION].keyword[k] != NULL && k < 4; k++) {
	if (!strncasecmp(s, tp[PARM_ALLOCATION].keyword[k], strlen(s)))  {
	    *units = (units_t)k;
	    return true;
	}
    }

    *units = DEFAULT_UNITS;
    return false;
}

const char *
ft_decode_units(units_t units)
{
    if (units < DEFAULT_UNITS || units > AVBLOCK) {
	return "unknown";
    }
    return tp[PARM_ALLOCATION].keyword[(int)units];
}

void
ft_init(void)
{
    /*
     * Do a dummy initialization of the Transfer action's ft_config, to catch
     * and display any errors in the resource defaults.
     */
    ft_init_conf(&transfer_ft_conf);
}

/*
 * Initialize or re-initialize an ft_conf structure from the appres
 * defaults.
 */
void
ft_init_conf(ft_conf_t *p)
{
    /* Initialize the private state. */
    p->receive_flag = true;
    p->host_type = HT_TSO;
    p->ascii_flag = true;
    p->cr_flag = p->ascii_flag;
    p->remap_flag = p->ascii_flag;
    p->allow_overwrite = false;
    p->append_flag = false;
    p->recfm = DEFAULT_RECFM;
    p->units = DEFAULT_UNITS;
    p->lrecl = 0;
    p->blksize = 0;
    p->primary_space = 0;
    p->secondary_space = 0;
    p->avblock = 0;
    p->dft_buffersize = set_dft_buffersize(0);
#if defined(_WIN32) /*[*/
    p->windows_codepage = appres.ft.codepage?
	appres.ft.codepage:
	(appres.ft.codepage_bc? appres.ft.codepage_bc: appres.local_cp);
#endif /*]*/

    /* Apply resources. */
    if (appres.ft.blksize) {
	p->blksize = appres.ft.blksize;
    }
    if (appres.ft.direction) {
	if (!strcasecmp(appres.ft.direction, "receive")) {
	    p->receive_flag = true;
	} else if (!strcasecmp(appres.ft.direction, "send")) {
	    p->receive_flag = false;
	} else {
	    xs_warning("Invalid %s '%s', ignoring", ResFtDirection,
		    appres.ft.direction);
	    appres.ft.direction = NULL;
	}
    }
    if (appres.ft.host &&
	    !ft_encode_host_type(appres.ft.host, &p->host_type)) {
	xs_warning("Invalid %s '%s', ignoring", ResFtHost, appres.ft.host);
	appres.ft.host = NULL;
    }
    if (appres.ft.host_file) {
	Replace(p->host_filename, NewString(appres.ft.host_file));
    } else {
	Replace(p->host_filename, NULL);
    }
    if (appres.ft.local_file) {
	Replace(p->local_filename, NewString(appres.ft.local_file));
    } else {
	Replace(p->local_filename, NULL);
    }
    if (appres.ft.mode) {
	if (!strcasecmp(appres.ft.mode, "ascii")) {
	    p->ascii_flag = true;
	} else if (!strcasecmp(appres.ft.mode, "binary")) {
	    p->ascii_flag = false;
	} else {
	    xs_warning("Invalid %s '%s', ignoring", ResFtMode, appres.ft.mode);
	    appres.ft.host = NULL;
	}
    }
    if (appres.ft.cr) { /* must come after processing "ascii" */
	if (!strcasecmp(appres.ft.cr, "auto")) {
	    p->cr_flag = p->ascii_flag;
	} else if (!strcasecmp(appres.ft.cr, "add") ||
		   !strcasecmp(appres.ft.cr, "remove")) {
	    p->cr_flag = true;
	} else if (!strcasecmp(appres.ft.cr, "keep")) {
	    p->cr_flag = false;
	} else {
	    xs_warning("Invalid %s '%s', ignoring", ResFtCr, appres.ft.cr);
	    appres.ft.cr = NULL;
	}
    }
    if (appres.ft.remap) {
	if (!strcasecmp(appres.ft.remap, "yes")) {
	    p->remap_flag = true;
	} else if (!strcasecmp(appres.ft.remap, "no")) {
	    p->remap_flag = false;
	} else {
	    xs_warning("Invalid %s '%s', ignoring", ResFtRemap,
		    appres.ft.remap);
	    appres.ft.remap = NULL;
	}
    }
    if (appres.ft.exist) {
	if (!strcasecmp(appres.ft.exist, "keep")) {
	    p->allow_overwrite = false;
	    p->append_flag = false;
	} else if (!strcasecmp(appres.ft.exist, "replace")) {
	    p->allow_overwrite = true;
	    p->append_flag = false;
	} else if (!strcasecmp(appres.ft.exist, "append")) {
	    p->allow_overwrite = false;
	    p->append_flag = true;
	} else {
	    xs_warning("Invalid %s '%s', ignoring", ResFtExist,
		    appres.ft.exist);
	    appres.ft.exist = NULL;
	}
    }
    if (appres.ft.primary_space) {
	p->primary_space = appres.ft.primary_space;
    }
    if (appres.ft.recfm &&
	    !ft_encode_recfm(appres.ft.recfm, &p->recfm)) {
	xs_warning("Invalid %s '%s', ignoring", ResFtRecfm, appres.ft.recfm);
	appres.ft.recfm = NULL;
    }
    if (appres.ft.secondary_space) {
	p->secondary_space = appres.ft.secondary_space;
    }
    if (appres.ft.lrecl) {
	p->lrecl = appres.ft.lrecl;
    }
    if (appres.ft.allocation &&
	    !ft_encode_units(appres.ft.allocation, &p->units)) {
	xs_warning("Invalid %s '%s', ignoring", ResFtAllocation,
		appres.ft.allocation);
	appres.ft.allocation = NULL;
    }
    if (appres.ft.avblock) {
	p->avblock = appres.ft.avblock;
    }
    p->dft_buffersize = set_dft_buffersize(0);
}

/* Return the right value for fopen()ing the local file. */
static const char *
ft_local_fflag(ft_conf_t *p)
{
    static char ret[3];
    int nr = 0;

    ret[nr++] = p->receive_flag? (p->append_flag? 'a': 'w' ): 'r';
    if (!p->ascii_flag) {
	ret[nr++] = 'b';
    }
    ret[nr] = '\0';
    return ret;
}

/* Timeout function for stalled transfers. */
static void
ft_didnt_start(ioid_t id _is_unused)
{
    ft_start_id = NULL_IOID;

    if (fts.local_file != NULL) {
	fclose(fts.local_file);
	fts.local_file = NULL;
	if (ftc->receive_flag && !ftc->append_flag) {
	    unlink(ftc->local_filename);
	}
    }

    ft_complete(get_message("ftStartTimeout"));
}

/* External entry points called by ft_dft and ft_cut. */

/* Pop up a message, end the transfer. */
void
ft_complete(const char *errmsg)
{
    /* Close the local file. */
    if (fts.local_file != NULL && fclose(fts.local_file) < 0) {
	popup_an_errno(errno, "close(%s)", ftc->local_filename);
    }
    fts.local_file = NULL;

    /* Clean up the state. */
    ft_state = FT_NONE;
    if (ft_start_id != NULL_IOID) {
	RemoveTimeOut(ft_start_id);
	ft_start_id = NULL_IOID;
    }

    /* Get the idle timeout going again. */
    idle_ft_complete();

    /* Pop down the in-progress shell. */
    ft_gui_progress_popdown();

    /* Pop up the text. */
    if (errmsg != NULL) {
	char *msg_copy = NewString(errmsg);

	/* Make sure the error message will fit on the pop-up. */
	ft_gui_errmsg_prepare(msg_copy);

	/* Clear out the progress display. */
	ft_gui_clear_progress();

	/* Pop up the error. */
	popup_an_error("%s", msg_copy);
	Free(msg_copy);
    } else {
	struct timeval t1;
	double bytes_sec;
	char *buf;

	(void) gettimeofday(&t1, NULL);
	bytes_sec = (double)fts.length /
		((double)(t1.tv_sec - t0.tv_sec) + 
		 (double)(t1.tv_usec - t0.tv_usec) / 1.0e6);
	buf = xs_buffer(get_message("ftComplete"), fts.length,
		display_scale(bytes_sec),
		fts.is_cut ? "CUT" : "DFT");
	ft_gui_clear_progress();
	ft_gui_complete_popup(buf);
	Free(buf);
    }

    /* I hope I can do this unconditionally. */
    sms_continue();
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_update_length(void)
{
    ft_gui_update_length(fts.length);
}

/* Process a transfer acknowledgement. */
void
ft_running(bool is_cut)
{
    if (ft_state == FT_AWAIT_ACK) {
	ft_state = FT_RUNNING;
	if (ft_start_id != NULL_IOID) {
	    RemoveTimeOut(ft_start_id);
	    ft_start_id = NULL_IOID;
	}
    }
    fts.is_cut = is_cut;
    (void) gettimeofday(&t0, NULL);
    fts.length = 0;

    ft_gui_running(fts.length);
}

/* Process a protocol-generated abort. */
void
ft_aborting(void)
{
    if (ft_state == FT_RUNNING || ft_state == FT_ABORT_WAIT) {
	ft_state = FT_ABORT_SENT;
	ft_gui_aborting();
    }
}

/* Process a disconnect abort. */
static void
ft_connected(bool ignored _is_unused)
{
    if (!CONNECTED && ft_state != FT_NONE) {
	ft_complete(get_message("ftDisconnected"));
    }
}

/* Process an abort from no longer being in 3270 mode. */
static void
ft_in3270(bool ignored _is_unused)
{
    if (!IN_3270 && ft_state != FT_NONE) {
	ft_complete(get_message("ftNot3270"));
    }
}

/*
 * Start a file transfer, based on the contents of an ft_state structure.
 *
 * This function will fail if the file exists and the overwrite flag is not
 * set.
 *
 * Returns the local file pointer, or NULL if the transfer could not start.
 * If an error is detected, it will call popup_an_error() with an appropriate
 * message.
 */
FILE *
ft_go(ft_conf_t *p)
{
    FILE *f;
    varbuf_t r;
    unsigned flen;

    /* Adjust the DFT buffer size. */
    p->dft_buffersize = set_dft_buffersize(p->dft_buffersize);

    /* See if the local file can be overwritten. */
    if (p->receive_flag && !p->append_flag && !p->allow_overwrite) {
	f = fopen(p->local_filename, p->ascii_flag? "r": "rb");
	if (f != NULL) {
	    (void) fclose(f);
	    popup_an_error("Transfer: File exists");
	    return NULL;
	}
    }

    /* Open the local file. */
    f = fopen(p->local_filename, ft_local_fflag(p));
    if (f == NULL) {
	popup_an_errno(errno, "Local file '%s'", p->local_filename);
	return NULL;
    }

    /* Build the ind$file command */
    vb_init(&r);
    vb_appendf(&r, "IND\\e005BFILE %s %s %s",
	    p->receive_flag? "GET": "PUT",
	    p->host_filename,
	    (p->host_type != HT_TSO)? "(": "");
    if (p->ascii_flag) {
	vb_appends(&r, "ASCII");
    } else if (p->host_type == HT_CICS) {
	vb_appends(&r, "BINARY");
    }
    if (p->ascii_flag && p->cr_flag) {
	vb_appends(&r, " CRLF");
    } else if (p->host_type == HT_CICS) {
	vb_appends(&r, " NOCRLF");
    }
    if (p->append_flag && !p->receive_flag) {
	vb_appends(&r, " APPEND");
    }
    if (!p->receive_flag) {
	if (p->host_type == HT_TSO) {
	    if (p->recfm != DEFAULT_RECFM) {
		/* RECFM Entered, process */
		vb_appends(&r, " RECFM(");
		switch (p->recfm) {
		case RECFM_FIXED:
		    vb_appends(&r, "F");
		    break;
		case RECFM_VARIABLE:
		    vb_appends(&r, "V");
		    break;
		case RECFM_UNDEFINED:
		    vb_appends(&r, "U");
		    break;
		default:
		    break;
		};
		vb_appends(&r, ")");
		if (p->lrecl) {
		    vb_appendf(&r, " LRECL(%d)", p->lrecl);
		}
		if (p->blksize) {
		    vb_appendf(&r, " BLKSIZE(%d)", p->blksize);
		}
	    }
	    if (p->units != DEFAULT_UNITS) {
		/* Space Entered, processs it */
		vb_appendf(&r, " SPACE(%d", p->primary_space);
		if (p->secondary_space) {
		    vb_appendf(&r, ",%d", p->secondary_space);
		}
		vb_appends(&r, ")");
		switch (p->units) {
		case TRACKS:
		    vb_appends(&r, " TRACKS");
		    break;
		case CYLINDERS:
		    vb_appends(&r, " CYLINDERS");
		    break;
		case AVBLOCK:
		    vb_appendf(&r, " AVBLOCK(%d)", p->avblock);
		    break;
		default:
		    break;
		}
	    }
	} else if (p->host_type == HT_VM) {
	    if (p->recfm != DEFAULT_RECFM) {
		vb_appends(&r, " RECFM ");
		switch (p->recfm) {
		case RECFM_FIXED:
		    vb_appends(&r, "F");
		    break;
		case RECFM_VARIABLE:
		    vb_appends(&r, "V");
		    break;
		default:
		    break;
		};

		if (p->lrecl) {
		    vb_appendf(&r, " LRECL %d", p->lrecl);
		}
	    }
	}
    }
    vb_appends(&r, "\\n");

    /* Erase the line and enter the command. */
    flen = kybd_prime();
    if (!flen || flen < vb_len(&r) - 1) {
	vb_free(&r);
	if (f != NULL) {
	    fclose(f);
	    if (p->receive_flag && !p->append_flag) {
		unlink(p->local_filename);
	    }
	}
	popup_an_error("%s", get_message("ftUnable"));
	return NULL;
    }
    (void) emulate_input(vb_buf(&r), vb_len(&r), false);
    vb_free(&r);

    /* Now proceed with this context. */
    ftc = p;

    /* Finish common initialization. */
    fts.last_cr = false;
    fts.is_cut = false;
    fts.last_dbcs = false;
    fts.dbcs_state = FT_DBCS_NONE;

    ft_state = FT_AWAIT_ACK;
    idle_ft_start();

    return f;
}

/*
 * Parse the keywords for the Transfer() action.
 *
 * Returns a pointer to the filled-out ft_state structure, or NULL for
 * errors.
 */
static ft_conf_t *
parse_ft_keywords(unsigned argc, const char **argv)
{
    ft_conf_t *p = &transfer_ft_conf;
    int i, k;
    unsigned j;
    char *ptr;

    /* Unlike the GUIs, always set everything to defaults. */
    ft_init_conf(p);
    p->is_action = true;
    for (i = 0; i < N_PARMS; i++) {
	Replace(tp[i].value, NULL);
    }

    /* The special keyword 'Defaults' means 'just use the defaults'. */
    if (argc == 1 && !strcasecmp(argv[0], "Defaults")) {
	argc--;
	argv++;
    }

    /* See what they specified. */
    for (j = 0; j < argc; j++) {
	for (i = 0; i < N_PARMS; i++) {
	    char *eq;
	    size_t kwlen;

	    eq = strchr(argv[j], '=');
	    if (eq == NULL || eq == argv[j] || !*(eq + 1)) {
		popup_an_error("Transfer: Invalid option syntax: '%s'",
			argv[j]);
		return NULL;
	    }
	    kwlen = eq - argv[j];
	    if (!strncasecmp(argv[j], tp[i].name, kwlen)
		    && !tp[i].name[kwlen]) {
		if (tp[i].keyword[0]) {
		    for (k = 0; tp[i].keyword[k] != NULL && k < 4; k++) {
			if (!strncasecmp(eq + 1, tp[i].keyword[k],
				    strlen(eq + 1))) {
			    break;
			}
		    }
		    if (k >= 4 || tp[i].keyword[k] == NULL) {
			popup_an_error("Transfer: Invalid option value: '%s'",
				eq + 1);
			return NULL;
		    }
		} else switch (i) {
		    case PARM_LRECL:
		    case PARM_BLKSIZE:
		    case PARM_PRIMARY_SPACE:
		    case PARM_SECONDARY_SPACE:
		    case PARM_BUFFER_SIZE:
#if defined(_WIN32) /*[*/
		    case PARM_WINDOWS_CODEPAGE:
#endif /*]*/
			(void) strtol(eq + 1, &ptr, 10);
			if (ptr == eq + 1 || *ptr) {
			    popup_an_error("Transfer: Invalid option value: "
				    "'%s'", eq + 1);
			    return NULL;
			}
			break;
		    default:
			break;
		}
		tp[i].value = NewString(eq + 1);
		break;
	    }
	}
	if (i >= N_PARMS) {
	    popup_an_error("Transfer: Unknown option: '%s'", argv[j]);
	    return NULL;
	}
    }

    /* Transfer from keywords to the ft_state. */
    if (tp[PARM_DIRECTION].value) {
	p->receive_flag = !strcasecmp(tp[PARM_DIRECTION].value, "receive");
    }
    if (tp[PARM_HOST_FILE].value) {
	p->host_filename = NewString(tp[PARM_HOST_FILE].value);
    }
    if (tp[PARM_LOCAL_FILE].value) {
	p->local_filename = NewString(tp[PARM_LOCAL_FILE].value);
    }
    if (tp[PARM_HOST].value) {
	(void) ft_encode_host_type(tp[PARM_HOST].value, &p->host_type);
    }
    if (tp[PARM_MODE].value) {
	p->ascii_flag = !strcasecmp(tp[PARM_MODE].value, "ascii");
    }
    if (tp[PARM_CR].value) {
	if (!strcasecmp(tp[PARM_CR].value, "auto")) {
	    p->cr_flag = p->ascii_flag;
	} else {
	    if (!p->ascii_flag) {
		popup_an_error("Transfer: Invalid 'Cr' option for ASCII mode");
		return NULL;
	    }
	    p->cr_flag = !strcasecmp(tp[PARM_CR].value, "remove") ||
			 !strcasecmp(tp[PARM_CR].value, "add");
	}
    }
    if (p->ascii_flag && tp[PARM_REMAP].value) {
	p->remap_flag = !strcasecmp(tp[PARM_REMAP].value, "yes");
    }
    if (tp[PARM_EXIST].value) {
	p->append_flag = !strcasecmp(tp[PARM_EXIST].value, "append");
	p->allow_overwrite = !strcasecmp(tp[PARM_EXIST].value, "replace");
    }
    if (tp[PARM_RECFM].value) {
	(void) ft_encode_recfm(tp[PARM_RECFM].value, &p->recfm);
    }
    if (tp[PARM_LRECL].value) {
	p->lrecl = atoi(tp[PARM_LRECL].value);
    }
    if (tp[PARM_BLKSIZE].value) {
	p->blksize = atoi(tp[PARM_BLKSIZE].value);
    }
    if (tp[PARM_ALLOCATION].value) {
	(void) ft_encode_units(tp[PARM_ALLOCATION].value, &p->units);
    }
    if (tp[PARM_PRIMARY_SPACE].value) {
	p->primary_space = atoi(tp[PARM_PRIMARY_SPACE].value);
    }
    if (tp[PARM_SECONDARY_SPACE].value) {
	p->secondary_space = atoi(tp[PARM_SECONDARY_SPACE].value);
    }
    if (tp[PARM_BUFFER_SIZE].value != NULL) {
	p->dft_buffersize = atoi(tp[PARM_BUFFER_SIZE].value);
    }
    if (tp[PARM_AVBLOCK].value) {
	p->avblock = atoi(tp[PARM_AVBLOCK].value);
    }
#if defined(_WIN32) /*[*/
    if (tp[PARM_WINDOWS_CODEPAGE].value != NULL) {
	p->windows_codepage = atoi(tp[PARM_WINDOWS_CODEPAGE].value);
    }
#endif /*]*/

    /* Check for required values. */
    if (!p->host_filename) {
	popup_an_error("Transfer: Missing 'HostFile' option");
	return NULL;
    }
    if (!p->local_filename) {
	popup_an_error("Transfer: Missing 'LocalFile' option");
	return NULL;
    }
    if (p->host_type == HT_TSO &&
	    !p->receive_flag &&
	    p->units != DEFAULT_UNITS &&
	    p->primary_space <= 0) {
	popup_an_error("Transfer: Missing or invalid 'PrimarySpace'");
	return NULL;
    }
    if (p->host_type == HT_TSO &&
	    !p->receive_flag &&
	    p->units == AVBLOCK &&
	    p->avblock <= 0) {
	popup_an_error("Transfer: Missing or invalid 'Avblock'");
	return NULL;
    }

    /* Check for contradictory values. */
    if (tp[PARM_CR].value && !p->ascii_flag) {
	popup_an_error("Transfer: 'Cr' is only for ASCII transfers");
	return NULL;
    }
    if (tp[PARM_REMAP].value && !p->ascii_flag) {
	popup_an_error("Transfer: 'Remap' is only for ASCII transfers");
	return NULL;
    }
    if (tp[PARM_RECFM].value && p->receive_flag) {
	popup_an_error("Transfer: 'Recfm' is only for sending files");
	return NULL;
    }
    if (tp[PARM_RECFM].value && p->host_type != HT_TSO &&
	    p->host_type != HT_VM) {
	popup_an_error("Transfer: 'Recfm' is only for TSO and VM hosts");
	return NULL;
    }
    if (tp[PARM_LRECL].value && p->receive_flag) {
	popup_an_error("Transfer: 'Lrecl' is only for sending files");
	return NULL;
    }
    if (tp[PARM_BLKSIZE].value && p->receive_flag) {
	popup_an_error("Transfer: 'Blksize' is only for sending files");
	return NULL;
    }
    if (tp[PARM_BLKSIZE].value && p->host_type != HT_TSO) {
	popup_an_error("Transfer: 'Blksize' is only for TSO hosts");
	return NULL;
    }
    if (tp[PARM_ALLOCATION].value && p->receive_flag) {
	popup_an_error("Transfer: 'Allocation' is only for sending files");
	return NULL;
    }
    if (tp[PARM_ALLOCATION].value && p->host_type != HT_TSO) {
	popup_an_error("Transfer: 'Allocation' is only for TSO hosts");
	return NULL;
    }
    if (tp[PARM_PRIMARY_SPACE].value && p->receive_flag) {
	popup_an_error("Transfer: 'PrimarySpace' is only for sending files");
	return NULL;
    }
    if (tp[PARM_PRIMARY_SPACE].value && p->host_type != HT_TSO) {
	popup_an_error("Transfer: 'PrimarySpace' is only for TSO hosts");
	return NULL;
    }
    if (tp[PARM_SECONDARY_SPACE].value && p->receive_flag) {
	popup_an_error("Transfer: 'SecondarySpace' is only for sending files");
	return NULL;
    }
    if (tp[PARM_SECONDARY_SPACE].value && p->host_type != HT_TSO) {
	popup_an_error("Transfer: 'SecondarySpace' is only for TSO hosts");
	return NULL;
    }
    if (tp[PARM_AVBLOCK].value && p->receive_flag) {
	popup_an_error("Transfer: 'Avblock' is only for sending files");
	return NULL;
    }
    if (tp[PARM_AVBLOCK].value && p->host_type != HT_TSO) {
	popup_an_error("Transfer: 'Avblock' is only for TSO hosts");
	return NULL;
    }
#if defined(_WIN32) /*[*/
    if (tp[PARM_WINDOWS_CODEPAGE].value && !p->ascii_flag) {
	popup_an_error("Transfer: 'WindowsCodePage' is only for ASCII "
		"transfers");
	return NULL;
    }
#endif /*]*/

    /* All set. */
    return p;
}

/*
 * Script/macro action for file transfer.
 *  Transfer(option=value[,...])
 *  Options are:
 *   Direction=send|receive	default receive
 *   HostFile=name		required
 *   LocalFile=name			required
 *   Host=[tso|vm|cics]		default tso
 *   Mode=[ascii|binary]	default ascii
 *   Cr=[add|remove|keep]	default add/remove
 *   Remap=[yes|no]     	default yes
 *   Exist=[keep|replace|append]	default keep
 *   Recfm=[default|fixed|variable|undefined] default default
 *   Lrecl=n			no default
 *   Blksize=n			no default
 *   Allocation=[default|tracks|cylinders|avblock] default default
 *   PrimarySpace=n		no default
 *   SecondarySpace=n		no default
 *   BufferSize			no default
 *   Avblock=n			no default
 *   WindowsCodePage=n		no default
 */

static bool  
Transfer_action(ia_t ia, unsigned argc, const char **argv)
{
    ft_conf_t *p = NULL;

    action_debug("Transfer", ia, argc, argv);

    /* Make sure we're connected. */
    if (!IN_3270) {
	popup_an_error("Transfer: Not connected");
	return false;
    }

    /* Check for interactive mode. */
    if (argc == 0) {
	if (!gui_conf_initted) {
	    ft_init_conf(&gui_ft_conf);
	    gui_ft_conf.is_action = true;
	    gui_conf_initted = true;
	}
	switch (ft_gui_interact(&gui_ft_conf)) {
	case FGI_NOP:
	    /* Hope the defaults are enough. */
	    break;
	case FGI_SUCCESS:
	    /* Proceed as specified in the ft_state. */
	    p = &gui_ft_conf;
	    break;
	case FGI_ABORT:
	    /* User said no. */
	    return false;
	}
    }

    if (p == NULL) {
	/* Parse the keywords into the ft_state structure. */
	p = parse_ft_keywords(argc, argv);
	if (p == NULL) {
	    return false;
	}
	p->is_interactive = (ia == IA_COMMAND);
    }

    /* Start the transfer. */
    fts.local_file = ft_go(p);
    if (fts.local_file == NULL) {
	return false;
    }

    /* If interactive, tell the user we're waiting. */
    ft_gui_awaiting();

    /* Set a timeout for failed command start. */
    ft_start_id = AddTimeOut(10 * 1000, ft_didnt_start);

    /* Success. */
    return true;
}

/*
 * Cancel a file transfer.
 *
 * Returns true if the transfer if the cancellation is complete, false if it
 * is pending (because it must be coordinated with the host).
 */
bool
ft_do_cancel(void)
{
    if (ft_state == FT_RUNNING) {
	ft_state = FT_ABORT_WAIT;
	return false;
    } else {
	if (ft_state != FT_NONE) {
	    ft_complete(get_message("ftUserCancel"));
	}
	return true;
    }
}

#if defined(_WIN32) /*[*/
/*
 * Windows character translation functions.
 *
 * These are wrappers around the existing functions in unicode.c, but they swap
 * the local codepage in and out to use the one specified for the transfer.
 *
 * On other platforms, these functions are #defined to their 'real'
 * counterparts.
 */
size_t
ft_ebcdic_to_multibyte(ebc_t ebc, char mb[], size_t mb_len)
{
    int local_cp = appres.local_cp;
    size_t rc;

    appres.local_cp = ftc->windows_codepage;
    rc = ebcdic_to_multibyte(ebc, mb, mb_len);
    appres.local_cp = local_cp;
    return rc;
}

int
ft_unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len)
{
    int local_cp = appres.local_cp;
    int rc;

    appres.local_cp = ftc->windows_codepage;
    rc = unicode_to_multibyte(ucs4, mb, mb_len);
    appres.local_cp = local_cp;
    return rc;
}

ucs4_t
ft_multibyte_to_unicode(const char *mb, size_t mb_len, int *consumedp,
	enum me_fail *errorp)
{
    int local_cp = appres.local_cp;
    ucs4_t rc;

    appres.local_cp = ftc->windows_codepage;
    rc = multibyte_to_unicode(mb, mb_len, consumedp, errorp);
    appres.local_cp = local_cp;
    return rc;
}

#endif /*]*/
