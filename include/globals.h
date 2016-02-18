/*
 * Copyright (c) 1993-2016 Paul Mattes.
 * Copyright (c) 2005, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes nor the
 *       names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL AND JEFF SPARKES
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, DON RUSSELL OR JEFF
 * SPARKES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	globals.h
 *		Common definitions for x3270, c3270, s3270 and tcl3270.
 */

/* Autoconf settings. */
#include "conf.h"			/* autoconf settings */
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
# define BLOCKING_CONNECT_ONLY	1
#endif /*]*/

#if defined(apollo) /*[*/
# define BLOCKING_CONNECT_ONLY	1
#endif /*]*/

/*
 * Compiler-specific #defines.
 */

/* '_is_unused' explicitly flags an unused parameter */
#if defined(__GNUC__) /*[*/
# define _is_unused __attribute__((__unused__))
# define printflike(s,f) __attribute__ ((__format__ (__printf__, s, f)))
#else /*][*/
# define _is_unused /* nothing */
# define printflike(s, f) /* nothing */
#endif /*]*/
#if 'A' > 'a' /*[*/
# define EBCDIC_HOST 1
#endif /*]*/

/*
 * Prerequisite #includes.
 */
#include <stdio.h>			/* Unix standard I/O library */
#include <stdlib.h>			/* Other Unix library functions */
#if !defined(_MSC_VER) /*[*/
# include <unistd.h>			/* Unix system calls */
#endif /*]*/
#include <ctype.h>			/* Character classes */
#include <string.h>			/* String manipulations */
#include <stdint.h>			/* Integer types */
#include <sys/types.h>			/* Basic system data types */
#if !defined(_WIN32) /*[*/
# include <sys/socket.h>		/* Socket data types */
#endif /*]*/
#if !defined(_MSC_VER) /*[*/
# include <sys/time.h>			/* System time-related data types */
#endif /*]*/
#include <time.h>			/* C library time functions */
#include <stdarg.h>			/* variable argument lists */
#if !defined(_MSC_VER) /*[*/
# include <stdbool.h>			/* bool, true, false */
#else /*][*/
typedef char bool;			/* roll our own for MSC */
# define true 1
# define false 0
#endif /*]*/
#if defined(_WIN32) /*[*/
# include "wincmn.h"			/* Common Windows definitions */
#endif /*]*/
#include "localdefs.h"			/* {s,tcl,c}3270-specific defines */

/*
 * Locale-related definitions.
 * Note that USE_ICONV can be used to override __STDC_ISO_10646__, so that
 * development of iconv-based logic can be done on 10646-compliant systems.
 */
#if defined(__STDC_ISO_10646__) && !defined(USE_ICONV) /*[*/
# define UNICODE_WCHAR	1
#endif /*]*/
#if !defined(_WIN32) && !defined(UNICODE_WCHAR) /*[*/
# undef USE_ICONV
# define USE_ICONV 1
# include <iconv.h>
#endif /*]*/

/*
 * Unicode UCS-4 characters are (hopefully) 32 bits.
 * EBCDIC (including DBCS) is (hopefully) 16 bits.
 */
typedef unsigned int ucs4_t;
typedef unsigned short ebc_t;

/*
 * Cancel out contradictory parts.
 */
#if defined(C3270) && defined(X3270_DBCS) && !defined(CURSES_WIDE) && !defined(_WIN32) /*[*/
# undef X3270_DBCS
#endif /*]*/
#if defined(X3270_IPV6) && !defined(AF_INET6) /*[*/
# undef X3270_IPV6
#endif /*]*/

/* Local process (-e) header files. */
#if defined(X3270_LOCAL_PROCESS) && defined(HAVE_FORKPTY) /*[*/
# define LOCAL_PROCESS	1
# include <termios.h>
# if defined(HAVE_PTY_H) /*[*/
#  include <pty.h>
# endif /*]*/
# if defined(HAVE_LIBUTIL_H) /*[*/
#  include <libutil.h>
# endif /*]*/
# if defined(HAVE_UTIL_H) /*[*/
#  include <util.h>
# endif /*]*/
#endif /*]*/

/* Stop conflicting with curses' COLS, even if we don't link with it. */
#define COLS cCOLS

/* Memory allocation. */
void *Malloc(size_t);
void Free(void *);
void *Calloc(size_t, size_t);
void *Realloc(void *, size_t);
char *NewString(const char *);

