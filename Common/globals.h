/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002,
 *  2003, 2004, 2005, 2006 by Paul Mattes.
 * RPQNAMES modifications copyright 2005 by Don Russell.
 * Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	globals.h
 *		Common definitions for x3270, c3270, s3270 and tcl3270.
 */

/* Autoconf settings. */
#include "conf.h"			/* autoconf settings */
#if defined(X3270_TN3270E) && !defined(X3270_ANSI) /*[*/
#define X3270_ANSI	1	/* RFC2355 requires NVT mode */
#endif /*]*/
#if defined(HAVE_VASPRINTF) && !defined(_GNU_SOURCE) /*[*/
#define _GNU_SOURCE		/* vasprintf isn't POSIX */
#endif /*]*/

/*
 * OS-specific #defines.  Except for the blocking-connect workarounds, these
 * should be replaced with autoconf probes as soon as possible.
 */

/*
 * BLOCKING_CONNECT_ONLY
 *   Use only blocking sockets.
 */
#if defined(sco) /*[*/
#define BLOCKING_CONNECT_ONLY	1
#endif /*]*/

#if defined(apollo) /*[*/
#define BLOCKING_CONNECT_ONLY	1
#endif /*]*/

/*
 * Compiler-specific #defines.
 */

/* 'unused' explicitly flags an unused parameter */
#if defined(__GNUC__) /*[*/
#define unused __attribute__((__unused__))
#define printflike(s,f) __attribute__ ((__format__ (__printf__, s, f)))
#else /*][*/
#define unused /* nothing */
#define printflike(s, f) /* nothing */
#endif /*]*/



/*
 * Prerequisite #includes.
 */
#include <stdio.h>			/* Unix standard I/O library */
#include <stdlib.h>			/* Other Unix library functions */
#include <unistd.h>			/* Unix system calls */
#include <ctype.h>			/* Character classes */
#include <string.h>			/* String manipulations */
#include <sys/types.h>			/* Basic system data types */
#include <sys/time.h>			/* System time-related data types */
#include <time.h>			/* C library time functions */
#include "localdefs.h"			/* {s,tcl,c}3270-specific defines */

/*
 * Cancel out contradictory parts.
 */
#if !defined(X3270_DISPLAY) /*[*/
#undef X3270_KEYPAD
#undef X3270_MENUS
#endif /*]*/

/* Local process (-e) header files. */
#if defined(X3270_LOCAL_PROCESS) && defined(HAVE_LIBUTIL) /*[*/
#define LOCAL_PROCESS	1
#include <termios.h>
#if defined(HAVE_PTY_H) /*[*/
#include <pty.h>
#endif /*]*/
#if defined(HAVE_LIBUTIL_H) /*[*/
#include <libutil.h>
#endif /*]*/
#if defined(HAVE_UTIL_H) /*[*/
#include <util.h>
#endif /*]*/
#endif /*]*/

/* Functions we may need to supply. */
#if defined(NEED_STRTOK_R) /*[*/
extern char *strtok_r(char *str, const char *sep, char **last);
#endif /*]*/

/* Stop conflicting with curses' COLS, even if we don't link with it. */
#define COLS cCOLS

/* Simple global variables */

extern int		COLS;
extern int		ROWS;
#if defined(X3270_DISPLAY) /*[*/
extern Atom		a_3270, a_registry, a_encoding;
extern XtAppContext	appcontext;
#endif /*]*/
extern const char	*build;
extern const char	*build_rpq_timestamp;
extern const char 	*build_rpq_version;
extern int		children;
extern char		*connected_lu;
extern char		*connected_type;
extern char		*current_host;
extern unsigned short	current_port;
#if defined(X3270_DBCS) /*[*/
extern Boolean		dbcs;
#endif /*]*/
#if defined(X3270_FT) /*[*/
extern int		dft_buffersize;
#endif /*]*/
extern char		*efontname;
extern Boolean		ever_3270;
extern Boolean		exiting;
#if defined(X3270_DISPLAY) /*[*/
extern Boolean		*extended_3270font;
extern Font		*fid;
extern Boolean		*font_8bit;
#endif /*]*/
extern Boolean		flipped;
extern char		*full_current_host;
extern char		*full_efontname;
#if defined(X3270_DBCS) /*[*/
extern char		*full_efontname_dbcs;
#endif /*]*/
extern char		full_model_name[];
extern char		*funky_font;
extern char		*hostname;
#if defined(X3270_DBCS) /*[*/
extern char		*local_encoding;
#if defined(X3270_DISPLAY) /*[*/
extern char		*locale_name;
#endif /*]*/
#endif /*]*/
extern char		luname[];
#if defined(LOCAL_PROCESS) /*[*/
extern Boolean		local_process;
#endif /*]*/
extern int		maxCOLS;
extern int		maxROWS;
extern char		*model_name;
extern int		model_num;
extern Boolean		no_login_host;
extern Boolean		non_tn3270e_host;
extern int		ov_cols, ov_rows;
extern Boolean		passthru_host;
extern const char	*programname;
extern char		*qualified_host;
extern char		*reconnect_host;
extern int		screen_depth;
extern Boolean		scroll_initted;
#if defined(HAVE_LIBSSL) /*[*/
extern Boolean		secure_connection;
#endif /*]*/
extern Boolean		shifted;
extern Boolean		ssl_host;
extern Boolean		*standard_font;
extern Boolean		std_ds_host;
extern char		*termtype;
extern Widget		toplevel;
extern Boolean		visible_control;
extern int		*xtra_width;

#if defined(X3270_DISPLAY) /*[*/
extern Atom		a_delete_me;
extern Atom		a_save_yourself;
extern Atom		a_state;
extern Display		*display;
extern Pixmap		gray;
extern Pixel		keypadbg_pixel;
extern XrmDatabase	rdb;
extern Window		root_window;
extern char		*user_title;
extern unsigned char	xk_selector;
#endif /*]*/

