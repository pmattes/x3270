/*
 * Copyright (c) 1996-2014, Paul Mattes.
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
#include "actionsc.h"
#include "charsetc.h"
#include "ft_cutc.h"
#include "ft_dftc.h"
#include "ft_guic.h"
#include "ft_private.h"
#include "unicodec.h"
#include "ftc.h"
#include "dialogc.h"
#include "hostc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "objects.h"
#include "popupsc.h"
#include "screenc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "utilc.h"
#include "varbufc.h"

/* Macros. */
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#define COLUMN_GAP	40	/* distance between columns */

/* Globals. */
enum ft_state ft_state = FT_NONE;	/* File transfer state */
char *ft_local_filename;		/* Local file to transfer to/from */
FILE *ft_local_file = NULL;		/* File descriptor for local file */
Boolean ft_last_cr = False;		/* CR was last char in local file */
Boolean ascii_flag = True;		/* Convert to ascii */
Boolean cr_flag = True;			/* Add crlf to each line */
Boolean remap_flag = True;		/* Remap ASCII<->EBCDIC */
unsigned long ft_length = 0;		/* Length of transfer */
#if defined(_WIN32) /*[*/
int ft_windows_codepage;		/* Windows code page */
#endif /*]*/
ft_private_t ft_private;		/* Private state */

/* Statics. */
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

#if defined(X3270_DBCS) /*[*/
enum ftd ft_dbcs_state = FT_DBCS_NONE;
unsigned char ft_dbcs_byte1;
Boolean ft_last_dbcs = False;
#endif /*]*/

static ioid_t ft_start_id = NULL_IOID;

static void ft_connected(Boolean ignored);
static void ft_in3270(Boolean ignored);

/* Main external entry point. */

void
ft_init(void)
{
    /* Register for state changes. */
    register_schange(ST_CONNECT, ft_connected);
    register_schange(ST_3270_MODE, ft_in3270);

    /* Initialize the private state. */
    ft_private.receive_flag = True;
    ft_private.host_type = HT_TSO;
    ft_private.recfm = DEFAULT_RECFM;
    ft_private.units = DEFAULT_UNITS;
}

/* Return the right value for fopen()ing the local file. */
char *
ft_local_fflag(void)
{
    static char ret[3];
    int nr = 0;

    ret[nr++] = ft_private.receive_flag?
	(ft_private.append_flag? 'a': 'w' ): 'r';
    if (!ascii_flag) {
	ret[nr++] = 'b';
    }
    ret[nr] = '\0';
    return ret;
}

/* Timeout function for stalled transfers. */
static void
ft_didnt_start(ioid_t id _is_unused)
{
    if (ft_local_file != NULL) {
	fclose(ft_local_file);
	ft_local_file = NULL;
	if (ft_private.receive_flag && !ft_private.append_flag) {
	    unlink(ft_local_filename);
	}
    }
    ft_private.allow_overwrite = False;

    ft_complete(get_message("ftStartTimeout"));
    sms_continue();
}

/* External entry points called by ft_dft and ft_cut. */

/* Pop up a message, end the transfer. */
void
ft_complete(const char *errmsg)
{
    /* Close the local file. */
    if (ft_local_file != NULL && fclose(ft_local_file) < 0) {
	popup_an_errno(errno, "close(%s)", ft_local_filename);
    }
    ft_local_file = NULL;

    /* Clean up the state. */
    ft_state = FT_NONE;
    if (ft_start_id != NULL_IOID) {
	RemoveTimeOut(ft_start_id);
	ft_start_id = NULL_IOID;
    }

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
	char kbuf[256];

	(void) gettimeofday(&t1, NULL);
	bytes_sec = (double)ft_length /
		((double)(t1.tv_sec - t0.tv_sec) + 
		 (double)(t1.tv_usec - t0.tv_usec) / 1.0e6);
	buf = xs_buffer(get_message("ftComplete"), ft_length,
		display_scale(bytes_sec, kbuf, sizeof(kbuf)),
		ft_private.is_cut ? "CUT" : "DFT");
	if (ft_private.is_action) {
	    /* Clear out the progress display. */
	    ft_gui_clear_progress();

	    sms_info("%s", buf);
	    sms_continue();
	} else {
	    ft_gui_complete_popup(buf);
	}
	Free(buf);
    }
    ft_private.is_interactive = False;
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_update_length(void)
{
    ft_gui_update_length(ft_length);
}