/* Error exits. */
void Error(const char *);
void Warning(const char *);

/* A key symbol. */
typedef unsigned long ks_t;
#define KS_NONE 0L

/* Host flags. */
typedef enum {
    ANSI_HOST,		/* A:, now a no-op */
    NO_LOGIN_HOST,	/* C: */
    SSL_HOST,		/* L: */
    NON_TN3270E_HOST,	/* N: */
    PASSTHRU_HOST,	/* P: */
    STD_DS_HOST,	/* S: */
    BIND_LOCK_HOST	/* B:, now a no-op */
} host_flags_t;
#define HOST_FLAG(t)	(host_flags & (1 << t))

/* Simple global variables */

extern int		COLS;		/* current */
extern int		ROWS;
extern int		maxCOLS;	/* maximum */
extern int		maxROWS;
extern int		defROWS;	/* default (EraseWrite) */
extern int		defCOLS;
extern int		altROWS;	/* alternate (EraseWriteAlternate) */
extern int		altCOLS;
extern const char	*app;
extern const char	*build;
extern const char	*cyear;
extern const char	*build_rpq_timestamp;
extern const char 	*build_rpq_version;
extern int		children;
extern char		*connected_lu;
extern char		*connected_type;
extern char		*current_host;
extern unsigned short	current_port;
extern bool		dbcs;
extern char		*efontname;
extern bool		ever_3270;
extern bool		exiting;
extern bool		flipped;
extern char		*full_current_host;
extern char		*full_efontname;
extern char		*full_efontname_dbcs;
extern char		full_model_name[];
extern bool		*funky_font;
extern char		*hostname;
extern unsigned		host_flags;
extern char		luname[];
#if defined(LOCAL_PROCESS) /*[*/
extern bool		local_process;
#endif /*]*/
extern char		*model_name;
extern int		model_num;
extern bool		non_tn3270e_host;
extern int		ov_cols, ov_rows;
extern bool		ov_auto;
extern char		*profile_name;
extern const char	*programname;
extern char		*qualified_host;
extern char		*reconnect_host;
extern int		screen_depth;
extern bool		scroll_initted;
#if defined(HAVE_LIBSSL) /*[*/
extern bool		secure_connection;
extern bool		secure_unverified;
extern char		**unverified_reasons;
#endif /*]*/
extern bool		shifted;
extern bool		*standard_font;
extern char		*termtype;
extern bool		visible_control;
extern int		*xtra_width;
extern int		x3270_exit_code;

#if defined(_WIN32) /*[*/
extern char		*instdir;
extern char		*mydesktop;
extern char		*mydocs3270;
extern char		*commondocs3270;
#endif /*]*/

#if defined(_WIN32) /*[*/
extern unsigned		windirs_flags;
#endif /*]*/

/* Data types and complex global variables */

/*   connection state */
enum cstate {
    NOT_CONNECTED,	/* no socket, unknown mode */
    RESOLVING,		/* resolving hostname */
    PENDING,		/* socket connection pending */
    NEGOTIATING,	/* SSL/proxy negotiation in progress */
    CONNECTED_INITIAL,	/* connected, no 3270 mode yet */
    CONNECTED_NVT,	/* connected in NVT mode */
    CONNECTED_3270,	/* connected in old-style 3270 mode */
    CONNECTED_UNBOUND,	/* connected in TN3270E mode, unbound */
    CONNECTED_E_NVT,	/* connected in TN3270E mode, NVT mode */
    CONNECTED_SSCP,	/* connected in TN3270E mode, SSCP-LU mode */
    CONNECTED_TN3270E	/* connected in TN3270E mode, 3270 mode */
};
extern enum cstate cstate;

#define PCONNECTED	((int)cstate >= (int)RESOLVING)
#define HALF_CONNECTED	(cstate == RESOLVING || cstate == PENDING)
#define CONNECTED	((int)cstate >= (int)CONNECTED_INITIAL)
#define IN_NEITHER	(cstate == NEGOTIATING || cstate == CONNECTED_INITIAL)
#define IN_NVT		(cstate == CONNECTED_NVT || cstate == CONNECTED_E_NVT)
#define IN_3270		(cstate == CONNECTED_3270 || cstate == CONNECTED_TN3270E || cstate == CONNECTED_SSCP)
#define IN_SSCP		(cstate == CONNECTED_SSCP)
#define IN_TN3270E	(cstate == CONNECTED_TN3270E)
#define IN_E		(cstate >= CONNECTED_UNBOUND)