#if defined(C3270) && defined(_WIN32) /*[*/
extern char		*instdir;
extern char		myappdata[];
#endif /*]*/

/* Data types and complex global variables */

/*   connection state */
enum cstate {
	NOT_CONNECTED,		/* no socket, unknown mode */
	RESOLVING,		/* resolving hostname */
	PENDING,		/* connection pending */
	CONNECTED_INITIAL,	/* connected, no mode yet */
	CONNECTED_ANSI,		/* connected in NVT ANSI mode */
	CONNECTED_3270,		/* connected in old-style 3270 mode */
	CONNECTED_INITIAL_E,	/* connected in TN3270E mode, unnegotiated */
	CONNECTED_NVT,		/* connected in TN3270E mode, NVT mode */
	CONNECTED_SSCP,		/* connected in TN3270E mode, SSCP-LU mode */
	CONNECTED_TN3270E	/* connected in TN3270E mode, 3270 mode */
};
extern enum cstate cstate;

#define PCONNECTED	((int)cstate >= (int)RESOLVING)
#define HALF_CONNECTED	(cstate == RESOLVING || cstate == PENDING)
#define CONNECTED	((int)cstate >= (int)CONNECTED_INITIAL)
#define IN_NEITHER	(cstate == CONNECTED_INITIAL)
#define IN_ANSI		(cstate == CONNECTED_ANSI || cstate == CONNECTED_NVT)
#define IN_3270		(cstate == CONNECTED_3270 || cstate == CONNECTED_TN3270E || cstate == CONNECTED_SSCP)
#define IN_SSCP		(cstate == CONNECTED_SSCP)
#define IN_TN3270E	(cstate == CONNECTED_TN3270E)
#define IN_E		(cstate >= CONNECTED_INITIAL_E)

/*   keyboard modifer bitmap */
#define ShiftKeyDown	0x01
#define MetaKeyDown	0x02
#define AltKeyDown	0x04

/*   toggle names */
struct toggle_name {
	const char *name;
	int index;
};
extern struct toggle_name toggle_names[];

/*   extended attributes */
struct ea {
	unsigned char cc;	/* EBCDIC or ASCII character code */
	unsigned char fa;	/* field attribute, it nonzero */
	unsigned char fg;	/* foreground color (0x00 or 0xf<n>) */
	unsigned char bg;	/* background color (0x00 or 0xf<n>) */
	unsigned char gr;	/* ANSI graphics rendition bits */
	unsigned char cs;	/* character set (GE flag, or 0..2) */
	unsigned char ic;	/* input control (DBCS) */
	unsigned char db;	/* DBCS state */
};
#define GR_BLINK	0x01
#define GR_REVERSE	0x02
#define GR_UNDERLINE	0x04
#define GR_INTENSIFY	0x08

#define CS_MASK		0x03	/* mask for specific character sets */
#define CS_BASE		0x00	/*  base character set (X'00') */
#define CS_APL		0x01	/*  APL character set (X'01' or GE) */
#define CS_LINEDRAW	0x02	/*  DEC line-drawing character set (ANSI) */
#define CS_DBCS		0x03	/*  DBCS character set (X'F8') */
#define CS_GE		0x04	/* cs flag for Graphic Escape */

/*   translation lists */
struct trans_list {
	char			*name;
	char			*pathname;
	Boolean			is_temp;
	Boolean			from_server;
	struct trans_list	*next;
};
extern struct trans_list *trans_list;

/*   input key type */
enum keytype { KT_STD, KT_GE };

/*   state changes */
#define ST_RESOLVING	1
#define ST_HALF_CONNECT	2
#define ST_CONNECT	3
#define ST_3270_MODE	4
#define ST_LINE_MODE	5
#define ST_REMODEL	6
#define ST_PRINTER	7
#define ST_EXITING	8
#define ST_CHARSET	9
#define N_ST		10

/* Naming convention for private actions. */
#define PA_PFX	"PA-"

/* Shorthand macros */

#define CN	((char *) NULL)
#define PN	((XtPointer) NULL)
#define Replace(var, value) { Free(var); var = (value); }

/* Configuration change masks. */
#define NO_CHANGE	0x0000	/* no change */
#define MODEL_CHANGE	0x0001	/* screen dimensions changed */
#define FONT_CHANGE	0x0002	/* emulator font changed */
#define COLOR_CHANGE	0x0004	/* color scheme or 3278/9 mode changed */
#define SCROLL_CHANGE	0x0008	/* scrollbar snapped on or off */
#define CHARSET_CHANGE	0x0010	/* character set changed */
#define ALL_CHANGE	0xffff	/* everything changed */

/* Portability macros */

/*   Equivalent of setlinebuf */

#if defined(_IOLBF) /*[*/
#define SETLINEBUF(s)	setvbuf(s, (char *)NULL, _IOLBF, BUFSIZ)
#else /*][*/
#define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

/*   Motorola version of gettimeofday */

#if defined(MOTOROLA)
#define gettimeofday(tp,tz)	gettimeofday(tp)
#endif

/* Default DFT file transfer buffer size. */
#if defined(X3270_FT) && !defined(DFT_BUF) /*[*/
#define DFT_BUF		(4 * 1024)
#endif /*]*/

/* DBCS Preedit Types */
#if defined(X3270_DBCS) /*[*/
#define PT_ROOT			"Root"
#define PT_OVER_THE_SPOT	"OverTheSpot"
#define PT_OFF_THE_SPOT		"OffTheSpot"
#define PT_ON_THE_SPOT		"OnTheSpot"
#endif /*]*/

