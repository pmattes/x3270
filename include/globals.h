/*
 * Copyright (c) 1993-2024 Paul Mattes.
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

#if defined(_WIN32) /*[*/
/* Needs to happen before any system includes, so MinGW doesn't default it */
# define _WIN32_WINNT	0x0600
#endif /*]*/

/*
 * Compiler-specific #defines.
 */

/*
 * '_is_unused' explicitly flags an unused parameter
 *
 * 'printflike' identifies printf-like functions.
 * The conditional strangeness is because recent versions of MinGW sometimes
 * force GNU-style definitions for the PRI*64 constants, so this definition has
 * to use the same condition, and it depends on <stdint.h>. Sigh.
 */
#if defined(__GNUC__) /*[*/
# include <stdint.h>
# define _is_unused __attribute__((__unused__))
# if defined(_UCRT) || __USE_MINGW_ANSI_STDIO /*[ per MinGW inttypes.h */
#  define printflike(s,f) __attribute__ ((__format__ (__gnu_printf__, s, f)))
# else /*][*/
#  define printflike(s,f) __attribute__ ((__format__ (__printf__, s, f)))
# endif /*]*/
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
 * Common struct typedefs.
 */
typedef struct json json_t;
typedef struct cmd cmd_t;

/*
 * Helpful macros.
 */
#define STR_HELPER(x)	#x
#define STR(x)		STR_HELPER(x)

/*
 * Unicode UCS-4 characters are (hopefully) 32 bits.
 * EBCDIC (including DBCS) is (hopefully) 16 bits.
 */
typedef unsigned int ucs4_t;
typedef unsigned short ebc_t;

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

/* Check for some version of 'start' or 'open'. */
#if defined(_WIN32) || defined(linux) || defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__) /*[*/
#define HAVE_START	1
#endif /*]*/

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
#define HOST_FLAG(t)	HOST_nFLAG(host_flags, t)

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
extern bool		*funky_font;
extern char		*hostname;
extern unsigned		host_flags;
extern char		*host_user;
extern char		luname[];
#if defined(LOCAL_PROCESS) /*[*/
extern bool		local_process;
#endif /*]*/
extern int		model_num;
extern bool		mode3279;
extern bool		non_tn3270e_host;
extern int		ov_cols, ov_rows;
extern bool		ov_auto;
extern char		*profile_name;
extern const char	*programname;
extern char		*qualified_host;
extern char		*reconnect_host;
extern int		screen_depth;
extern bool		scroll_initted;
extern bool		shifted;
extern bool		*standard_font;
extern bool		supports_cmdline_host;
extern char		*termtype;
extern bool		visible_control;
extern int		*xtra_width;
extern int		x3270_exit_code;
extern bool		x3270_exiting;
extern char		*security_cookie;

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

/**   connection state */
enum cstate {
    NOT_CONNECTED,	/**< no socket, unknown mode */
    RECONNECTING,	/**< delay before automatic reconnect */
    TLS_PASS,		/**< waiting for interactive TLS password */

    /* Half-connected states. */
    RESOLVING,		/**< resolving hostname */
    TCP_PENDING,	/**< socket connection pending */
    TLS_PENDING,	/**< TLS negotiation pending */
    PROXY_PENDING,	/**< proxy negotiation pending */
    TELNET_PENDING,	/**< TELNET negotiation pending */

    /* Connected states. */
    CONNECTED_NVT,	/**< connected in NVT mode */
    CONNECTED_NVT_CHAR,	/**< connected in NVT character-at-a-time mode */
    CONNECTED_3270,	/**< connected in RFC 1576 TN3270 mode */
    CONNECTED_UNBOUND,	/**< connected in TN3270E mode, unbound */
    CONNECTED_E_NVT,	/**< connected in TN3270E mode, NVT mode */
    CONNECTED_SSCP,	/**< connected in TN3270E mode, SSCP-LU mode */
    CONNECTED_TN3270E,	/**< connected in TN3270E mode, 3270 mode */
    NUM_CSTATE		/**< number of cstates */
};
extern enum cstate cstate; /**< connection state */