/*   keyboard modifer bitmap */
#define ShiftKeyDown	0x01
#define MetaKeyDown	0x02
#define AltKeyDown	0x04

/*   toggles */
typedef enum {
    MONOCASE,		/* all-uppercase display */
    ALT_CURSOR,		/* block cursor (x3270) */
    CURSOR_BLINK,	/* blinking cursor (x3270) */
    SHOW_TIMING,	/* display command execution time in the OIA
			   (interactive) */
    CURSOR_POS,		/* display cursor position in the OIA (interactive) */
    TRACING,		/* trace data and events */
    SCROLL_BAR,		/* include scroll bar (x3270) */
    LINE_WRAP,		/* NVT xterm line-wrap mode (auto-wraparound) */
    BLANK_FILL,		/* treat trailing blanks like NULLs on input */
    SCREEN_TRACE,	/* trace screen contents to file or printer */
    MARGINED_PASTE,	/* respect left margin when pasting (x3270 and
			   wc3270) */
    RECTANGLE_SELECT,	/* select by rectangles (x3270) */
    CROSSHAIR,		/* display cursor crosshair (x3270) */
    VISIBLE_CONTROL,	/* display visible control characters (x3270) */
    AID_WAIT,		/* make scripts wait for AIDs to complete */
    UNDERSCORE,		/* special c3270/wc3270 underscore display mode
			   (c3270 and wc320) */
    OVERLAY_PASTE,	/* overlay protected fields when pasting (x3270 and
			   wc3270) */
    N_TOGGLES
} toggle_index_t;
bool toggled(toggle_index_t ix);

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

/*   input key type */
enum keytype { KT_STD, KT_GE };

/* Shorthand macros */

#define Replace(var, value) do { Free(var); var = (value); } while(false)

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
# define SETLINEBUF(s)	setvbuf(s, NULL, _IOLBF, BUFSIZ)
#else /*][*/
# define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

/* Default DFT file transfer buffer size. */
#if !defined(DFT_BUF) /*[*/
# define DFT_BUF		(4 * 1024)
#endif /*]*/

/* DBCS Preedit Types */
#define PT_ROOT		"Root"
#define PT_OVER_THE_SPOT	"OverTheSpot"
#define PT_OFF_THE_SPOT	"OffTheSpot"
#define PT_ON_THE_SPOT		"OnTheSpot"

/* I/O typedefs */
#if !defined(_WIN32) /*[*/
typedef int iosrc_t;
# define INVALID_IOSRC	(-1)
#else /*][*/
typedef HANDLE iosrc_t;
# define INVALID_IOSRC	INVALID_HANDLE_VALUE
#endif /*]*/
typedef uintptr_t ioid_t;
#define NULL_IOID	0L

/* Screen print types. */
typedef enum { P_TEXT, P_HTML, P_RTF, P_GDI } ptype_t;

/* Usage message with error exit. */
void usage(const char *);

/* Emulator actions. */
/* types of internal actions */
typedef enum iaction {
    IA_STRING, IA_PASTE, IA_REDRAW, IA_KEYPAD, IA_DEFAULT, IA_KEY, IA_MACRO,
    IA_SCRIPT, IA_PEEK, IA_TYPEAHEAD, IA_FT, IA_COMMAND, IA_KEYMAP, IA_IDLE
} ia_t;
extern enum iaction ia_cause;

typedef bool (action_t)(ia_t ia, unsigned argc, const char **argv);

/* Common socket definitions. */
#if !defined(_WIN32) /*[*/
typedef int socket_t;
# define INVALID_SOCKET  (-1)
# define INET_ADDR_T	in_addr_t
# define SOCK_CLOSE(s)  close(s)
# define socket_errno()	errno
# define SE_EWOULDBLOCK	EWOULDBLOCK
#else /*][*/
typedef SOCKET socket_t;
# define INET_ADDR_T	unsigned long
# define SOCK_CLOSE(s)  closesocket(s)
# define socket_errno()	WSAGetLastError()
# define SE_EWOULDBLOCK WSAEWOULDBLOCK
#endif /*]*/

/* Handy stuff. */
#define array_count(a)	sizeof(a)/sizeof(a[0])

/* Doubly-linked lists. */
typedef struct llist {
    struct llist *next;
    struct llist *prev;
} llist_t;