/* Process a transfer acknowledgement. */
void
ft_running(Boolean is_cut)
{
    if (ft_state == FT_AWAIT_ACK) {
	ft_state = FT_RUNNING;
	if (ft_start_id != NULL_IOID) {
	    RemoveTimeOut(ft_start_id);
	    ft_start_id = NULL_IOID;
	}
    }
    ft_private.is_cut = is_cut;
    (void) gettimeofday(&t0, NULL);
    ft_length = 0;

    ft_gui_running(ft_length);
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
ft_connected(Boolean ignored _is_unused)
{
    if (!CONNECTED && ft_state != FT_NONE) {
	ft_complete(get_message("ftDisconnected"));
    }
}

/* Process an abort from no longer being in 3270 mode. */
static void
ft_in3270(Boolean ignored _is_unused)
{
    if (!IN_3270 && ft_state != FT_NONE) {
	ft_complete(get_message("ftNot3270"));
    }
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
 *   WindowsCodePage=n		no default
 */
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
#if defined(_WIN32) /*[*/
    { "WindowsCodePage" },
#endif /*]*/
};

Boolean  
Transfer_action(ia_t ia, unsigned argc, const char **argv)
{
    int i, k;
    unsigned j;
    long l;
    char *ptr;
    unsigned flen;
    varbuf_t r;

    char **xparams = (char **)argv;
    unsigned xnparams = argc;

    action_debug("Transfer", ia, argc, argv);

    ft_private.is_action = True;

    /* Make sure we're connected. */
    if (!IN_3270) {
	popup_an_error("Not connected");
	return False;
    }

    /* Check for interactive mode. */
    if (ft_gui_interact(&xparams, &xnparams)) {
	return True;
    }

    /* Set everything to the default. */
    for (i = 0; i < N_PARMS; i++) {
	Free(tp[i].value);
	if (tp[i].keyword[0] != NULL) {
	    tp[i].value = NewString(tp[i].keyword[0]);
	} else {
	    tp[i].value = NULL;
	}
    }

    /* See what they specified. */
    for (j = 0; j < xnparams; j++) {
	for (i = 0; i < N_PARMS; i++) {
	    char *eq;
	    int kwlen;

	    eq = strchr(xparams[j], '=');
	    if (eq == NULL || eq == xparams[j] || !*(eq + 1)) {
		popup_an_error("Invalid option syntax: '%s'", xparams[j]);
		return False;
	    }
	    kwlen = eq - xparams[j];
	    if (!strncasecmp(xparams[j], tp[i].name, kwlen)
		    && !tp[i].name[kwlen]) {
		if (tp[i].keyword[0]) {
		    for (k = 0; tp[i].keyword[k] != NULL && k < 4; k++) {
			if (!strcasecmp(eq + 1, tp[i].keyword[k])) {
			    break;
			}
		    }
		    if (k >= 4 || tp[i].keyword[k] == NULL) {
			popup_an_error("Invalid option value: '%s'", eq + 1);
			return False;
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
			l = strtol(eq + 1, &ptr, 10);
			l = l; /* keep gcc happy */
			if (ptr == eq + 1 || *ptr) {
			    popup_an_error("Invalid option value: '%s'",
				    eq + 1);
			    return False;
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
	    popup_an_error("Unknown option: %s", xparams[j]);
	    return False;
	}
    }

    /* Check for required values. */
    if (tp[PARM_HOST_FILE].value == NULL) {
	popup_an_error("Missing 'HostFile' option");
	return False;
    }
    if (tp[PARM_LOCAL_FILE].value == NULL) {
	popup_an_error("Missing 'LocalFile' option");
	return False;
    }

    /*
     * Start the transfer.  Much of this is duplicated from ft_start()
     * and should be made common.
     */
    if (tp[PARM_BUFFER_SIZE].value != NULL) {
	dft_buffersize = atoi(tp[PARM_BUFFER_SIZE].value);
    } else {
	dft_buffersize = 0;
    }
    set_dft_buffersize();

    ft_private.receive_flag = !strcasecmp(tp[PARM_DIRECTION].value, "receive");
    ft_private.append_flag = !strcasecmp(tp[PARM_EXIST].value, "append");
    ft_private.allow_overwrite = !strcasecmp(tp[PARM_EXIST].value, "replace");
    ascii_flag = !strcasecmp(tp[PARM_MODE].value, "ascii");
    if (!strcasecmp(tp[PARM_CR].value, "auto")) {
	cr_flag = ascii_flag;
    } else {
	if (!ascii_flag) {
	    popup_an_error("Invalid 'Cr' option for ASCII mode");
	    return False;
	}
	cr_flag = !strcasecmp(tp[PARM_CR].value, "remove") ||
		  !strcasecmp(tp[PARM_CR].value, "add");
    }
    if (ascii_flag) {
	remap_flag = !strcasecmp(tp[PARM_REMAP].value, "yes");
    }
    if (!strcasecmp(tp[PARM_HOST].value, "tso")) {
	ft_private.host_type = HT_TSO;
    } else if (!strcasecmp(tp[PARM_HOST].value, "vm")) {
	ft_private.host_type = HT_VM;
    } else if (!strcasecmp(tp[PARM_HOST].value, "cics")) {
	ft_private.host_type = HT_CICS;
    } else {
	assert(0);
    }
    ft_private.recfm = DEFAULT_RECFM;
    for (k = 0; tp[PARM_RECFM].keyword[k] != NULL && k < 4; k++) {
	if (!strcasecmp(tp[PARM_RECFM].value, tp[PARM_RECFM].keyword[k]))  {
	    ft_private.recfm = (enum recfm)k;
	    break;
	}
    }
    ft_private.units = DEFAULT_UNITS;
    for (k = 0; tp[PARM_ALLOCATION].keyword[k] != NULL && k < 4; k++) {
	if (!strcasecmp(tp[PARM_ALLOCATION].value,
			tp[PARM_ALLOCATION].keyword[k]))  {
	    ft_private.units = (enum units)k;
	    break;
	}
    }

#if defined(_WIN32) /*[*/
    if (tp[PARM_WINDOWS_CODEPAGE].value != NULL) {
	ft_windows_codepage = atoi(tp[PARM_WINDOWS_CODEPAGE].value);
    } else if (appres.ft_cp) {
	ft_windows_codepage = appres.ft_cp;
    } else {
	ft_windows_codepage = appres.local_cp;
    }
#endif /*]*/

    ft_private.host_filename = tp[PARM_HOST_FILE].value;
    ft_local_filename = tp[PARM_LOCAL_FILE].value;

    /* See if the local file can be overwritten. */
    if (ft_private.receive_flag && !ft_private.append_flag && !ft_private.allow_overwrite) {
	ft_local_file = fopen(ft_local_filename, ascii_flag? "r": "rb");
	if (ft_local_file != NULL) {
	    (void) fclose(ft_local_file);
	    popup_an_error("File exists");
	    return False;
	}
    }

    /* Open the local file. */
    ft_local_file = fopen(ft_local_filename, ft_local_fflag());
    if (ft_local_file == NULL) {
	popup_an_errno(errno, "Local file '%s'", ft_local_filename);
	return False;
    }

    /* Build the ind$file command */
    vb_init(&r);
    vb_appendf(&r, "IND\\e005BFILE %s %s %s",
	    ft_private.receive_flag? "GET": "PUT",
	    ft_private.host_filename,
	    (ft_private.host_type != HT_TSO)? "(": "");
    if (ascii_flag) {
	vb_appends(&r, "ASCII");
    } else if (ft_private.host_type == HT_CICS) {
	vb_appends(&r, "BINARY");
    }
    if (cr_flag) {
	vb_appends(&r, " CRLF");
    } else if (ft_private.host_type == HT_CICS) {
	vb_appends(&r, " NOCRLF");
    }
    if (ft_private.append_flag && !ft_private.receive_flag) {
	vb_appends(&r, " APPEND");
    }
    if (!ft_private.receive_flag) {
	if (ft_private.host_type == HT_TSO) {
	    if (ft_private.recfm != DEFAULT_RECFM) {
		/* RECFM Entered, process */
		vb_appends(&r, " RECFM(");
		switch (ft_private.recfm) {
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
		if (tp[PARM_LRECL].value != NULL) {
		    vb_appendf(&r, " LRECL(%s)", tp[PARM_LRECL].value);
		}
		if (tp[PARM_BLKSIZE].value != NULL) {
		    vb_appendf(&r, " BLKSIZE(%s)", tp[PARM_BLKSIZE].value);
		}
	    }
	    if (ft_private.units != DEFAULT_UNITS) {
		/* Space Entered, processs it */
		switch (ft_private.units) {
		case TRACKS:
		    vb_appends(&r, " TRACKS");
		    break;
		case CYLINDERS:
		    vb_appends(&r, " CYLINDERS");
		    break;
		case AVBLOCK:
		    vb_appends(&r, " AVBLOCK");
		    break;
		default:
		    break;
		}
		if (tp[PARM_PRIMARY_SPACE].value != NULL) {
		    vb_appendf(&r, " SPACE(%s", tp[PARM_PRIMARY_SPACE].value);
		    if (tp[PARM_SECONDARY_SPACE].value) {
			vb_appendf(&r, ",%s", tp[PARM_SECONDARY_SPACE].value);
		    }
		    vb_appends(&r, ")");
		}
	    }
	} else if (ft_private.host_type == HT_VM) {
	    if (ft_private.recfm != DEFAULT_RECFM) {
		vb_appends(&r, " RECFM ");
		switch (ft_private.recfm) {
		case RECFM_FIXED:
		    vb_appends(&r, "F");
		    break;
		case RECFM_VARIABLE:
		    vb_appends(&r, "V");
		    break;
		default:
		    break;
		};

		if (tp[PARM_LRECL].value) {
		    vb_appendf(&r, " LRECL %s", tp[PARM_LRECL].value);
		}
	    }
	}
    }
    vb_appends(&r, "\\n");

    /* Erase the line and enter the command. */
    flen = kybd_prime();
    if (!flen || flen < vb_len(&r) - 1) {
	vb_free(&r);
	if (ft_local_file != NULL) {
	    fclose(ft_local_file);
	    ft_local_file = NULL;
	    if (ft_private.receive_flag && !ft_private.append_flag) {
		unlink(ft_local_filename);
	    }
	}
	popup_an_error("%s", get_message("ftUnable"));
	return False;
    }
    (void) emulate_input(vb_buf(&r), vb_len(&r), False);
    vb_free(&r);

    ft_gui_awaiting();

    /* Get this thing started. */
    ft_start_id = AddTimeOut(10 * 1000, ft_didnt_start);
    ft_state = FT_AWAIT_ACK;
    ft_private.is_cut = False;

    return True;
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
int
ft_ebcdic_to_multibyte(ebc_t ebc, char mb[], int mb_len)
{
    int local_cp = appres.local_cp;
    int rc;

    appres.local_cp = ft_windows_codepage;
    rc = ebcdic_to_multibyte(ebc, mb, mb_len);
    appres.local_cp = local_cp;
    return rc;
}

int
ft_unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len)
{
    int local_cp = appres.local_cp;
    int rc;

    appres.local_cp = ft_windows_codepage;
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

    appres.local_cp = ft_windows_codepage;
    rc = multibyte_to_unicode(mb, mb_len, consumedp, errorp);
    appres.local_cp = local_cp;
    return rc;
}

#endif /*]*/