#define cPCONNECTED(c)	(c > NOT_CONNECTED)
#define cHALF_CONNECTED(c) (c >= RESOLVING && c < CONNECTED_NVT)
#define cCONNECTED(c)	(c > TCP_PENDING)
#define cIN_NVT(c)	(c == CONNECTED_NVT || \
			 c == CONNECTED_NVT_CHAR || \
			 c == CONNECTED_E_NVT)
#define cIN_3270(c)	(c == CONNECTED_3270 || \
			 c == CONNECTED_TN3270E || \
			 c == CONNECTED_SSCP)
#define cIN_SSCP(c)	(c == CONNECTED_SSCP)
#define cIN_TN3270E(c)	(c == CONNECTED_TN3270E)
#define cIN_E(c)	(c >= CONNECTED_UNBOUND)
#define cFULL_SESSION(c) (cIN_NVT(c) || cIN_3270(c))
#define cIN_E_NVT(c) 	(c == CONNECTED_E_NVT)

#define PCONNECTED	cPCONNECTED(cstate)
#define HALF_CONNECTED	cHALF_CONNECTED(cstate)
#define CONNECTED	cCONNECTED(cstate)
#define IN_NVT		cIN_NVT(cstate)
#define IN_3270		cIN_3270(cstate)
#define IN_SSCP		cIN_SSCP(cstate)
#define IN_TN3270E	cIN_TN3270E(cstate)
#define IN_E		cIN_E(cstate)
#define FULL_SESSION	cFULL_SESSION(cstate)
#define IN_E_NVT	cIN_E_NVT(cstate)

/*   network connection status */
typedef enum {
    NC_FAILED,		/* failed */
    NC_RESOLVING,	/* name resolution in progress */
    NC_TLS_PASS,	/* TLS password pending */
    NC_CONNECT_PENDING,	/* connection pending */
    NC_CONNECTED	/* connected */
} net_connect_t;

/*   toggles */
typedef enum {
    MONOCASE,		/* all-uppercase display */
    ALT_CURSOR,		/* block cursor */
    CURSOR_BLINK,	/* blinking cursor */
    SHOW_TIMING,	/* display command execution time in the OIA */
    TRACING,		/* trace data and events */
    SCROLL_BAR,		/* include scroll bar */
    LINE_WRAP,		/* NVT xterm line-wrap mode (auto-wraparound) */
    BLANK_FILL,		/* treat trailing blanks like NULLs on input */
    SCREEN_TRACE,	/* trace screen contents to file or printer */
    MARGINED_PASTE,	/* respect left margin when pasting */
    RECTANGLE_SELECT,	/* select by rectangles */
    CROSSHAIR,		/* display cursor crosshair */
    VISIBLE_CONTROL,	/* display visible control characters */
    AID_WAIT,		/* make scripts wait for AIDs to complete */
    UNDERSCORE,		/* special c3270/wc3270 underscore display mode */
    OVERLAY_PASTE,	/* overlay protected fields when pasting */
    TYPEAHEAD,		/* typeahead */
    APL_MODE,		/* APL mode */
    ALWAYS_INSERT,	/* always-insert mode */
    RIGHT_TO_LEFT,	/* right-to-left display */
    REVERSE_INPUT,	/* reverse input */
    INSERT_MODE,	/* insert mode */
    SELECT_URL,		/* double-click on a URL opens the browser */
    UNDERSCORE_BLANK_FILL, /* treat trailing underscores as blanks when
			      BLANK_FILL is on */
    N_TOGGLES
} toggle_index_t;
bool toggled(toggle_index_t ix);

/*
 * ea.ucs4 (Unicode) will be non-zero if the buffer location was set in NVT
 *  mode.
 * ea.ec (EBCDIC) will be non-zero if the buffer location was set in 3270 mode.
 * They will *never* both be non-zero.
 *
 * We translate between the two values as needed for display or for the Ascii()
 * action, but when getting a raw buffer dump via ReadBuffer(Ebcdic) or sending
 * the buffer to the host (Read Modified), we only send EBCDIC: If there is
 * Unicode in a buffer location, we consider it an EBCDIC X'00' (NUL).
 *
 * Note that the right-hand location of a DBCS pair is ec=0 in 3270 mode, but
 * ucs4=0x20 (a space) in NVT mode.
 */

/*   extended attributes */
struct ea {
    unsigned char ec;	/* EBCDIC code */
    unsigned char fa;	/* field attribute, if nonzero */
    unsigned char fg;	/* foreground color (0x00 or 0xf<n>) */
    unsigned char bg;	/* background color (0x00 or 0xf<n>) */
    unsigned char gr;	/* ANSI graphics rendition bits */
    unsigned char cs;	/* character set (GE flag, or 0..2) */
    unsigned char ic;	/* input control (DBCS) */
    unsigned char db;	/* DBCS state */
    ucs4_t ucs4;	/* Unicode value, if set in NVT mode */
};
#define GR_BLINK	0x01
#define GR_REVERSE	0x02
#define GR_UNDERLINE	0x04
#define GR_INTENSIFY	0x08
#define GR_WRAP		0x10	/* NVT-mode wrap occurred after this position */
#define GR_RESET	0x20	/* ignore preceding field attribute */

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
#define NO_CHANGE       0x0000  /* no change */
#define MODEL_CHANGE    0x0001  /* screen dimensions changed */
#define FONT_CHANGE     0x0002  /* emulator font changed */
#define COLOR_CHANGE    0x0004  /* color scheme or 3278/9 mode changed */
#define SCROLL_CHANGE   0x0008  /* scrollbar snapped on or off */
#define CODEPAGE_CHANGE 0x0010  /* code page changed */
#define ALL_CHANGE      0xffff  /* everything changed */

/* Portability macros */

/*   Equivalent of setlinebuf */

#if defined(_IOLBF) /*[*/
# define SETLINEBUF(s)	setvbuf(s, NULL, _IOLBF, BUFSIZ)
#else /*][*/
# define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

/* Default DFT file transfer buffer size. */
#if !defined(DFT_BUF) /*[*/
# define DFT_BUF	16384
#endif /*]*/
#define DFT_MIN_BUF	256
#define DFT_MAX_BUF	32767

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
typedef uintptr_t ioid_t;	/**< An I/O callback identifier. */
#define NULL_IOID	0L	/**< An empty @ref ioid_t. */

/* Screen print types. */
typedef enum { P_NONE, P_TEXT, P_HTML, P_RTF, P_GDI } ptype_t;

/* Usage message with error exit. */
void usage(const char *);

/* Emulator actions. */
/* types of internal actions */
typedef enum iaction {
    IA_INVALID = -1, IA_NONE, IA_STRING, IA_PASTE, IA_REDRAW, IA_KEYPAD, IA_DEFAULT, IA_MACRO,
    IA_SCRIPT, IA_PEEK, IA_TYPEAHEAD, IA_FT, IA_COMMAND, IA_KEYMAP, IA_IDLE,
    IA_PASSWORD, IA_UI, IA_HTTPD
} ia_t;
#define IA_IS_KEY(ia)	\
    ((ia) == IA_KEYPAD || (ia) == IA_KEYMAP || (ia) == IA_DEFAULT)
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
# if !defined(WSA_FLAG_NO_HANDLE_INHERIT) /*[*/
#  define WSA_FLAG_NO_HANDLE_INHERIT 0x80
# endif /*]*/
#endif /*]*/

/* Handy stuff. */
#define array_count(a)	(sizeof(a)/sizeof(a[0]))

/* Doubly-linked lists. */
typedef struct llist {
    struct llist *next;
    struct llist *prev;
} llist_t;

/* Resource types. */
enum resource_type {
  XRM_STRING,     /* char * */
  XRM_BOOLEAN,    /* bool */
  XRM_INT         /* int */
};

/* Error type for popup_an_xerror(). */
typedef enum {
    ET_CONNECT,
    ET_OTHER
} pae_t;

