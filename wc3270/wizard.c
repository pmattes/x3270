/*
 * Copyright (c) 2006-2024 Paul Mattes.
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
 *	wizard.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Session creation wizard
 */

#include "globals.h"

#include <signal.h>
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "ctlrc.h"
#include "host.h"
#include "proxy_names.h"
#include "resources.h"
#include "screen.h"
#include "trace.h"
#include "utils.h"

#include <wincon.h>
#include <lmcons.h>
#include <winspool.h>
#include <commdlg.h>
#include <sys/stat.h>

#include "winvers.h"
#include "shortcutc.h"
#include "windirs.h"
#include "relinkc.h"

#define LEGAL_CNAME	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcedfghijklmnopqrstuvwxyz" \
			"0123456789_- "

#define KEYMAP_SUFFIX	".wc3270km"
#define KS_LEN		strlen(KEYMAP_SUFFIX)

#define KM_3270		".3270"
#define LEN_3270	strlen(KM_3270)

#define KM_NVT		".nvt"
#define LEN_NVT		strlen(KM_NVT)

#define KM_DESC		"!description: "
#define LEN_DESC	strlen(KM_DESC)

#define SESS_SUFFIX	".wc3270"
#define SESS_LEN	strlen(SESS_SUFFIX)

#define CHOICE_NONE	"none"
#define DISPLAY_NONE	"(none)"

#define DONE_FILE	"migrated"

enum {
    MN_NONE = 0,
    MN_HOST,		/* host name */
    MN_LU,		/* logical unit */
    MN_PORT,		/* TCP port */
    MN_MODEL,		/* model number */
    MN_OVERSIZE,	/* oversize */
    MN_CODEPAGE,	/* code page */
    MN_CROSSHAIR,	/* crosshair cursor */
    MN_CURSORTYPE,	/* cursor type */
    MN_CURSORBLINK,	/* cursor blink */
    MN_TLS,		/* TLS tunnel */
    MN_VERIFY,		/* verify host certificate */
    MN_ACCEPT,		/* accept hostname */
    MN_PROXY,		/* use proxy host */
    MN_PROXY_SERVER,	/* proxy host name */
    MN_PROXY_PORT,	/* proxy port number */
    MN_PROXY_USER,	/* proxy user name */
    MN_PROXY_PASSWORD,	/* proxy password */
    MN_3287,		/* printer session */
    MN_3287_MODE,	/* printer mode */
    MN_3287_LU,		/* printer logical unit */
    MN_3287_PRINTER,	/* printer Windows printer */
    MN_3287_CODEPAGE,	/* printer code page */
    MN_KEYMAPS,		/* keymaps */
    MN_EMBED_KEYMAPS,	/* embed keymaps */
    MN_FONT,		/* font */
    MN_BG,		/* background color */
    MN_MENUBAR,		/* menu bar */
    MN_TRACE,		/* trace at start-up */
    MN_ALWAYS_INSERT,	/* always use insert mode */
    MN_NOTEPAD,		/* use Notepad to edit file (last option) */
    MN_N_OPTS
} menu_option_t;

/* Return value from get_session(). */
typedef enum {
    GS_NEW,		/* file does not exist */
    GS_EDIT,		/* file does exist and is editable, edit it */
    GS_NOEDIT,		/* file does exist and is editable, do not edit it */
    GS_OVERWRITE,	/* file exists but is uneditable, overwrite it */
    GS_ERR = -1,	/* error */
    GS_NOEDIT_LEAVE = -2 /* uneditable and they don't want to overwrite it */
} gs_t;

/* Return value from edit_menu(). */
typedef enum {
    SRC_PUBLIC_DOCUMENTS,/* success, in public Documents\wc3270 */
    SRC_DOCUMENTS,	/* success, in My Documents\wc3270 */
    SRC_PUBLIC_DESKTOP,	/* success, on public Desktop */
    SRC_DESKTOP,	/* success, on Desktop */
    SRC_OTHER,		/* not sure where the file is */
    SRC_NONE,		/* don't rewrite the file */
    SRC_ERR = -1	/* error */
} src_t;

/* Return value from main_menu(). */
typedef enum {
    MO_CREATE = 1,	/* create new session */
    MO_EDIT,		/* edit existing session */
    MO_DELETE,		/* delete existing session */
    MO_COPY,		/* copy existing session */
    MO_RENAME,		/* rename existing session */
    MO_SHORTCUT,	/* create shortcut */
    MO_MIGRATE,		/* migrate AppData files */
    MO_RESHORT,		/* re-create all shortcuts */
    MO_QUIT,		/* quit wizard */
    MO_ERR = -1		/* error */
} menu_op_t;
#define MO_FIRST	MO_CREATE
#define MO_LAST		MO_QUIT

/* Return value from session_wizard(). */
typedef enum {
    SW_SUCCESS,		/* successful operation */
    SW_QUIT,		/* quit */
    SW_ERR = -1		/* error */
} sw_t;

#define YN_ERR		(-1)	/* error return from getyn() */
#define YN_RETRY	(-2)	/* user input error from getyn() */

/* Return value from write_shorcut(). */
typedef enum {
    WS_NOP,		/* did nothing */
    WS_CREATED,		/* new shortcut */
    WS_REPLACED,	/* replaced shortcut */
    WS_FAILED,		/* operation failed */
    WS_ERR = -1		/* error */
} ws_t;

extern char *wversion;

#define CS_WIDTH	19
#define CP_WIDTH	8
#define WP_WIDTH	6
#define	CS_COLS		2

#define MAX_PRINTERS	256
PRINTER_INFO_1 printer_info[MAX_PRINTERS];
unsigned num_printers = 0;
char default_printer[1024];

static struct {
    char *name;
    char *description;
} builtin_keymaps[] = {
    { "rctrl",	"Map PC Right Ctrl key to 3270 'Enter' and PC Enter key to 3270 'Newline'" },
    { "righttoleft",	"Mappings for right-to-left language support" },
    { NULL,		NULL }
};

static struct proxy_desc {
    char *name;
    char *protocol;
    char *port;
    bool user;
} proxies[] = {
    {
	PROXY_HTTP,
	"HTTP tunnel (RFC 2817, e.g., squid)",
	PORT_HTTP,
	true
    },
    {
	PROXY_PASSTHRU,
	"Sun telnet-passthru",
	NULL,
	false
    },
    {
	PROXY_SOCKS4,
	"SOCKS version 4",
	PORT_SOCKS4,
	true
    },
    {
	PROXY_SOCKS5,
	"SOCKS version 5 (RFC 1928)",
	PORT_SOCKS5,
	true
    },
    {
	PROXY_TELNET,
	"None (just send 'connect host port')",
	NULL,
	false
    },
    { NULL, NULL, NULL, false }
};

static int write_session_file(const session_t *s, char *us, const char *path);

static char *program;
static char *appdata_wc3270 = NULL;	/* user's wc3270 AppData directory */
static char *common_appdata_wc3270 = NULL;/* common wc327 AppData directory */
static char *installdir = NULL;		/* installation directory */
static char *desktop = NULL;		/* Desktop */
static char *public_desktop = NULL;	/* Public Desktop */
static char *documents;			/* My Documents directory */
static char *public_documents;		/* public Documents directory */
static char *documents_wc3270;		/* My Documents\wc3270 directory */
static char *public_documents_wc3270;	/* public Documents\wc3270 directory */
static char *searchdir;			/* where to look for current user's sessions */
static char *public_searchdir;		/* where to look for shared sessions */
unsigned windirs_flags;
static TCHAR username[UNLEN + 1];

static int get_printerlu(session_t *s, int explain);

static int num_xs;
static const char *xs_name(int n, src_t *lp);
static void xs_init(bool include_public);
typedef struct xs {	/* Existing session: */
    src_t location;	/*  location (current user or all users) */
    char *name;		/*  session name */
    struct xs *next;	/*  list linkage */
} xs_t;
typedef struct {	/* Set of existing sessions: */
    int count;		/*  count */
    xs_t *list;		/*  list of sessions */
} xsb_t;
static xsb_t xs_my;	/* current-user sessions */
static xsb_t xs_public;	/* public sessions */

static session_t empty_session;
static HANDLE conin_handle = INVALID_HANDLE_VALUE;
static HANDLE stdout_handle = INVALID_HANDLE_VALUE;

static void write_user_settings(char *us, FILE *f);
static void display_sessions(bool with_numbers, bool include_public);
static ws_t write_shortcut(const session_t *s, bool ask, src_t src,
	const char *path, bool change_shortcut);
static void create_wc3270_folder(src_t src);

static sw_t do_upgrade(bool);
static BOOL admin(void);
static bool ad_exist(void);

/* Set up the stdout handle. */
static bool
setup_stdout(void)
{
    if (stdout_handle != INVALID_HANDLE_VALUE) {
	return true;
    }
    stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    return (stdout_handle != INVALID_HANDLE_VALUE);
}

/* Clear the screen. */
static void
cls(void)
{
    system("cls");
    if (setup_stdout()) {
	SetConsoleTextAttribute(stdout_handle,
		FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED |
		FOREGROUND_INTENSITY);
    }
}

/* Generate output in specific colors. */
static void
color_out(const char *fmt, int colors, va_list ap)
{
    if (!setup_stdout()) {
	vprintf(fmt, ap);
	fflush(stdout);
	return;
    }
    fflush(stdout);
    SetConsoleTextAttribute(stdout_handle, colors);
    vprintf(fmt, ap);
    fflush(stdout);
    SetConsoleTextAttribute(stdout_handle,
	    FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED |
	    FOREGROUND_INTENSITY);
}

/* Generate error (actually just red) output. */
static int
errout(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    color_out(fmt, FOREGROUND_RED | FOREGROUND_INTENSITY, ap);
    va_end(ap);
    return 0;
}

/* Generate green output. */
static void
greenout(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    color_out(fmt, FOREGROUND_GREEN | FOREGROUND_INTENSITY, ap);
    va_end(ap);
}

/* Generate yellow output. */
static void
yellowout(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    color_out(fmt, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	    ap);
    va_end(ap);
}

/* Generate reverse output. */
static void
reverseout(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    color_out(fmt, BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN, ap);
    va_end(ap);
}

/* Generate gray output. */
static void
grayout(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    color_out(fmt, FOREGROUND_INTENSITY, ap);
    va_end(ap);
}

/**
 * Fetch a line of input from the console.
 *
 * The input is stripped of any leading whitespace and trailing whitespace,
 * and is NULL-terminated.
 *
 * @param[out] buf	Buffer to read input into
 * @param[in] bufsize	Size of buffer
 *
 * @return buf, or NULL if an error such as EOF is encountered.
 */
static char *
get_input(char *buf, int bufsize)
{
    char xbuf[8192];
    char *s;
    size_t sl;

    /* Make sure all of the output gets out. */
    fflush(stdout);

    /*
     * Get the raw input from stdin.
     * Use a large temporary buffer instead of reading directly into the
     * caller's buffer, so oversize input will be truncated rather than being
     * interpreted as the next line of input.
     */
    if (fgets(xbuf, sizeof(xbuf), stdin) == NULL) {
	return NULL;
    }
    strncpy(buf, xbuf, bufsize);
    buf[bufsize - 1] = '\0';

    /* Trim leading whitespace. */
    s = buf;
    sl = strlen(buf);
    while (*s && isspace(*s)) {
	s++;
	sl--;
    }
    if (s != buf) {
	memmove(buf, s, sl + 1);
    }

    /* Trim trailing whitespace. */
    while (sl && isspace(buf[--sl])) {
	buf[sl] = '\0';
    }

    return buf;
}

/**
 * Ask a yes or no question.
 *
 * @param[in] defval	Default response (TRUE or FALSE).
 *
 * @return TRUE or FALSE	Proper respoonse
 *         YN_ERR		I/O error occurred (usually EOF)
 *         YN_RETRY		User entry error, error message already printed
 */
static int
getyn(int defval)
{
    char yn[STR_SIZE];

    if (get_input(yn, STR_SIZE) == NULL) {
	return YN_ERR;
    }

    if (!yn[0]) {
	return defval;
    }
    if (!strncasecmp(yn, "quit", strlen(yn))) {
	return YN_ERR;
    }
    if (!strncasecmp(yn, "yes", strlen(yn))) {
	return TRUE;
    }
    if (!strncasecmp(yn, "no", strlen(yn))) {
	return FALSE;
    }

    errout("\nPlease answer (y)es or (n)o.");
    return YN_RETRY;
}

/**
 * Gather the list of system printers from Windows.
 */
static void
enum_printers(void)
{
    DWORD needed = 0;
    DWORD returned = 0;

    /* Get the default printer name. */
    default_printer[0] = '\0';
    if (GetProfileString("windows", "device", "", default_printer,
		sizeof(default_printer)) != 0) {
	char *comma;

	if ((comma = strchr(default_printer, ',')) != NULL) {
	    *comma = '\0';
	}
    }

    /* Get the list of printers. */
    if (EnumPrinters(
		PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
		NULL,
		1,
		(LPBYTE)&printer_info,
		sizeof(printer_info),
		&needed,
		&returned) == 0) {
	return;
    }

    num_printers = returned;
}

/**
 * Request that the user press the Enter key.
 *
 * This generally happens after displaying an error message.
 */
static void
ask_enter(void)
{
    char buf[2];

    grayout("[Press <Enter>] ");
    fflush(stdout);
    fgets(buf, sizeof(buf), stdin);
}

/**
 * Return true if the string names a folder.
 * The string is assumed not to have doubled backslashes.
 *
 * @param[in] name	Name
 * @return true if printer is a folder.
 */
static bool
nprinter_is_folder(const char *name)
{
    return (((name[0] >= 'A' && name[0] <= 'Z') ||
		(name[0] >= 'a' && name[0] <= 'z')) &&
	    name[1] == ':' && name[2] == '\\' && name[3] == '\\') ||
	(name[0] == '\\' && name[1] != '\\');
}

/**
 * Return true if the printer in a session names a folder.
 * Alas, the printer name has doubled backslashes.
 *
 * @param[in] s		Session
 * @return true if printer is a folder.
 */
static bool
printer_is_folder(const session_t *s)
{
    const char *p = s->printer;

    return (((p[0] >= 'A' && p[0] <= 'Z') ||
		(p[0] >= 'a' && p[0] <= 'z')) &&
	    p[1] == ':' && p[2] == '\\') ||
	(p[0] == '\\' && p[1] == '\\' && p[2] != '\\');
}

/**
 * Check a printer name to see if it fits in the buffer.
 * @param[in] s		Printer name
 * @param[in] size	Buffer size, including trailing NUL
 * @return true if the expanded name fits in the buffer.
 */
static bool
printer_fits(const char *s, size_t size)
{
    size_t sl = strlen(s);
    const char *t = s;
    char c;

    while ((c = *t++) != '\0') {
	sl += c == '\\';
    }
    return (sl + 1) <= size;
}

/**
 * Get an 'other' printer name from the console.
 *
 * Accepts the name 'default' to mean the system default printer.
 *
 * @param[in] defname		Default printer name to use for empty input, or
 * 				NULL
 * @param[out] printername	Resulting printer name
 * @param[in] bufsize		Size of printername buffer
 *
 * @return 0 for success, -1 for error such as EOF
 *  Returns an empty string in 'printername' to indicate the default printer
 */
static int
get_printer_name(const char *defname, char *printername, int bufsize)
{
    for (;;) {
	printf("\nEnter Windows printer name or folder path: [%s] ",
		defname[0]? defname: "use system default");
	fflush(stdout);
	if (get_input(printername, bufsize) == NULL) {
	    return -1;
	}
	if (!printername[0]) {
	    if (defname[0]) {
		snprintf(printername, bufsize, "%s", defname);
	    }
	    break;
	}
	if (!printer_fits(printername, bufsize)) {
	    yellowout("Name is too long\n");
	    continue;
	}
	if (!strcmp(printername, "default")) {
	    printername[0] = '\0';
	}
	if (nprinter_is_folder(printername)) {
	    struct stat buf;

	    if (stat(printername, &buf) != 0 ||
		    (buf.st_mode & S_IFMT) != S_IFDIR) {
		yellowout("Warning: %s does not exist or is not a folder\n",
			printername);
		ask_enter();
	    }

	}
	break;
    }
    return 0;
}

typedef struct km {			/* Keymap: */
	struct km *next;		/*  List linkage */
    	char name[MAX_PATH];		/*  Name */
	char description[STR_SIZE];	/*  Description */
	char *def_both;			/*  Definition (common) */
	char *def_3270;			/*  Definition (3270 mode) */
	char *def_nvt;			/*  Definition (NVT mode) */
	src_t src;			/*  Where it is: */
					/*   SRC_DOCUMENTS per-user */
					/*   SRC_PUBLIC_DOCUMENTS all-users */
					/*   SRC_NONE built-in */
					/*   SRC_OTHER install dir */
} km_t;
km_t *km_first = NULL;
km_t *km_last = NULL;

/**
 * Save a keymap name. Return its node.
 *
 * @param[in] path		Pathname of keymap file
 * @param[in] keymap_name	Name of keymap
 * @param[in] description	Keymap description, or NULL
 * @param[in] src		Where it is
 *
 * @return Keymap node, possibly newly-allocated.
 */
static km_t *
save_keymap_name(const char *path, char *keymap_name, const char *description,
	src_t src)
{
    km_t *km;
    size_t sl;
    km_t *kms;
    FILE *f;
    enum { KMF_BOTH, KMF_3270, KMF_NVT } km_mode = KMF_BOTH;
    char **def = NULL;

    km = (km_t *)malloc(sizeof(km_t));
    if (km == NULL) {
	errout("Out of memory\n");
	return NULL;
    }
    memset(km, '\0', sizeof(km_t));
    strcpy(km->name, keymap_name);
    km->description[0] = '\0';
    sl = strlen(km->name);
    km->src = src;

    /* Slice off the '.wc3270km' suffix. */
    if (sl > KS_LEN && !strcasecmp(km->name + sl - KS_LEN, KEYMAP_SUFFIX)) {
	km->name[sl - KS_LEN] = '\0';
	sl -= KS_LEN;
    }

    /* Slice off any '.3270' or '.nvt' before that. */
    if (sl > LEN_3270 && !strcasecmp(km->name + sl - LEN_3270, KM_3270)) {
	km->name[sl - LEN_3270] = '\0';
	sl -= LEN_3270;
	km_mode = KMF_3270;
    } else if (sl > LEN_NVT && !strcasecmp(km->name + sl - LEN_NVT, KM_NVT)) {
	km->name[sl - LEN_NVT] = '\0';
	sl -= LEN_NVT;
	km_mode = KMF_NVT;
    }

    for (kms = km_first; kms != NULL; kms = kms->next) {
	if (!strcasecmp(kms->name, km->name)) {
	    break;
	}
    }
    if (kms != NULL) {
	free(km);
	km = kms;
    } else {
	km->next = NULL;
	if (km_last != NULL) {
	    km_last->next = km;
	} else {
	    km_first = km;
	}
	km_last = km;
    }

    /* Check if we've already seen this keymap. */
    switch (km_mode) {
    case KMF_BOTH:
	def = &km->def_both;
	break;
    case KMF_3270:
	def = &km->def_3270;
	break;
    case KMF_NVT:
	def = &km->def_nvt;
	break;
    }
    if (*def != NULL) {
	return km;
    }

    if (description != NULL) {
	strcpy(km->description, description);
	return km;
    }

    /* Dig for a description and save the definition. */
    if (path != NULL) {
	f = fopen(path, "r");
	if (f != NULL) {
	    char buf[STR_SIZE];

	    while (fgets(buf, STR_SIZE, f) != NULL) {
		int any = 0;

		sl = strlen(buf);
		if (sl > 0 && buf[sl - 1] == '\n') {
		    buf[--sl] = '\0';
		}
		if (!strncasecmp(buf, KM_DESC, LEN_DESC)) {
		    strncpy(km->description, buf + LEN_DESC,
			    sl - LEN_DESC + 1);
		    continue;
		}
		if (buf[0] == '!' || !buf[0]) {
		    continue;
		}
		if (*def == NULL) {
		    *def = malloc(strlen(buf) + 2);
		} else {
		    *def = realloc(*def, strlen(*def) + 5 + strlen(buf) + 1);
		    any = 1;
		}
		if (*def == NULL) {
		    errout("Out of memory\n");
		    ask_enter();
		    exit(1);
		}
		if (!any) {
		    strcat(strcpy(*def, " "), buf);
		} else {
		    strcat(strcat(*def, "\\n\\\n "), buf);
		}
	    }
	    fclose(f);
	}
    }

    return km;
}

/**
 * Initialize keymaps from one directory.
 *
 * @param[in] src	type of directory
 * @param[in] dirname	name of directory
 */
static void
save_keymaps_type(src_t src, const char *dirname)
{
    char dpath[MAX_PATH];
    char fpath[MAX_PATH];
    HANDLE h;
    WIN32_FIND_DATA find_data;

    sprintf(dpath, "%s%s", dirname, DONE_FILE);
    if (access(dpath, R_OK) != 0) {
	sprintf(dpath, "%s*%s", searchdir, KEYMAP_SUFFIX);
	h = FindFirstFile(dpath, &find_data);
	if (h != INVALID_HANDLE_VALUE) {
	    do {
		sprintf(fpath, "%s%s", dirname, find_data.cFileName);
		save_keymap_name(fpath, find_data.cFileName, NULL, src);
	    } while (FindNextFile(h, &find_data) != 0);
	    FindClose(h);
	}
    }
}

/**
 * Initialize the set of available keymaps.
 *
 * Adds the builtin keymaps to a database, then searches the two Docs
 * directories for user-defined keymaps and adds those.
 *
 * @param[in] include_public	if true, include public folder
 */
static void
save_keymaps(bool include_public)
{
    int i;

    for (i = 0; builtin_keymaps[i].name != NULL; i++) {
	save_keymap_name(NULL, builtin_keymaps[i].name,
		builtin_keymaps[i].description, SRC_NONE);
    }

    save_keymaps_type(SRC_DOCUMENTS, searchdir);
    if (include_public) {
	save_keymaps_type(SRC_PUBLIC_DOCUMENTS, public_searchdir);
    }
}

/**
 * Fix up a printer path, doubling backslashes.
 *
 * @param[in,out] s	Session
 */
static void
fixup_backslashes(session_t *s)
{
    char buf[STR_SIZE];
    int i, j;
    char c;

    j = 0;
    for (i = 0; i < (STR_SIZE - 1) && (c = s->printer[i]) != '\0'; i++) {
	if (c == '\\' || c == '!') {
	    if (j < (STR_SIZE - 1)) {
		buf[j++] = '\\';
	    }
	    if (j < (STR_SIZE - 1)) {
		buf[j++] = c;
	    }
	} else {
	    if (j < (STR_SIZE - 1)) {
		    buf[j++] = c;
	    }
	}
    }
    buf[j] = '\0';
    strncpy(s->printer, buf, STR_SIZE);
}

/**
 * Canonicalize a code page.
 *
 * @param[in,out] s	Session
 *
 * @return 1 if the name needed fixing, 0 otherwise.
 */
static int
fixup_codepage(session_t *s)
{
    unsigned i;

    /* See if it's okay already. */
    if (!strcmp(s->codepage, "bracket") || !strncmp(s->codepage, "cp", 2)) {
	return 0;
    }

    /* Search for a match. */
    for (i = 0; i < num_codepages; i++) {
	if (!strcmp(codepages[i].name, "bracket")) {
	    continue;
	}
	if (!strcmp(s->codepage, codepages[i].name)) {
	    snprintf(s->codepage, STR_SIZE, "cp%s", codepages[i].hostcp);
	    return 1;
	}
    }

    /* No match. This will not be pretty. */
    return 0;
}

/**
 * Reformat a quoted path for display.
 *
 * @param[in] expanded		path in session file (quoted) format
 * @param[out] condensed	path in display format
 */
static void
redisplay_printer(const char *expanded, char *condensed)
{
    int i;
    int j;
    bool bsl = false;

    j = 0;
    for (i = 0; i < STR_SIZE; i++) {
	char c = expanded[i];

	if (c == '\0') {
	    if (bsl) {
		goto abort;
	    }
	    condensed[j] = c;
	    break;
	}

	if (bsl) {
	    bsl = false;
	} else if (c == '\\') {
	    bsl = true;
	}
	if (!bsl) {
	    condensed[j++] = c;
	}
    }
    return;

abort:
    strcpy(condensed, expanded);
}

/**
 * Clear the screen, print a common banner and a title.
 *
 * @param[in] s		Session (its name is displayed, if defined)
 * @param[in] path	Pathname of session file, or NULL
 * @param[in] title	Text to display after session and path
 */
static void
new_screen(session_t *s, const char *path, const char *title)
{
    static char wizard[] = "wc3270 Session Wizard";
    cls();
    reverseout("%s%*s%s\n",
	    wizard,
	    (int)(79 - strlen(wizard) - strlen(wversion)), " ",
	    wversion);
    if (s->session[0]) {
	printf("\nSession: %s\n", s->session);
    }
    if (path != NULL) {
	printf("Path: %s\n", path);
    }
    printf("\n%s\n", title);
}

/*
 * List of main menu operations.
 *
 * N.B.: This list is sorted in menu_op_t (MO_XXX) order. If you re-order one,
 * you *must* re-order the other.
 */
struct {		/* Menu options: */
    const char *text;	/*  long name */
    const char *name;	/*  short name */
    const char *alias;	/*  short name alias */
    bool requires_xs;	/*  if true, requires existing sessions */
    bool requires_ad;	/*  if true, requires unmigrated files */
    int num_params;	/*  number of command-line parameters to accept */
} main_option[] = {
    { NULL, NULL, FALSE, 0 }, /* intentional hole */
    { "Create new session",         "new",      "create", false, false, 1 },
    { "Edit session",               "edit",     NULL,     true,  false, 1 },
    { "Delete session",             "delete",   "rm",     true,  false, 1 },
    { "Copy session",               "copy",     "cp",     true,  false, 2 },
    { "Rename session",             "rename",   "mv",     true,  false, 2 },
    { "Create shortcut",            "shortcut", NULL,     true,  false, 1 },
    { "Migrate files from AppData", "migrate",  NULL,     false, true,  0 },
    { "Re-create all shortcuts",    "reshort",  NULL,     true, false,  0 },
    { "Quit",                       "quit",     "exit",   false, false, 0 },
    { NULL, NULL, FALSE, 0 } /* end marker */
};

/**
 * Main screen.
 *
 * Displays a list of existing sessions (as a mnemonic) and a list of
 * available operations. Prompts for an operation. Returns the operation
 * selected.
 *
 * @param[out] argcp	Returned number of parameters
 * @param[out] argvp	Returned parameters
 * @param[in,out] result Result of previous operation
 *
 * @return menu_op_t describing selected operation
 */
#define MAX_TOKENS 3

static menu_op_t
main_menu(int *argcp, char ***argvp, char *result)
{
    static char enq[256];
    static char *token[MAX_TOKENS + 1];
    int num_tokens;
    int i;

    *argcp = 0;
    *argvp = NULL;

    new_screen(&empty_session, NULL, "\
Overview\n\
\n\
This wizard allows you to set up a new wc3270 session or modify an existing\n\
one. It also lets you create or replace a shortcut on the desktop.\n");

    display_sessions(false, true);

    printf("\n");
    for (i = MO_FIRST; main_option[i].text != NULL; i++) {
	if ((main_option[i].requires_xs && !num_xs) ||
	    (main_option[i].requires_ad && !ad_exist())) {
#if 0
	    grayout("  %d. %s (%s)\n",
		    i, main_option[i].text, main_option[i].name);
#endif
	    continue;
	} else {
	    printf("  %d. %s (%s)\n",
		    i, main_option[i].text, main_option[i].name);
	}
    }

    for (;;) {
	size_t sl;
	int mo;

	if (result && result[0]) {
	    if (result[0] == 1) {
		greenout("\n%s", result + 1);
	    } else {
		errout("\n%s", result + 1);
	    }
	    result[0] = '\0';
	}
	printf("\nEnter command name or number (%d..%d) [%s] ",
		MO_FIRST, MO_LAST, main_option[MO_CREATE].name);
	fflush(stdout);
	if (get_input(enq, sizeof(enq)) == NULL) {
	    return MO_ERR;
	}

	/* Check the default. */
	sl = strlen(enq);
	if (!sl) {
	    return MO_CREATE;
	}

	/* Split into tokens. */
	num_tokens = 0;
	token[0] = strtok(enq, " \t");
	if (token[0] == NULL) {
	    errout("\nWow, am I confused.\n");
	    continue;
	}
	sl = strlen(token[0]);
	num_tokens++;
	while (num_tokens < MAX_TOKENS) {
	    if ((token[num_tokens] = strtok(NULL, " \t")) != NULL) {
		num_tokens++;
	    } else {
		break;
	    }
	}
	if (strtok(NULL, " \t") != NULL) {
	    goto extra;
	}
	token[num_tokens] = NULL;

	/* Check numbers. */
	mo = atoi(token[0]);
	if (mo >= MO_FIRST && mo <= MO_LAST) {
	    if (num_tokens > 1) {
		goto extra;
	    }
	    if (!num_xs && main_option[mo].requires_xs) {
		errout("\nUnknown command.");
		continue;
	    }
	    if (main_option[mo].requires_ad && !ad_exist()) {
		errout("\nUnknown command.");
		continue;
	    }
	    return (menu_op_t)mo;
	}

	/* Check keywords. */
	for (i = MO_FIRST; main_option[i].text != NULL; i++) {
	    if (!num_xs && main_option[i].requires_xs) {
		continue;
	    }
	    if (!strncasecmp(token[0], main_option[i].name, sl)) {
		if (num_tokens - 1 > main_option[i].num_params) {
		    goto extra;
		}
		*argcp = num_tokens - 1;
		*argvp = token + 1;
		return (menu_op_t)i;
	    }
	}

	/* Check again for aliases. */
	for (i = MO_FIRST; main_option[i].text != NULL; i++) {
	    if (!num_xs && main_option[i].requires_xs) {
		continue;
	    }
	    if (main_option[i].alias != NULL &&
		    !strncasecmp(token[0], main_option[i].alias, sl)) {
		if (num_tokens - 1 > main_option[i].num_params) {
		    goto extra;
		}
		*argcp = num_tokens - 1;
		*argvp = token + 1;
		return (menu_op_t)i;
	    }
	}

	errout("\nUnknown command.");
	continue;

    extra:
	errout("\nExtra parameter(s).");
	continue;
    }
}

/**
 * Search a well-defined series of locations for a session file.
 *
 * @param[in] session_name	Name of session
 * @param[out] path		Returned pathname
 *
 * @return SRC_XXX enumeration
 */
static src_t
find_session_file(const char *session_name, char *path)
{
    /* Try the user's My Documents\wc3270. */
    snprintf(path, MAX_PATH, "%s%s%s", documents_wc3270, session_name,
	    SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_DOCUMENTS;
    }

    /* Try the public Documents\wc3270. */
    if (admin()) {
	snprintf(path, MAX_PATH, "%s%s%s", public_documents_wc3270,
		session_name, SESS_SUFFIX);
	if (access(path, R_OK) == 0) {
	    return SRC_PUBLIC_DOCUMENTS;
	}
    }

    /* Try the user's Desktop. */
    snprintf(path, MAX_PATH, "%s%s%s", desktop, session_name, SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_DESKTOP;
    }

    /* Try the public Desktop. */
    if (admin()) {
	snprintf(path, MAX_PATH, "%s%s%s", public_desktop, session_name,
		SESS_SUFFIX);
	if (access(path, R_OK) == 0) {
	    return SRC_PUBLIC_DESKTOP;
	}
    }

    /* Try cwd. */
    snprintf(path, MAX_PATH, "%s%s", session_name, SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_OTHER;
    }

    /*
     * Put the new one in My Documents\wc3270.
     * XXX: I don't think this value is actually used.
     */
    snprintf(path, MAX_PATH, "%s%s%s", documents_wc3270, session_name,
	    SESS_SUFFIX);
    return SRC_OTHER;
}

/**
 * Check a session name for illegal characters.
 *
 * Displays an error message.
 *
 * @param[in] name		Name to check
 * @param[in] result		Result buffer for error message, or NULL
 * @param[in] result_size	Size of result buffer
 *
 * @return true for success, false for error.
 */
#define SESSION_NAME_ERR \
"Illegal character(s).\n\
Session names can only have letters, numbers, spaces, underscores and dashes."
static bool
legal_session_name(const char *name, char *result, size_t result_size)
{
    if (strspn(name, LEGAL_CNAME) != strlen(name)) {
	if (result != NULL) {
	    snprintf(result, result_size, "%c%s", 2, SESSION_NAME_ERR);
	} else {
	    errout("\n%s", SESSION_NAME_ERR);
	}
	return false;
    } else {
	return true;
    }
}

/**
 * Preliminary triage of session file.
 *
 * Prompts for a session name if one was not provided on the command line.
 * Figures out if the file is editable. Asks if an existing file should be
 * edited or (if not editable) replaced.
 *
 * @param[in] session_name	Session name. If NULL, prompt for one
 * 				If non-NULL and does not end in .wc3270, take
 * 				this as the session name, and fail if it
 * 				contains invalid characters.
 *   				If non-NULL and ends in .wc3270, take this as
 *   				the path to the session file.
 * @param[out] s		Session structure to fill in with name and (if
 * 				the file exists) current contents
 * @param[out] us		User parameters
 * @param[out] path		Pathname of session file
 * @param[in] explicit_edit	If true, -e was passed on command line; skip
 * 				the 'exists. Edit?' dialog
 * @param[out] src		Where the session file was found, if it exists
 * @param[out] modified		Session was already modified when read in
 *
 * Returns: gs_t
 *  GS_NEW		file does not exist
 *  GS_EDIT		file does exist and is editable, edit it
 *  GS_NOEDIT		file does exist and is editable, do not edit it
 *  GS_OVERWRITE	file exists but is uneditable, overwrite it
 *  GS_ERR		fatal error
 *  GS_NOEDIT_LEAVE	uneditable and they don't want to overwrite it
 */
static gs_t
get_session(const char *session_name, session_t *s, char **us, char *path,
	bool explicit_edit, src_t *src, bool *modified)
{
    FILE *f;
    int rc;
    int editable;

    *src = SRC_OTHER;
    *modified = false;

    if (session_name != NULL) {
	size_t sl = strlen(session_name);
	size_t slen = sizeof(s->session);

	/*
	 * Session file pathname or session name specified on the
	 * command line.
	 */
	if (sl > SESS_LEN && !strcasecmp(session_name + sl - SESS_LEN,
					    SESS_SUFFIX)) {
	    char *bsl;
	    char *colon;

	    /* Ends in .wc3270km. Pathname. */
	    path[MAX_PATH - 1] = '\0';
	    bsl = strrchr(session_name, '\\');
	    colon = strrchr(session_name, ':');
	    if (bsl == NULL && colon == NULL) {
		/*
		 * No directory or drive prefix -- just a file name.
		 */
		if (sl - SESS_LEN + 1 < slen) {
		    slen = sl - SESS_LEN + 1;
		}
		strncpy(s->session, session_name, slen);
		s->session[slen - 1] = '\0';

		*src = find_session_file(s->session, path);
	    } else {
		/*
		 * Full pathname.  Copy what's between the last [:\] and
		 * ".wc3270" as the session name.
		 */
		char *start;

		strncpy(path, session_name, MAX_PATH);
		if (bsl != NULL && colon == NULL) {
		    start = bsl + 1;
		} else if (bsl == NULL && colon != NULL) {
		    start = colon + 1;
		} else if (bsl > colon) {
		    start = bsl + 1;
		} else {
		    start = colon + 1;
		}
		if (strlen(start) - SESS_LEN + 1 < slen) {
		    slen = strlen(start) - SESS_LEN + 1;
		}
		strncpy(s->session, start, slen);
		s->session[slen - 1] = '\0';

		/*
		 * Try to figure out where it is.  This is inherently
		 * imperfect.
		 */
		if (!strncmp(path, documents_wc3270,
			    strlen(documents_wc3270))) {
		    *src = SRC_DOCUMENTS;
		} else if (!strncmp(path, public_documents_wc3270,
			    strlen(public_documents_wc3270))) {
		    *src = SRC_PUBLIC_DOCUMENTS;
		} else if (!strncmp(path, desktop, strlen(desktop))) {
		    *src = SRC_DESKTOP;
		} else if (!strncmp(path, public_desktop,
			    strlen(public_desktop))) {
		    *src = SRC_PUBLIC_DESKTOP;
		} else {
		    *src = SRC_OTHER;
		}
	    }

	} else {
	    /* Session name, no suffix. */
	    strncpy(s->session, session_name, slen);
	    s->session[slen - 1] = '\0';

	    *src = find_session_file(s->session, path);
	}

	/* Validate the session name. */
	if (!legal_session_name(s->session, NULL, 0)) {
	    return GS_ERR;
	}

    } else {

	/* Get the session name interactively. */
	new_screen(s, NULL, "\
New Session Name\n\
\n\
This is a unique name for the wc3270 session.  It is the name of the file\n\
containing the session configuration parameters and the name of the desktop\n\
shortcut.");
	for (;;) {
	    printf("\nEnter session name: ");
	    fflush(stdout);
	    if (get_input(s->session, sizeof(s->session)) == NULL) {
		return GS_ERR;
	    }
	    if (!s->session[0]) {
		continue;
	    }
	    if (!legal_session_name(s->session, NULL, 0)) {
		continue;
	    }

	    break;
	}
	*src = find_session_file(s->session, path);
    }

    f = fopen(path, "r");
    if (f != NULL) {
	editable = read_session(f, s, us);
	fclose(f);
	if (editable) {
	    if (fixup_codepage(s)) {
		printf("\n"
"NOTE: This session file contains a code page alias. Even if you do not need\n"
" to make any other changes to the session, please update the session file\n"
" have this name changed to the canonical form.\n");
		*modified = true;
	    }
	}

	if (editable) {
	    if (explicit_edit) {
		return GS_EDIT; /* edit it */
	    }
	    for (;;) {
		printf("\nSession '%s' exists", s->session);
		switch (*src) {
		case SRC_PUBLIC_DOCUMENTS:
		    printf(" (defined for all users)");
		    break;
		case SRC_DOCUMENTS:
		    printf(" (defined for user '%s')", username);
		    break;
		default:
		    break;
		}
		printf(".\nEdit it? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(TRUE);
		if (rc == YN_ERR) {
		    return GS_ERR;
		} else if (rc == FALSE) {
		    return GS_NOEDIT; /* do not edit */
		} else if (rc == TRUE) {
		    return GS_EDIT; /* edit it */
		}
	    }
	} else {
	    for (;;) {
		printf("\nSession '%s' already exists but cannot be edited. "
			"Replace it? (y/n) [n] ", s->session);
		fflush(stdout);
		rc = getyn(FALSE);
		if (rc == YN_ERR) {
		    return GS_ERR;
		} else if (rc == FALSE) {
		    return GS_NOEDIT_LEAVE; /* don't overwrite */
		} else if (rc == TRUE) {
		    return GS_OVERWRITE; /* overwrite */
		}
	    }
	}
    } else {
	/*
	 * Set the auto-shortcut flag in all new session files,
	 * but not in old ones.  This will prevent unintended
	 * interactions with old shortcuts that don't specify +S, but
	 * will allow new session files to be started with a
	 * double-click.
	 */
	s->flags |= WF_AUTO_SHORTCUT;

	return GS_NEW; /* create it */
    }
}

/**
 * Prompt for a hostname or address.
 *
 * Allows IPv6 addresses if the underlying OS supports them.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure.
 */
static int
get_host(session_t *s)
{
    char buf[STR_SIZE];

#define COMMON_HOST_TEXT1 "\
Host Name\n\
\n\
This specifies the IBM host to connect to.  It can be a symbolic name like\n\
'foo.company.com'"

#define COMMON_HOST_TEXT2 "\
an IPv4 address in dotted-decimal notation such as\n\
'1.2.3.4'"

#define IPV6_HOST_TEXT "\
an IPv6 address in colon notation, such as 'fec0:0:0:1::27'"

#define COMMON_HOST_TEXT3 "\
\n\
\n\
To create a session file with no hostname (one that just specifies the model\n\
number, code page, etc.), enter '" CHOICE_NONE "'."

    new_screen(s, NULL, COMMON_HOST_TEXT1 ", " COMMON_HOST_TEXT2 " or "
	    IPV6_HOST_TEXT "." COMMON_HOST_TEXT3);

    for (;;) {
	size_t n_good;

	if (s->host[0]) {
	    printf("\nEnter host name or IP address: [%s] ", s->host);
	} else {
	    printf("\nEnter host name or IP address: ");
	}
	fflush(stdout);
	if (get_input(buf, sizeof(s->host)) == NULL) {
	    return -1;
	} else if (!strcmp(buf, CHOICE_NONE)) {
	    strcpy(s->host, buf);
	    break;
	}
	n_good = strcspn(buf, " @[]");
	if (n_good != strlen(buf)) {
	    errout("\nInvalid character '%c' in host name.",
		    buf[n_good]);
	    continue;
	}
	if (isalpha(buf[0]) && buf[1] == ':') {
	    errout("\nDo not include prefixes in host names.");
	    if (buf[0] == 'L' || buf[0] == 'l') {
		errout("\nTo set up a TLS tunnel, use the TLS tunnel option.");
	    }
	    continue;
	}
	if (!buf[0]) {
	    if (!s->host[0]) {
		continue;
	    }
	} else {
	    strcpy(s->host, buf);
	}
	break;
    }
    return 0;
}

/**
 * Prompt for a port number.
 *
 * Allows an non-zero 16-bit number, or the name 'telnet' (23).
 *
 * @return 0 for success, -1 for error.
 */
static int
get_port(session_t *s)
{
    char inbuf[STR_SIZE];
    char *ptr;
    unsigned long u;

    new_screen(s, NULL, "\
TCP Port\n\
\n\
This specifies the TCP Port to use to connect to the host.  It is a number from\n\
1 to 65535 or the name 'telnet'.  The default is the 'telnet' port, port 23.");

    for (;;) {
	printf("\nTCP port: [%d] ", (int)s->port);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	} else if (!strcasecmp(inbuf, "telnet")) {
	    s->port = 23;
	    break;
	}
	u = strtoul(inbuf, &ptr, 10);
	if (u < 1 || u > 65535 || *ptr != '\0') {
	    errout("\nInvalid port.");
	} else {
	    s->port = (int)u;
	    break;
	}
    }
    return 0;
}

static int
get_lu(session_t *s)
{
    char buf[STR_SIZE];

    new_screen(s, NULL, "\
Logical Unit (LU) Name\n\
\n\
This specifies a particular Logical Unit or Logical Unit group to connect to\n\
on the host.  The default is to allow the host to select the Logical Unit.");

    for (;;) {
	size_t n_good;

	printf("\nEnter Logical Unit (LU) name: [%s] ",
		s->luname[0]? s->luname: CHOICE_NONE);
	fflush(stdout);
	if (get_input(buf, sizeof(buf)) == NULL) {
	    return -1;
	} else if (!buf[0]) {
	    break;
	} else if (!strcmp(buf, CHOICE_NONE)) {
	    s->luname[0] = '\0';
	    break;
	}
	n_good = strcspn(buf, ":@[]");
	if (n_good != strlen(buf)) {
	    errout("\nLU name contains invalid character '%c'", buf[n_good]);
	    continue;
	}
	strcpy(s->luname, buf);
	break;
    }
    return 0;
}

/**
 * Prompt for a model number.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error.
 */
static int
get_model(session_t *s)
{
    unsigned long i;
    char inbuf[STR_SIZE];
    char *ptr;
    unsigned long u;
    unsigned long max_model = 5;

    new_screen(s, NULL, "\
Model Number\n\
\n\
This specifies the dimensions of the screen.");

    printf("\n");
    for (i = 2; i <= max_model; i++) {
	if (wrows[i]) {
	    printf(" Model %lu has %2d rows and %3d columns.\n",
		    i, wrows[i], wcols[i]);
	}
    }
    for (;;) {
	printf("\nEnter model number: (2, 3, 4 or 5) [%d] ", (int)s->model);
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	}
	u = strtoul(inbuf, &ptr, 10);
	if (u < 2 || u > max_model || *ptr != '\0') {
	    errout("\nInvalid model number.");
	    continue;
	} else if (s->model != (int)u) {
	    s->model = (int)u;
	    s->ov_rows = 0;
	    s->ov_cols = 0;
	}
	break;
    }
    return 0;
}

/**
 * Prompt for an oversize option.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error
 */
static int
get_oversize(session_t *s)
{
    char inbuf[STR_SIZE];
    unsigned r, c;
    char xc;

    new_screen(s, NULL, "\
Oversize\n\
\n\
This specifies 'oversize' dimensions for the screen, beyond the number of\n\
rows and columns specified by the model number.  Some hosts are able to use\n\
this additional screen area; some are not.  Enter '"CHOICE_NONE"' to specify no\n\
oversize.");

    printf("\n\
The oversize must be larger than the default for a model %d (%u rows x %u\n\
columns).\n",
	    (int)s->model, wrows[s->model], wcols[s->model]);

    for (;;) {
	printf("\nEnter oversize dimensions (rows x columns) ");
	if (s->ov_rows || s->ov_cols) {
	    printf("[%ux%u]: ", s->ov_rows, s->ov_cols);
	} else {
	    printf("["CHOICE_NONE"]: ");
	}
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	} else if (!strcasecmp(inbuf, CHOICE_NONE)) {
	    s->ov_rows = 0;
	    s->ov_cols = 0;
	    break;
	} else if (sscanf(inbuf, "%u x %u%c", &r, &c, &xc) != 2) {
	    errout("\nPlease enter oversize in the form 'rows x cols'.");
	    continue;
	} else if ((int)r < wrows[s->model] || (int)c < wcols[s->model]) {
	    errout("\nOversize must be larger than the default for a model %d "
		    "(%u x %u).",
		    (int)s->model, wrows[s->model], wcols[s->model]);
	    continue;
	} else if (r > 255 || c > 255) {
	    errout("\nRows and columns must be 255 or less.");
	    continue;
	} else if (r * c > 0x4000) {
	    errout("\nThe total screen area (rows multiplied by columns) must "
		    "be less than %d.", 0x4000);
	    continue;
	}
	s->ov_rows = (unsigned char)r;
	s->ov_cols = (unsigned char)c;
	break;
    }
    return 0;
}

/**
 * Issue a warning for DBCS characters sets.
 */
static void
dbcs_check(void)
{
    if (IsWindowsVersionOrGreater(6, 0, 0)) {
	printf("\n\
Note: wc3270 DBCS support on Windows Vista and later may require setting the\n\
Windows System Locale to a matching language.\n");
    } else {
	printf("\n\
Note: wc3270 DBCS support on Windows XP requires installation of Windows East\n\
Asian language support.\n");
    }

    printf("[Press Enter to continue] ");
    fflush(stdout);
    (void) getchar();
}

/**
 * Prompt for a code page.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error
 */
static int
get_codepage(session_t *s)
{
    char buf[STR_SIZE];
    unsigned i, k;
    char *ptr;
    unsigned long u;
    int was_dbcs = s->is_dbcs;

    new_screen(s, NULL, "\
Code Page\n\
\n\
This specifies the EBCDIC code page used by the host.");

    printf("\
\nAvailable code pages:\n\n\
  #  Name                Host CP      #  Name                Host CP\n\
 --- ------------------- --------    --- ------------------- --------\n");
    k = 0;
    for (i = 0; codepages[i].name != NULL; i++) {
	size_t j;

	if (i) {
	    if (!(i % CS_COLS)) {
		printf("\n");
	    } else {
		printf("   ");
	    }
	}
	if (!(i % 2)) {
	    j = k;
	} else {
	    j += num_codepages / 2;
	    k++;
	}
	printf(" %2d. %-*s %-*s",
		(int)(j + 1),
		CS_WIDTH, codepages[j].name,
		CP_WIDTH, codepages[j].hostcp);
    }
    printf("\n");
    for (;;) {
	printf("\nCode page: [%s] ", s->codepage);
	if (get_input(buf, sizeof(buf)) == NULL) {
	    return -1;
	}
	if (!buf[0]) {
	    break;
	}
	/* Check for numeric index. */
	u = strtoul(buf, &ptr, 10);
	if (u > 0 && u <= i && *ptr == '\0') {
	    if (!strcmp(codepages[u - 1].name, "bracket")) {
		strcpy(s->codepage, "bracket");
	    } else {
		snprintf(s->codepage, STR_SIZE, "cp%s",
			codepages[u - 1].hostcp);
	    }
	    s->is_dbcs = codepages[u - 1].is_dbcs;
	    break;
	}
	/* Check for numeric code page. */
	if (u > 0 && *ptr == '\0') {
	    unsigned k;
	    bool matched = false;

	    for (k = 0; k < num_codepages; k++) {
		if (strcmp(codepages[k].name, "bracket") &&
			    u == atoi(codepages[k].hostcp)) {
		    snprintf(s->codepage, STR_SIZE, "cp%s",
			    codepages[k].hostcp);
		    s->is_dbcs = codepages[k].is_dbcs;
		    matched = true;
		    break;
		}
	    }
	    if (matched) {
		break;
	    }
	}
	/* Check for name match. */
	for (i = 0; codepages[i].name != NULL; i++) {
	    if (!strcmp(buf, codepages[i].name)) {
		if (!strcmp(buf, "bracket")) {
		    strcpy(s->codepage, buf);
		} else {
		    snprintf(s->codepage, STR_SIZE, "cp%s",
			    codepages[i].hostcp);
		}
		s->is_dbcs = codepages[i].is_dbcs;
		break;
	    }
	}
	/* Check for a 'cpXXX' match. */
	if (!strncmp(buf, "cp", 2) && strlen(buf) > 2) {
	    u = strtoul(buf + 2, &ptr, 10);
	    if (u > 0 && *ptr == '\0') {
		unsigned k;
		bool matched = false;

		for (k = 0; k < num_codepages; k++) {
		    if (strcmp(codepages[k].name, "bracket") &&
				u == atoi(codepages[k].hostcp)) {
			snprintf(s->codepage, STR_SIZE, "cp%s",
				codepages[k].hostcp);
			s->is_dbcs = codepages[k].is_dbcs;
			matched = true;
			break;
		    }
		}
		if (matched) {
		    break;
		}
	    }
	}

	if (codepages[i].name != NULL) {
	    break;
	}
	errout("\nInvalid code page name.");
    }

    if (!was_dbcs && s->is_dbcs) {
	dbcs_check();
    }

    return 0;
}

/**
 * Prompt for crosshair cursor mode.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_crosshair(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Crosshair Cursor\n\
\n\
This option causes wc3270 to use a crosshair cursor.");

    do {
	printf("\nCrosshair cursor? [%s] ",
		(s->flags & WF_CROSSHAIR)? "y" : "n");
	fflush(stdout);
	rc = getyn((s->flags & WF_CROSSHAIR) != 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags |= WF_CROSSHAIR;
	    break;
	case FALSE:
	    s->flags &= ~WF_CROSSHAIR;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for alternate cursor mode.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_cursor_type(session_t *s)
{
    char inbuf[STR_SIZE];

    new_screen(s, NULL, "\
Cursor Type\n\
\n\
This option controls whether the wc3270 cursor is a block or an underscore.");

    do {
	printf("\nCursor type? (block/underscore) [%s] ",
		(s->flags & WF_ALTCURSOR)? "underscore" : "block");
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    break;
	}
	if (!strncasecmp(inbuf, "quit", strlen(inbuf))) {
	    return -1;
	}
	if (!strncasecmp(inbuf, "underscore", strlen(inbuf))) {
	    s->flags |= WF_ALTCURSOR;
	    break;
	}
	if (!strncasecmp(inbuf, "block", strlen(inbuf))) {
	    s->flags &= ~WF_ALTCURSOR;
	    break;
	}
	errout("\nPlease answer 'underscore' or 'block'.");
    } while (true);
    return 0;
}

/**
 * Prompt for cursor blink.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_cursor_blink(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Cursor Blink\n\
\n\
This option controls whether the wc3270 cursor blinks.");

    do {
	printf("\nCursor blink? (y/n) [%s] ",
		(s->flags2 & WF2_CURSOR_BLINK)? "y": "n");
	fflush(stdout);
	switch ((rc = getyn((s->flags2 & WF2_CURSOR_BLINK)? TRUE: FALSE))) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags2 |= WF2_CURSOR_BLINK;
	    return 0;
	case FALSE:
	    s->flags2 &= ~WF2_CURSOR_BLINK;
	    return 0;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for TLS tunnel mode.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_tls(session_t *s)
{
    new_screen(s, NULL, "\
TLS Tunnel\n\
\n\
This option causes wc3270 to first create a tunnel to the host using\n\
Transport Layer Security (TLS); then it runs the TN3270 session inside the\n\
tunnel.\n\
\n\
TLS is also known as the Secure Sockets Layer (SSL).");

    do {
	printf("\nUse a TLS tunnel? (y/n) [%s] ", s->tls? "y" : "n");
	fflush(stdout);
	s->tls = getyn(s->tls);
	if (s->tls == YN_ERR) {
	    return -1;
	}
    } while (s->tls < 0);
    return 0;
}

/**
 * Prompt for verify-host-certificate mode
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error
 */
static int
get_verify(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Verify Host Certificates\n\
\n\
This option causes wc3270 to verify the certificates presented by the host\n\
if a TLS tunnel is used, or if the TELNET TLS option is negotiated.  If the\n\
certificates are not valid, the connection will be aborted.");

    if (!(s->flags2 & WF2_NEW_VHC_DEFAULT)) {
	yellowout("\n\
Note: The default for this option has changed from 'n' to 'y'.\n");
    }

    do {
	printf("\nVerify host certificates? (y/n) [%s] ",
		(s->flags2 & WF2_NO_VERIFY_HOST_CERT)? "n" : "y");
	fflush(stdout);
	rc = getyn((s->flags2 & WF2_NO_VERIFY_HOST_CERT) == 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags2 &= ~WF2_NO_VERIFY_HOST_CERT;
	    s->flags2 |= WF2_NEW_VHC_DEFAULT;
	    break;
	case FALSE:
	    s->flags2 |= WF2_NO_VERIFY_HOST_CERT;
	    s->flags2 |= WF2_NEW_VHC_DEFAULT;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for accept host name.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error
 */
static int
get_accept(session_t *s)
{
    char inbuf[STR_SIZE];

    new_screen(s, NULL, "\
Accept host name\n\
\n\
This option specifies the name to match against in the TLS certificate\n\
provided by the host. It is needed if the certificate contains a different\n\
name than the one used to connect to it.\n\
\n\
To remove an existing accept host name, enter '" CHOICE_NONE "'.");

    while (true) {
	if (s->accept_hostname[0]) {
	    printf("\nAccept host name: [%s] ", s->accept_hostname);
	} else {
	    printf("\nAccept host name: ");
	}
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!strcmp(inbuf, CHOICE_NONE)) {
	    memset(s->accept_hostname, 0, STR_SIZE);
	    break;
	} else if (!inbuf[0]) {
	    break;
	}
	if (strchr(inbuf, ' ') != NULL) {
	    errout("\nName cannot contain spaces.");
	    continue;
	}
	strcpy(s->accept_hostname, inbuf);
	break;
    }
    return 0;
}

/**
 * Prompt for proxy server name
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_proxy_server(session_t *s)
{
    char hbuf[STR_SIZE];

    /* Get the hostname. */
    for (;;) {
	if (s->proxy_host[0]) {
	    printf("\nProxy server name: [%s] ", s->proxy_host);
	} else {
	    printf("\nProxy server name: ");
	}
	if (get_input(hbuf, STR_SIZE) == NULL) {
	    return -1;
	}
	if (!hbuf[0]) {
	    if (s->proxy_host[0]) {
		break;
	    } else {
		continue;
	    }
	}
	if (strchr(hbuf, '[') != NULL || strchr(hbuf, ']') != NULL) {
	    errout("\nServer name cannot include '[' or ']'.");
	    continue;
	}
	strcpy(s->proxy_host, hbuf);
	break;
    }
    return 0;
}

/**
 * Find a proxy descriptor.
 *
 * @param[in] name	Proxy name
 *
 * @return proxy descriptor, or NULL
 */
static struct proxy_desc *
find_proxy(char *name)
{
    int i;

    for (i = 0; proxies[i].name != NULL; i++) {
	if (!strcmp(name, proxies[i].name)) {
	    return &proxies[i];
	}
    }
    return NULL;
}

/**
 * Prompt for proxy server port
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_proxy_server_port(session_t *s)
{
    struct proxy_desc *d;
    char pbuf[STR_SIZE];

    if ((d = find_proxy(s->proxy_type)) == NULL) {
	errout("Internal error\n");
	return -1;
    }

    for (;;) {
	unsigned long l;
	char *ptr;

	if (s->proxy_port[0]) {
	    printf("\nProxy server TCP port: [%s] ", s->proxy_port);
	} else if (d->port != NULL) {
	    printf("\nProxy server TCP port: [%s] ", d->port);
	} else {
	    printf("\nProxy server TCP port: ");
	}
	if (get_input(pbuf, STR_SIZE) == NULL) {
	    return -1;
	} else if (!strcmp(pbuf, "default") && d->port != NULL) {
	    strcpy(s->proxy_port, d->port);
	    break;
	} else if (!pbuf[0]) {
	    if (s->proxy_port[0]) {
		break;
	    } else if (d->port != NULL) {
		strcpy(s->proxy_port, d->port);
		break;
	    } else {
		continue;
	    }
	}
	l = strtoul(pbuf, &ptr, 10);
	if (l == 0 || *ptr != '\0' || (l & ~0xffffL)) {
	    errout("\nInvalid port.");
	} else {
	    strcpy(s->proxy_port, pbuf);
	    break;
	}
    }
    return 0;
}

/**
 * Prompt for proxy type
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_proxy(session_t *s)
{
    int i;
    char tbuf[STR_SIZE];
    char old_proxy[STR_SIZE];
    struct proxy_desc *d;

    new_screen(s, NULL, "\
Proxy\n\
\n\
If you do not have a direct connection to your host, this option allows\n\
wc3270 to use a proxy server to make the connection.");

    printf("\nProxy types available:\n");
    printf(" 1. none      Direct connection to host\n");
    for (i = 0; proxies[i].name != NULL; i++) {
	printf(" %d. %-8s  %s\n",
		i + 2,
		proxies[i].name,
		proxies[i].protocol);
    }

    strcpy(old_proxy, s->proxy_type);

    /* Get the proxy type. */
    for (;;) {
	int n;

	printf("\nProxy type: [%s] ",
		s->proxy_type[0]? s->proxy_type: CHOICE_NONE );
	if (get_input(tbuf, STR_SIZE) == NULL) {
	    return -1;
	} else if (!tbuf[0]) {
	    return 0;
	} else if (!strcasecmp(tbuf, CHOICE_NONE)) {
	    s->proxy_type[0] = '\0';
	    s->proxy_host[0] = '\0';
	    s->proxy_port[0] = '\0';
	    return 0;
	}
	d = find_proxy(tbuf);
	if (d != NULL) {
	    strcpy(s->proxy_type, tbuf);
	    break;
	}
	n = atoi(tbuf);
	if (n > 0 && n <= i+1) {
	    if (n == 1) {
		s->proxy_type[0] = '\0';
		s->proxy_host[0] = '\0';
		s->proxy_port[0] = '\0';
		s->proxy_user[0] = '\0';
		s->proxy_password[0] = '\0';
		return 0;
	    } else {
		d = &proxies[n - 2];
		strcpy(s->proxy_type, d->name);
		break;
	    }
	}
	errout("\nInvalid proxy type.");
    }

    /* If the type changed, the rest of the information is invalid. */
    if (strcmp(old_proxy, s->proxy_type)) {
	s->proxy_host[0] = '\0';
	s->proxy_port[0] = '\0';
	s->proxy_user[0] = '\0';
	s->proxy_password[0] = '\0';

	if (get_proxy_server(s) < 0) {
	    return -1;
	}

	if (d->port != NULL) {
	    strcpy(s->proxy_port, d->port);
	} else if (get_proxy_server_port(s) < 0) {
	    return -1;
	}
    }

    return 0;
}

/**
 * Prompt for proxy user name.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_proxy_user(session_t *s)
{
    struct proxy_desc *d;
    char pbuf[STR_SIZE];

    if ((d = find_proxy(s->proxy_type)) == NULL) {
	errout("Internal error\n");
	return -1;
    }

    new_screen(s, NULL, "\
Proxy User Name\n\
\n\
Enter the optional user name to use with the proxy. To specify no user name,\n\
enter 'none'.");

    for (;;) {
	if (s->proxy_user[0]) {
	    printf("\nProxy user name: [%s] ", s->proxy_user);
	} else {
	    printf("\nProxy user name: ");
	}
	if (get_input(pbuf, STR_SIZE) == NULL) {
	    return -1;
	} else if (!pbuf[0]) {
	    if (s->proxy_user[0]) {
		break;
	    }
	    continue;
	}
	if (!strcmp(pbuf, "none")) {
	    s->proxy_user[0] = '\0';
	    break;
	} else if (strchr(pbuf, ':') || strchr(pbuf, '@')) {
	    errout("\nInvalid user name.");
	} else {
	    strcpy(s->proxy_user, pbuf);
	    break;
	}
    }
    return 0;
}

/**
 * Prompt for proxy password.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_proxy_password(session_t *s)
{
    struct proxy_desc *d;
    char pbuf[STR_SIZE];

    if ((d = find_proxy(s->proxy_type)) == NULL) {
	errout("Internal error\n");
	return -1;
    }

    new_screen(s, NULL, "\
Proxy Password\n\
\n\
Enter the optional password to use with the proxy. To specify no password,\n\
enter 'none'.\n\
\n\
The password will not be echoed.");

    for (;;) {
	char *input;

	if (s->proxy_password[0]) {
	    printf("\nProxy password: [***] ");
	} else {
	    printf("\nProxy password: ");
	}
	SetConsoleMode(conin_handle, ENABLE_LINE_INPUT |
		ENABLE_PROCESSED_INPUT);
	input = get_input(pbuf, STR_SIZE);
	SetConsoleMode(conin_handle, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
		ENABLE_PROCESSED_INPUT);
	if (input == NULL) {
	    return -1;
	} else if (!pbuf[0]) {
	    if (s->proxy_password[0]) {
		break;
	    }
	    continue;
	}
	if (!strcmp(pbuf, "none")) {
	    s->proxy_password[0] = '\0';
	    break;
	} else if (strchr(pbuf, ':') || strchr(pbuf, '@')) {
	    errout("\nInvalid password.");
	} else {
	    strcpy(s->proxy_password, pbuf);
	    break;
	}
    }
    return 0;
}

/**
 * Prompt for pr3287 session
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_pr3287(session_t *s)
{
    new_screen(s, NULL, "\
pr3287 Session\n\
\n\
This option allows wc3270 to automatically start a pr3287 printer session\n\
when it connects to the host, allowing the host to direct print jobs to a\n\
Windows printer.");

    do {
	printf("\nAutomatically start a pr3287 printer session? (y/n) [n] ");
	fflush(stdout);
	s->wpr3287 = getyn(s->wpr3287);
	if (s->wpr3287 == YN_ERR) {
	    return -1;
	}
    } while (s->wpr3287 < 0);
    if (s->wpr3287 == 0) {
	strcpy(s->printerlu, ".");
    }
    return 0;
}

/**
 * Prompt for pr3287 session mode (associate/LU)
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_printer_mode(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
pr3287 Session -- Printer Mode\n\
\n\
The pr3287 printer session can be configured in one of two ways.  The first\n\
method automatically associates the printer session with the current login\n\
session.  The second method specifies a particular Logical Unit (LU) to use\n\
for the printer session.");

    do {
	printf("\nAssociate the printer session with the current login "
		"session (y/n) [%s]: ",
		strcmp(s->printerlu, ".")? "n": "y");
	fflush(stdout);
	rc = getyn(!strcmp(s->printerlu, "."));
	switch (rc) {
	case YN_ERR:
	    return -1;
	case FALSE:
	    if (!strcmp(s->printerlu, ".")) {
		s->printerlu[0] = '\0';
	    }
	    break;
	case TRUE:
	    strcpy(s->printerlu, ".");
	    break;
	}
    } while (rc < 0);

    if (strcmp(s->printerlu, ".") && get_printerlu(s, 0) < 0) {
	return -1;
    }
    return 0;
}

/**
 * Prompt for pr3287 session LU name
 *
 * @param[in,out] s	Session
 * @param[in] explain	True if the screen needs a header
 *
 * @return 0 for success, -1 for failure
 */
static int
get_printerlu(session_t *s, int explain)
{
    if (explain) {
	new_screen(s, NULL, "\
pr3287 Session -- Printer Logical Unit (LU) Name\n\
\n\
If the pr3287 printer session is associated with a particular Logical Unit,\n\
then that Logical Unit must be configured explicitly.");
    }

    for (;;) {
	char tbuf[STR_SIZE];

	if (s->printerlu[0]) {
	    printf("\nEnter printer Logical Unit (LU) name: [%s] ",
		    s->printerlu);
	} else {
	    printf("\nEnter printer Logical Unit (LU) name: ");
	}
	fflush(stdout);
	if (get_input(tbuf, STR_SIZE) == NULL) {
	    return -1;
	}
	if (!tbuf[0]) {
	    if (s->printerlu[0]) {
		break;
	    } else {
		continue;
	    }
	} else {
	    strcpy(s->printerlu, tbuf);
	    break;
	}
    }

    return 0;
}

/**
 * Prompt for pr3287 session printer name or folder
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_printer(session_t *s)
{
    char tbuf[STR_SIZE];
    unsigned i;
    char *ptr;
    unsigned long u;
    char cbuf[STR_SIZE];
    int matching_printer = -1;
    int matching_option = 1;
    int other_option = -1;
    int extra_needed = 0;

    new_screen(s, NULL, "\
pr3287 Session -- Windows Printer Name or Save Folder\n\
\n\
The pr3287 session can use the Windows default printer as its real printer,\n\
or you can specify a particular Windows printer. You can specify a local\n\
rinter, or a remote printer with a UNC path such as '\\\\server\\printer22'.\n\
\n\
pr3287 can also save documents as text files, which you can specify by\n\
selecting 'Other' and giving the full pathname of a folder to save them in.");

    enum_printers();
    redisplay_printer(s->printer, cbuf);

    printf("\nOptions:\n");
    printf(" %2d. System default printer", 1);
    if (default_printer[0]) {
	printf(" (currently %s)", default_printer);
    }
    printf("\n");

    if (num_printers) {
	for (i = 0; i < num_printers; i++) {
	    printf(" %2d. Printer: %s\n", i + 2, printer_info[i].pName);
	    if (!strcasecmp(cbuf, printer_info[i].pName)) {
		matching_printer = i;
		matching_option = i + 2;
	    }
	}
    }

    if (s->printer[0]) {
	if (printer_is_folder(s)) {
	    matching_option = num_printers + 2;
	    printf(" %2d. Save Folder: %s\n", matching_option, cbuf);
	    extra_needed = 1;
	} else if (matching_printer == -1) {
	    matching_option = num_printers + 2;
	    printf(" %2d. Printer: %s\n", matching_option, cbuf);
	    extra_needed = 1;
	}
    }
    other_option = num_printers + 2 + extra_needed;
    printf(" %2d. Other printer or save folder\n", other_option);
    for (;;) {
	printf("\nEnter option (1-%d): [%d] ", other_option, matching_option);
	fflush(stdout);
	if (get_input(tbuf, STR_SIZE) == NULL || !strcasecmp(tbuf, "quit")) {
	    return -1;
	}
	if (!tbuf[0]) {
	    return 0;
	}
	u = strtoul(tbuf, &ptr, 10);
	if (*ptr != '\0' || u < 1 || u > (unsigned long)other_option) {
	    continue;
	}
	if (u == 1) {
	   s->printer[0] = 0;
	   break;
	}
	if (u == matching_option) {
	    return 0;
	}
	if (u == other_option) {
	    if (get_printer_name(cbuf, tbuf, STR_SIZE) < 0) {
		return -1;
	    }
	    strcpy(s->printer, tbuf);
	    break;
	}
	strcpy(s->printer, printer_info[u - 2].pName);
	break;
    }

    /* Double any backslashes. */
    fixup_backslashes(s);
    return 0;
}

/**
 * Prompt for pr3287 session printer code page
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_printercp(session_t *s)
{
    char buf[STR_SIZE];

    new_screen(s, NULL, "\
pr3287 Session -- Printer Code Page\n\
\n\
By default, pr3287 uses the system's default ANSI code page.  You can\n\
override that code page here, or specify 'default' to use the system ANSI code\n\
page.");

    for (;;) {
	int cp;

	printf("\nPrinter code page [%s]: ",
		s->printercp[0]? s->printercp: "default");
	fflush(stdout);
	if (get_input(buf, STR_SIZE) == NULL) {
	    return -1;
	} else if (!buf[0]) {
	    break;
	} else if (!strcmp(buf, "default")) {
	    s->printercp[0] = '\0';
	    break;
	}
	cp = atoi(buf);
	if (cp <= 0) {
	    errout("\nInvald code page.");
	} else {
	    strcpy(s->printercp, buf);
	    break;
	}
    }

    return 0;
}

/**
 * Prompt for keymap names
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_keymaps(session_t *s)
{
    km_t *km;

    new_screen(s, NULL, "\
Keymaps\n\
\n\
A keymap is a mapping from the PC keyboard to the virtual 3270 keyboard.\n\
You can override the default keymap and specify one or more built-in or \n\
user-defined keymaps, separated by commas.");

    printf("\n");

    for (km = km_first; km != NULL; km = km->next) {
	printf(" %s\n", km->name);
	if (km->description[0]) {
	    printf("  %s", km->description);
	} else {
	    printf("  (no description)");
	}
	printf("\n");
    }

    for (;;) {
	char inbuf[STR_SIZE];
	char tknbuf[STR_SIZE];
	char *t;
	char *buf;
	bool wrong = false;

	printf("\nEnter keymap name(s) [%s]: ",
		s->keymaps[0]? s->keymaps: CHOICE_NONE);
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	} else if (!strcmp(inbuf, CHOICE_NONE)) {
	    s->keymaps[0] = '\0';
	    break;
	}
	strcpy(tknbuf, inbuf);
	wrong = false;
	buf = tknbuf;
	while (!wrong && (t = strtok(buf, ",")) != NULL) {
	    buf = NULL;
	    for (km = km_first; km != NULL; km = km->next) {
		if (!strcasecmp(t, km->name)) {
		    break;
		}
	    }
	    if (km == NULL) {
		errout("Invalid keymap name '%s'.", t);
		wrong = true;
		break;
	    }
	}
	if (!wrong) {
	    strcpy(s->keymaps, inbuf);
	    break;
	}
    }
    return 0;
}

/**
 * Prompt for keymap embedding (copying keymaps into session file)
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_embed(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Embed Keymaps\n\
\n\
If selected, this option causes any selected keymaps to be copied into the\n\
session file, instead of being found at runtime.");

    do {
	printf("\nEmbed keymaps? (y/n) [%s] ",
		(s->flags & WF_EMBED_KEYMAPS)? "y": "n");
	fflush(stdout);
	rc = getyn((s->flags & WF_EMBED_KEYMAPS) != 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags |= WF_EMBED_KEYMAPS;
	    break;
	case FALSE:
	    s->flags &= ~WF_EMBED_KEYMAPS;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for screen font
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_font(session_t *s)
{
    CHOOSEFONT ch;
    LOGFONT lf;
    HDC dc = GetDC(NULL);
    int ps;

    new_screen(s, NULL, "Opening font dialog in separate window");

    memset(&ch, 0, sizeof(ch));
    ch.lStructSize = sizeof(ch);
    ch.lpLogFont = &lf;
    ch.Flags = CF_INITTOLOGFONTSTRUCT | CF_FIXEDPITCHONLY | CF_FORCEFONTEXIST |
	CF_TTONLY | CF_LIMITSIZE | CF_NOSCRIPTSEL | CF_NOVERTFONTS;
    ch.nSizeMin = 5;
    ch.nSizeMax = 72;
    memset(&lf, 0, sizeof(lf));
    ps = s->point_size? s->point_size: 12;
    lf.lfHeight = -MulDiv(ps, GetDeviceCaps(dc, LOGPIXELSY), 72);
    lf.lfWeight = s->font_weight? s->font_weight: 400;
    if (s->font_name[0]) {
	strcpy(lf.lfFaceName, s->font_name);
    } else {
	int cp;

	WideCharToMultiByte(CP_ACP, 0, reg_font_from_host_codepage(s->font_name, s->codepage, &cp, errout),
		-1, lf.lfFaceName, STR_SIZE, NULL, NULL);
    }
    if (ChooseFont(&ch)) {
	strncpy(s->font_name, lf.lfFaceName, STR_SIZE);
	s->font_name[sizeof(s->font_name) - 1] = '\0';
	s->point_size = ch.iPointSize / 10;
	s->font_weight = lf.lfWeight;
	if (lf.lfItalic) {
	    yellowout("Warning: Ignoring italic/oblique property in font\n");
	    ask_enter();
	}
    }
    return 0;
}

/**
 * Prompt for screen background color
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_background(session_t *s)
{
    new_screen(s, NULL, "\
Background Color\n\
\n\
This option selects whether the screen background is black (the default) or\n\
white.");

    for (;;) {
	char inbuf[STR_SIZE];

	printf("\nBackground color? (black/white) [%s] ",
		(s->flags & WF_WHITE_BG)? "white": "black");
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	} else if (!strcasecmp(inbuf, "black")) {
	    s->flags &= ~WF_WHITE_BG;
	    break;
	} else if (!strcasecmp(inbuf, "white")) {
	    s->flags |= WF_WHITE_BG;
	    break;
	}
	errout("\nPlease answer 'black' or 'white'.");
    }
    return 0;
}

/**
 * Prompt for menubar mode
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_menubar(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Menu Bar\n\
\n\
This option selects whether the menu bar is displayed on the screen.");

    do  {
	printf("\nDisplay menu bar? (y/n) [%s] ",
		(s->flags & WF_NO_MENUBAR)? "n": "y");
	fflush(stdout);
	rc = getyn(!(s->flags & WF_NO_MENUBAR));
	switch (rc) {
	case YN_ERR:
	    return -1;
	case FALSE:
	    s->flags |= WF_NO_MENUBAR;
	    break;
	case TRUE:
	    s->flags &= ~WF_NO_MENUBAR;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for trace-at-startup mode
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_trace(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Tracing\n\
\n\
This option causes wc3270 to begin tracing at start-up. The trace file will\n\
be left on your desktop.");

    do {
	printf("\nTrace at start-up? (y/n) [%s] ",
		(s->flags & WF_TRACE)? "y" : "n");
	fflush(stdout);
	rc = getyn((s->flags & WF_TRACE) != 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags |= WF_TRACE;
	    break;
	case FALSE:
	    s->flags &= ~WF_TRACE;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Prompt for always insert mode
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_always_insert(session_t *s)
{
    int rc;

    new_screen(s, NULL, "\
Default to Insert Mode\n\
\n\
This option causes wc3270 to use insert mode by default.");

    do {
	printf("\nDefault to insert mode? (y/n) [%s] ",
		(s->flags2 & WF2_ALWAYS_INSERT)? "y" : "n");
	fflush(stdout);
	rc = getyn((s->flags2 & WF2_ALWAYS_INSERT) != 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags2 |= WF2_ALWAYS_INSERT;
	    break;
	case FALSE:
	    s->flags2 &= ~WF2_ALWAYS_INSERT;
	    break;
	}
    } while (rc < 0);
    return 0;
}

/**
 * Run Notepad on the session file, allowing arbitrary resources to be
 * edited.
 *
 * @param[in] s		Session
 * @param[in,out] us	User settings
 *
 * @return 0 for success, -1 for failure
 */
static int
run_notepad(session_t *s, char **us)
{
    int rc;
    char *t = NULL;
    char cmd[MAX_PATH + 64];
    FILE *f;
    char *new_us;
    char buf[2];

    new_screen(s, NULL, "\
Notepad\n\
\n\
This option will start up the Windows Notepad editor to allow you to edit\n\
miscellaneous resources in your session file.");

    do {
	printf("\nProceed? (y/n) [y] ");
	fflush(stdout);
	rc = getyn(TRUE);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case FALSE:
	    return 0;
	case TRUE:
	    break;
	}
    } while (rc < 0);

    t = _tempnam(NULL, "w3270wiz");
    if (t == NULL) {
	errout("Error creating temporary session file name.\n");
	goto failed;
    }
    f = fopen(t, "w");
    if (f == NULL) {
	errout("Error creating temporary session file: %s\n",
		strerror(errno));
	goto failed;
    }
    fprintf(f, "! Comment lines begin with '!', like this one.\n\
! Resource values look like this (without the '!'):\n\
!  wc3270.printTestScreensPerPage: 3\n");
    write_user_settings(*us, f);
    fclose(f);
    f = NULL;

    printf("Starting Notepad... ");
    fflush(stdout);
    snprintf(cmd, sizeof(cmd), "start/wait notepad.exe \"%s\"", t);
    system(cmd);
    printf("done\n");

    f = fopen(t, "r");
    if (f == NULL) {
	errout("Error reading back temporary session file: %s\n",
		strerror(errno));
	goto failed;
    }
    new_us = NULL;
    if (read_user_settings(f, &new_us) == 0) {
	errout("Error reading back temporary session file.\n");
	goto failed;
    }
    fclose(f);
    if (*us != NULL) {
	free(*us);
    }
    *us = new_us;
    unlink(t);
    free(t);
    return 0;

failed:
    grayout("[Press <Enter>] ");
    fflush(stdout);
    fgets(buf, 2, stdin);
    if (t != NULL) {
	free(t);
    }
    return -1;
}

typedef enum {
    SP_REPLACE,	/* replace uneditable file */
    SP_CREATE,	/* create new file */
    SP_UPDATE,	/* update editable file */
    N_SP
} sp_t;

static char *how_name[N_SP] = {
    "Replace",
    "Create",
    "Update"
};

/**
 * Prompt for where a session file should go (all-users or current user's
 * Docs).
 *
 * @param[in] name	Session name
 * @param[in] def	Default folder
 *
 * @return selected folder
 */
static src_t
get_src(const char *name, src_t def)
{
    char ac[STR_SIZE];
    src_t src_out = def;

    /* Ask where they want the file. */
    if (admin()) {
	for (;;) {
	    printf("\nCreate '%s' in My Documents or Public Documents? "
		    "(my/public) [%s] ",
		    name, (def == SRC_PUBLIC_DOCUMENTS)? "public": "my");
	    fflush(stdout);
	    if (get_input(ac, STR_SIZE) == NULL) {
		return SRC_ERR;
	    } else if (!ac[0]) {
		break;
	    } else if (!strncasecmp(ac, "public", strlen(ac))) {
		src_out = SRC_PUBLIC_DOCUMENTS;
		break;
	    } else if (!strncasecmp(ac, "my", strlen(ac)) ||
		       !strcasecmp(ac, username)) {
		src_out = SRC_DOCUMENTS;
		break;
	    } else if (!strncasecmp(ac, "quit", strlen(ac))) {
		return SRC_NONE;
	    } else {
		errout("\nPlease answer 'my' or 'public'.");
	    }
	}
    } else {
	return SRC_DOCUMENTS;
    }

    /* Make sure the subfolder exists. */
    create_wc3270_folder(src_out);
    return src_out;
}

static const char *
weight_name(int weight)
{
    static struct {
	int weight;
	const char *name;
    } names[] = {
	{ 100, "Thin" },
	{ 200, "Extra Light" },
	{ 300, "Light" },
	{ 400, "Regular" },
	{ 500, "Medium" },
	{ 600, "Semi-Bold" },
	{ 700, "Bold" },
	{ 800, "Extra Bold" },
	{ 900, "Black" },
	{ 0, NULL },
    };
    int i;

    for (i = 0; names[i].name != NULL; i++) {
	if (weight == names[i].weight ||
		(names[i + 1].name != NULL && weight < names[i + 1].weight)) {
	    return names[i].name;
	}
    }
    return "Black";
}

/**
 * Return the default font for the current codepage, as a multi-byte string.
 *
 * @param[in] s		Session
 * @return font name
 */
static const char *
default_font(session_t *s)
{
    static char font[STR_SIZE];
    int cp;

    WideCharToMultiByte(CP_ACP, 0, reg_font_from_host_codepage("", s->codepage, &cp, errout), -1, font, STR_SIZE, NULL, NULL);
    return font;
}

/**
 * Display the current settings for a session and allow them to be edited.
 *
 * @param[in,out] s	Session
 * @param[in,out] us	User settings
 * @param[in] how	How session is being edited (replace/create/update)
 * @param[in] path	Session pathname
 * @param[in] session_name Name of session
 * @param[out] change_shortcut Returned as true if the shortcut should be
 *                             changed
 * @param[in] modified	True if session is already modified
 *
 * @return 0 for success, -1 for failure
 */
static src_t
edit_menu(session_t *s, char **us, sp_t how, const char *path,
	const char *session_name, bool *change_shortcut, bool modified)
{
    int rc;
    char choicebuf[32];
    session_t old_session;
    char *old_us = NULL;
    src_t ret = SRC_NONE;
    struct proxy_desc *d;

    *change_shortcut = false;

    switch (how) {
    case SP_REPLACE:
    case SP_CREATE:
    case N_SP: /* can't happen, but the compiler wants it */
	memset(&old_session, '\0', sizeof(session_t));
	break;
    case SP_UPDATE:
	memcpy(&old_session, s, sizeof(session_t));
	break;
    }

    /* Save a copy of the original user settings. */
    if (*us != NULL) {
	old_us = strdup(*us);
	if (old_us == NULL) {
	    errout("Out of memory.\n");
	    ask_enter();
	    exit(1);
	}
    }

    for (;;) {
	int done = 0;
	char *cp = "?";
	int i;

	/* Look up the codepage. */
	if (!strcmp(s->codepage, "bracket")) {
	    cp = "CP 37+";
	} else if (!strncmp(s->codepage, "cp", 2)) {
	    for (i = 0; codepages[i].name != NULL; i++) {
		if (!strcmp(codepages[i].name, "bracket")) {
		    continue;
		}
		if (!strcmp(codepages[i].hostcp, s->codepage + 2)) {
		    cp = codepages[i].name;
		    break;
		}
	    }
	}

	new_screen(s, (how == SP_CREATE)? NULL: path, "Options");

	printf("%3d. Host ................... : %s\n", MN_HOST,
		strcmp(s->host, CHOICE_NONE)? s->host: DISPLAY_NONE);
	printf("%3d. Logical Unit Name ...... : %s\n", MN_LU,
		s->luname[0]? s->luname: DISPLAY_NONE);
	printf("%3d. TCP Port ............... : %d\n", MN_PORT,
		(int)s->port);
	printf("%3d. Model Number ........... : %d "
		"(%d rows x %d columns)\n", MN_MODEL,
		(int)s->model, wrows[s->model], wcols[s->model]);
	printf("%3d.  Oversize .............. : ", MN_OVERSIZE);
	if (s->ov_rows || s->ov_cols) {
	    printf("%u rows x %u columns\n", s->ov_rows, s->ov_cols);
	} else {
	    printf(DISPLAY_NONE"\n");
	}
	printf("%3d. Code Page .............. : %s (%s)\n",
		MN_CODEPAGE, s->codepage, cp);
	printf("%3d. Crosshair Cursor ....... : %s\n",
		MN_CROSSHAIR, (s->flags & WF_CROSSHAIR)? "Yes": "No");
	printf("%3d. Cursor Type ............ : %s\n",
		MN_CURSORTYPE, (s->flags & WF_ALTCURSOR)?
		    "Underscore": "Block");
	printf("%3d. Cursor Blink ........... : %s\n",
		MN_CURSORBLINK, (s->flags2 & WF2_CURSOR_BLINK)?
		    "Yes": "No");
	printf("%3d. TLS (SSL) Tunnel ....... : %s\n", MN_TLS,
		s->tls? "Yes": "No");
	printf("%3d. Verify host certificates : %s", MN_VERIFY,
		(s->flags2 & WF2_NO_VERIFY_HOST_CERT)? "No": "Yes");
	fflush(stdout);
	if (!(s->flags2 & WF2_NEW_VHC_DEFAULT)) {
	    yellowout(" [default has changed]");
	}
	printf("\n");
	printf("%3d. Accept host name ....... : %s\n", MN_ACCEPT,
		s->accept_hostname[0]? s->accept_hostname: DISPLAY_NONE);
	printf("%3d. Proxy .................. : %s\n", MN_PROXY,
		s->proxy_type[0]? s->proxy_type: DISPLAY_NONE);
	if (s->proxy_type[0]) {
	    d = find_proxy(s->proxy_type);
	    printf("%3d.  Proxy Server .......... : %s\n",
		    MN_PROXY_SERVER, s->proxy_host);
	    if (s->proxy_port[0]) {
		printf("%3d.  Proxy Server TCP Port . : %s\n",
			MN_PROXY_PORT, s->proxy_port);
	    }
	    if (d != NULL && d->user) {
		printf("%3d.  Proxy user name ....... : %s\n",
			MN_PROXY_USER,
			s->proxy_user[0]? s->proxy_user: "(none)");
		if (s->proxy_user[0]) {
		    printf("%3d.  Proxy password ........ : %s\n",
			    MN_PROXY_PASSWORD,
			    s->proxy_password[0]? "***": "(none)");
		}
	    }
	}
	printf("%3d. pr3287 Printer Session . : %s\n", MN_3287,
		s->wpr3287? "Yes": "No");
	if (s->wpr3287) {
	    char pbuf[STR_SIZE];

	    printf("%3d.  pr3287 Mode ........... : ",
		    MN_3287_MODE);
	    if (!strcmp(s->printerlu, ".")) {
		printf("Associate\n");
	    } else {
		printf("LU\n");
		printf("%3d.  pr3287 LU ............. : %s\n",
			MN_3287_LU, s->printerlu);
	    }
	    redisplay_printer(s->printer, pbuf);
	    if (printer_is_folder(s)) {
		printf("%3d.  pr3287 Save Folder .... : %s\n",
			MN_3287_PRINTER, pbuf);
	    } else {
		printf("%3d.  pr3287 Windows printer  : %s\n",
			MN_3287_PRINTER,
			s->printer[0]? pbuf: "(system default)");
	    }
	    printf("%3d.  pr3287 Code Page ...... : ",
		    MN_3287_CODEPAGE);
	    if (s->printercp[0]) {
		printf("%s\n", s->printercp);
	    } else {
		printf("(system ANSI default of %d)\n", GetACP());
	    }
	}
	printf("%3d. Keymaps ................ : %s\n", MN_KEYMAPS,
		s->keymaps[0]? s->keymaps: DISPLAY_NONE);
	if (s->keymaps[0]) {
	    printf("%3d.  Embed Keymaps ......... : %s\n",
		    MN_EMBED_KEYMAPS,
		    (s->flags & WF_EMBED_KEYMAPS)? "Yes": "No");
	}
	printf("%3d. Font ................... : %s %s %u\n",
		MN_FONT,
		s->font_name[0]? s->font_name: default_font(s),
		weight_name(s->font_weight? s->font_weight: 400),
		(unsigned int)(s->point_size? s->point_size: 12));
	printf("%3d. Background Color ....... : %s\n", MN_BG,
		(s->flags & WF_WHITE_BG)? "white": "black");
	printf("%3d. Menu Bar ............... : %s\n", MN_MENUBAR,
		(s->flags & WF_NO_MENUBAR)? "No": "Yes");
	printf("%3d. Trace at start-up ...... : %s\n", MN_TRACE,
		(s->flags & WF_TRACE)? "Yes": "No");
	printf("%3d. Always use insert mode . : %s\n", MN_ALWAYS_INSERT,
		(s->flags2 & WF2_ALWAYS_INSERT)? "Yes": "No");
	printf("%3d. Edit miscellaneous resources with Notepad\n",
		MN_NOTEPAD);

	for (;;) {
	    int invalid = 0;
	    int was_pr3287 = 0;

	    printf("\nEnter item number to change: [%s] ", CHOICE_NONE);
	    fflush(stdout);
	    if (get_input(choicebuf, sizeof(choicebuf)) == NULL) {
		ret = SRC_ERR;
		goto done;
	    } else if (!choicebuf[0] || !strcasecmp(choicebuf, CHOICE_NONE)) {
		/* none */
		done = 1;
		break;
	    }
	    if (!strncasecmp(choicebuf, "quit", strlen(choicebuf))) {
		ret = SRC_ERR;
		goto done;
	    }
	    switch (atoi(choicebuf)) {
	    case MN_HOST:
		if (get_host(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_LU:
		if (get_lu(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_PORT:
		if (get_port(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_MODEL:
		if (get_model(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_OVERSIZE:
		if (get_oversize(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_CODEPAGE:
		if (get_codepage(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_CROSSHAIR:
		if (get_crosshair(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_CURSORTYPE:
		if (get_cursor_type(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_CURSORBLINK:
		if (get_cursor_blink(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_TLS:
		if (get_tls(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_VERIFY:
		if (get_verify(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_ACCEPT:
		if (get_accept(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_PROXY:
		if (get_proxy(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_PROXY_SERVER:
		if (s->proxy_type[0]) {
		    if (get_proxy_server(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_PROXY_PORT:
		if (s->proxy_type[0]) {
		    if (get_proxy_server_port(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_PROXY_USER:
		if (s->proxy_type[0] &&
			(d = find_proxy(s->proxy_type)) != NULL &&
			d->user) {
		    if (get_proxy_user(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_PROXY_PASSWORD:
		if (s->proxy_type[0] &&
			s->proxy_user[0] &&
			(d = find_proxy(s->proxy_type)) != NULL &&
			d->user) {
		    if (get_proxy_password(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_3287:
		was_pr3287 = s->wpr3287;
		if (get_pr3287(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		if (s->wpr3287 && !was_pr3287) {
		    if (get_printer_mode(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		}
		break;
	    case MN_3287_MODE:
		if (s->wpr3287) {
		    if (get_printer_mode(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_3287_LU:
		if (s->wpr3287 && strcmp(s->printerlu, ".")) {
		    if (get_printerlu(s, 1) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_3287_PRINTER:
		if (s->wpr3287) {
		    if (get_printer(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_3287_CODEPAGE:
		if (s->wpr3287) {
		    if (get_printercp(s) < 0) {
			ret = SRC_ERR;
			goto done;
		    }
		} else {
		    errout("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_KEYMAPS:
		if (get_keymaps(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_EMBED_KEYMAPS:
		if (get_embed(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_FONT:
		if (get_font(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_BG:
		if (get_background(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_MENUBAR:
		if (get_menubar(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_TRACE:
		if (get_trace(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_ALWAYS_INSERT:
		if (get_always_insert(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    case MN_NOTEPAD:
		if (run_notepad(s, us) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    default:
		errout("\nInvalid entry.");
		invalid = 1;
		break;
	    }

	    if (!invalid) {
		break;
	    }
	}
	if (done) {
	    break;
	}
    }

    /*
     * Set the WF2_NEW_VHC_DEFAULT flag in the session, so even if they
     * don't change anything on an old session file, it will need to be
     * written back out.
     */
    s->flags2 |= WF2_NEW_VHC_DEFAULT;

    /* Ask if they want to write the file. */
    if (memcmp(s, &old_session, sizeof(session_t)) ||
	((old_us != NULL) ^ (*us != NULL)) ||
	(old_us != NULL && strcmp(old_us, *us)) ||
	modified) {

	for (;;) {
	    printf("\n%s session file '%s'? (y/n) [y] ",
		    how_name[how], session_name);
	    fflush(stdout);
	    rc = getyn(TRUE);
	    if (rc == YN_ERR) {
		ret = SRC_ERR;
		goto done;
	    } else if (rc == FALSE) {
		ret = SRC_NONE;
		goto done;
	    } else if (rc == TRUE) {
		break;
	    }
	}
    } else {
	ret = SRC_NONE;
	goto done;
    }

    /* If creating, ask where they want it written. */
    if (how == SP_CREATE) {
	ret = get_src(session_name, SRC_DOCUMENTS);
	goto done;
    }

    /* Return where the file ended up. */
    if (!strncasecmp(documents_wc3270, path, strlen(documents_wc3270))) {
	ret = SRC_DOCUMENTS;
	goto done;
    } else if (!strncasecmp(public_documents_wc3270, path,
		strlen(public_documents_wc3270))) {
	ret = SRC_PUBLIC_DOCUMENTS;
	goto done;
    } else if (!strncasecmp(desktop, path, strlen(desktop))) {
	ret = SRC_DESKTOP;
	goto done;
    } else if (!strncasecmp(public_desktop, path, strlen(public_desktop))) {
	ret = SRC_PUBLIC_DESKTOP;
	goto done;
    } else {
	ret = SRC_OTHER;
	goto done;
    }

done:
    {
	int old_codepage;
	const wchar_t *old_font = reg_font_from_host_codepage(old_session.font_name, old_session.codepage,
		&old_codepage, errout);
	int codepage;
	const wchar_t *font = reg_font_from_host_codepage(s->font_name, s->codepage, &codepage, errout);

	if (old_session.model != s->model ||
	    old_session.ov_rows != s->ov_rows ||
	    old_session.ov_cols != s->ov_cols ||
	    wcscmp(old_font, font) ||
	    old_codepage != codepage) {

	    *change_shortcut = true;
	}
    }

    if (old_us != NULL) {
	free(old_us);
    }
    return ret;
}

/**
 * Print the prefix for a session name (ordinal or blank)
 *
 * @param[in] n			Ordinal to display
 * @param[in] with_numbers 	If true, display number, otherwise blanks
 */
static void
print_n(int n, bool with_numbers)
{
    if (with_numbers) {
	printf(" %2d.", n + 1);
    } else {
	printf(" ");
    }
}

/**
 * Display the current set of sessions.
 *
 * @param[in] with_numbers	If true, display with ordinals
 * @param[in] include_public	If true, include public sessions
 */
static void
display_sessions(bool with_numbers, bool include_public)
{
    int i;
    int col = 0;
    const char *n;

    /*
     * Display the session names in four colums. Each 20-character column
     * looks like:
     * <space><nn><.><space><name>
     * So there is room for 15 characters of session name in the first
     * three columns, and 14 in the last column (since we avoid writing in
     * column 80 of the display).
     */
    for (i = 0; (n = xs_name(i + 1, NULL)) != NULL; i++) {
	size_t slen;

	if (i >= xs_my.count && !include_public) {
	    break;
	}

	if (i == 0 && xs_my.count != 0) {
	    printf("Sessions for user '%s' in %.*s:\n",
		    username,
		    (int)(strlen(documents_wc3270) - 1),
		    documents_wc3270);
	} else if (i == xs_my.count) {
	    if (col) {
		printf("\n");
		col = 0;
	    }
	    printf("Sessions for all users in %.*s:\n",
		    (int)(strlen(public_documents_wc3270) - 1),
		    public_documents_wc3270);
	    if (!with_numbers && !admin()) {
		yellowout("(run the Session Wizard as administrator to edit these sessions)\n");
	    }
	}

	slen = strlen(n);

    retry:
	switch (col) {
	default:
	case 0:
	    print_n(i, with_numbers);
	    printf(" %s", n);
	    if (slen <= 15) { /* fits in column 0 */
		printf("%*s", (int)(15 - slen), "");
		col = 1;
	    } else if (slen <= 15 + 20) { /* covers 0 and 1 */
		printf("%*s", (int)(15 + 20 - slen), "");
		col = 2;
	    } else if (slen <= 15 + 20 + 20) { /* covers 0, 1, 2 */
		printf("%*s", (int)(15 + 20 + 20 - slen), "");
		col = 3;
	    } else { /* whole line */
		printf("\n");
	    }
	    break;
	case 1:
	    if (slen > 15 + 20 + 19) { /* overflows */
		printf("\n");
		col = 0;
		goto retry;
	    }
	    print_n(i, with_numbers);
	    printf(" %s", n);
	    if (slen <= 15) { /* fits in column 1 */
		printf("%*s", (int)(15 - slen), "");
		col = 2;
	    } else if (slen <= 15 + 20) { /* covers 1 and 2 */
		printf("%*s", (int)(15 + 20 - slen), "");
		col = 3;
	    } else { /* rest of the line */
		printf("\n");
		col = 0;
	    }
	    break;
	case 2:
	    if (slen > 15 + 19) { /* overflows */
		printf("\n");
		col = 0;
		goto retry;
	    }
	    print_n(i, with_numbers);
	    printf(" %s", n);
	    if (slen <= 15) { /* fits in column 2 */
		printf("%*s", (int)(15 - slen), "");
		col = 3;
	    } else { /* rest of the line */
		printf("\n");
		col = 0;
	    }
	    break;
	case 3:
	    if (slen > 14) { /* overflows */
		printf("\n");
		col = 0;
		goto retry;
	    }
	    print_n(i, with_numbers);
	    printf(" %s\n", n);
	    col = 0;
	    break;
	}
    }
    if (col) {
	printf("\n");
    }
}

/**
 * Display the list of existing sessions, and return a selected session name.
 *
 * @param[in] why		Name of operation in progress
 * @param[in] include_public	true if public sessions should be included
 * @param[out] name		Returned selected session name
 * @param[out] lp		Returned selected session location
 *
 * @return -1 for error, 0 for success
 * If no name is chosen, returns 0, but also returns NULL in name.
 */
static int
get_existing_session(const char *why, bool include_public, const char **name,
	src_t *lp)
{
    char nbuf[64];
    int max = include_public? num_xs: num_xs - xs_public.count;

    display_sessions(true, include_public);

    for (;;) {
	int n;

	printf("\nEnter session name or number");
	if (max > 1) {
	    printf(" (1..%d)", max);
	}
	printf(" to %s, or 'q' to quit: ", why);
	fflush(stdout);
	if (get_input(nbuf, sizeof(nbuf)) == NULL) {
	    return -1;
	} else if (nbuf[0] == '\0') {
	    continue;
	} else if (nbuf[0] == 'q' || nbuf[0] == 'Q') {
	    *name = NULL;
	    return 0;
	}
	n = atoi(nbuf);
	if (n == 0) {
	    int i;

	    for (i = 0; i < max; i++) {
		if (!strcasecmp(nbuf, xs_name(i + 1, NULL))) {
		    *name = xs_name(i + 1, lp);
		    return 0;
		}
	    }
	    errout("\nNo such session.");
	    continue;
	} else if (n < 0 || n > max) {
	    errout("\nNo such session.");
	    continue;
	}
	*name = xs_name(n, lp);
	return 0;
    }
}

/**
 * Look up a session specified by the user on the main menu.
 *
 * @param[in] name		Session name to look up
 * @param[in] include_public	true to include public sessions
 * @param[out] lp		Returned location of session
 * @param[out] result		Buffer to put error message in
 * @param[in] result_size	Size of 'result' buffer
 *
 * @return Session name, or NULL if not found
 */
static char *
menu_existing_session(char *name, bool include_public, src_t *lp,
	char *result, size_t result_size)
{
    int i;
    int max = include_public? num_xs: num_xs - xs_public.count;

    for (i = 0; i < max; i++) {
	if (!strcasecmp(name, xs_name(i + 1, lp))) {
	    break;
	}
    }
    if (i >= max) {
	snprintf(result, result_size, "%cNo such session: '%s'", 2, name);
	return NULL;
    } else {
	return name;
    }
}

/**
 * Delete a session.
 *
 * Prompts for a session name, if none is provided in argc/argv.
 *
 * @param[in] argc	Command argument count (from main menu prompt)
 * @param[in] argv	Command argumens (from main menu prompt)
 * @param[out] result	Result returned here
 * @param[in] result_size Size of 'result' buffer
 *
 * @return 0 for success, -1 for failure
 */
static int
delete_session(int argc, char **argv, char *result, size_t result_size)
{
    const char *name = NULL;
    src_t l = SRC_ERR;
    char path[MAX_PATH];

    if (argc > 0) {
	name = menu_existing_session(argv[0], admin(), &l, result,
		result_size);
	if (name == NULL) {
	    return 0;
	}
    }

    if (argc == 0) {
	new_screen(&empty_session, NULL, "\
Delete Session\n");

	if (get_existing_session("delete", admin(), &name, &l) < 0) {
	    return -1;
	} else if (name == NULL) {
	    return 0;
	}
    }

    for (;;) {
	gs_t rc;

	printf("\nAre you sure you want to delete session '%s'? (y/n) [n] ",
		name);
	fflush(stdout);
	rc = getyn(FALSE);
	if (rc == YN_ERR) {
	    return -1;
	} else if (rc == FALSE) {
	    return 0;
	} else if (rc == TRUE) {
	    break;
	}
    }

    snprintf(path, MAX_PATH, "%s%s%s",
	    (l == SRC_DOCUMENTS)? documents_wc3270: public_documents_wc3270,
	    name, SESS_SUFFIX);
    if (unlink(path) < 0) {
	errout("\nDelete of '%s' failed: %s\n", path, strerror(errno));
	goto failed;
    }
    snprintf(path, MAX_PATH, "%s%s.lnk",
	    (l == SRC_DOCUMENTS)? desktop: public_desktop, name);
    if (access(path, R_OK) == 0 && unlink(path) < 0) {
	errout("\nDelete of '%s' failed: %s\n", path, strerror(errno));
	goto failed;
    }

    snprintf(result, result_size, "%cSession '%s' deleted.", 1, name);
    return 0;

failed:
    ask_enter();
    return 0;
}

/**
 * Rename or copy a session.
 *
 * Prompts for from/to session names, if not provided in argc/argv.
 *
 * @param[in] argc	Command argument count (from main menu prompt)
 * @param[in] argv	Command argumens (from main menu prompt)
 * @param[in] is_rename	true if rename, false if copy
 * @param[out] result	Result returned here
 * @param[in] result_size Size of 'result' buffer
 *
 * @return 0 for success, -1 for failure
 */
static int
rename_or_copy_session(int argc, char **argv, bool is_rename, char *result,
	size_t result_size)
{
    char to_name[64];
    const char *from_name = NULL;
    src_t from_l, to_l;
    char from_path[MAX_PATH];
    char to_path[MAX_PATH];
    char from_linkpath[MAX_PATH];
    int i;
    FILE *f;
    session_t s;
    ws_t wsrc;
    char *us;

    if (argc > 0) {
	from_name = menu_existing_session(argv[0],
		!is_rename || admin(),
		&from_l, result,
		result_size);
	if (from_name == NULL) {
	    return 0;
	}
    }

    if (argc == 0) {
	if (is_rename) {
	    new_screen(&empty_session, NULL, "\
Rename Session\n");
	} else {
	    new_screen(&empty_session, NULL, "\
Copy Session\n");
	}
	if (get_existing_session(is_rename? "rename": "copy",
		    !is_rename || admin(),
		    &from_name,
		    &from_l) < 0) {
	    return -1;
	} else if (from_name == NULL) {
	    return 0;
	}
    }

    if (is_rename && !admin() && from_l == SRC_PUBLIC_DOCUMENTS) {
	errout("Cannot rename public session\n");
	goto failed;
    }

    for (;;) {
	if (argc > 1) {
	    strncpy(to_name, argv[1], sizeof(to_name));
	    to_name[sizeof(to_name) - 1] = '\0';
	    argc = 1; /* a bit of a hack */
	} else {
	    if (is_rename) {
		printf("\nEnter new session name for '%s', or 'q' to quit: ",
			from_name);
	    } else {
		printf("\nEnter new session name to copy '%s' into, or 'q' to "
			"quit: ",
			from_name);
	    }
	    fflush(stdout);
	    if (get_input(to_name, sizeof(to_name)) == NULL) {
		return -1;
	    } else if (to_name[0] == '\0') {
		continue;
	    } else if ((to_name[0] == 'q' || to_name[0] == 'Q') &&
		    to_name[1] == '\0') {
		return 0;
	    }
	}
	for (i = 0; i < num_xs; i++) {
	    if (!strcasecmp(to_name, xs_name(i + 1, NULL))) {
		break;
	    }
	}
	if (i < num_xs) {
	    errout("\nSession '%s' already exists. To replace it, you must "
		    "delete it first.", to_name);
	    continue;
	}
	if (!legal_session_name(to_name, NULL, 0)) {
	    continue;
	}
	break;
    }

    switch (from_l) {
    case SRC_PUBLIC_DOCUMENTS:
	snprintf(from_path, MAX_PATH, "%s%s%s", public_documents_wc3270,
		from_name, SESS_SUFFIX);
	break;
    default:
    case SRC_DOCUMENTS:
	snprintf(from_path, MAX_PATH, "%s%s%s", documents_wc3270, from_name,
		SESS_SUFFIX);
	break;
    }

    switch ((to_l = get_src(to_name, from_l))) {
    case SRC_PUBLIC_DOCUMENTS:
	snprintf(to_path, MAX_PATH, "%s%s%s", public_documents_wc3270, to_name,
		SESS_SUFFIX);
	break;
    case SRC_DOCUMENTS:
	snprintf(to_path, MAX_PATH, "%s%s%s", documents_wc3270, to_name,
		SESS_SUFFIX);
	break;
    case SRC_NONE:
	return 0;
    default:
	return -1;
    }

    /* Read in the existing session. */
    f = fopen(from_path, "r");
    if (f == NULL) {
	errout("Cannot open %s for reading: %s\n", from_path,
		strerror(errno));
	goto failed;
    }
    if (!read_session(f, &s, &us)) {
	fclose(f);
	errout("Cannot read '%s'.\n", from_path);
	goto failed;
    }
    fclose(f);

    /* Change its name and write it back out. */
    strncpy(s.session, to_name, STR_SIZE);
    if (write_session_file(&s, us, to_path) < 0) {
	errout("Cannot write '%s'.\n", to_path);
	goto failed;
    }

    /* Remove the orginal. */
    if (is_rename) {
	if (unlink(from_path) < 0) {
	    errout("Cannot remove '%s'.\n", from_path);
	    goto failed;
	}
    }

    /* See about the shortcut as well. */
    snprintf(from_linkpath, sizeof(from_linkpath), "%s%s.lnk",
	    (from_l == SRC_PUBLIC_DOCUMENTS)? public_desktop: desktop,
	    from_name);
    if (access(from_linkpath, R_OK) == 0) {
	for (;;) {
	    gs_t rc;

	    printf("\n%s desktop shortcut as well? (y/n) [y] ",
		    is_rename? "Rename": "Copy");
	    fflush(stdout);
	    rc = getyn(TRUE);
	    if (rc == YN_ERR) {
		return -1;
	    } else if (rc == FALSE) {
		return 0;
	    } else if (rc == TRUE) {
		break;
	    }
	}

	/* Create the new shortcut. */
	wsrc = write_shortcut(&s, false, to_l, to_path, false);
	switch (wsrc) {
	case WS_ERR:
	    return -1;
	case WS_FAILED:
	    goto failed;
	case WS_CREATED:
	case WS_REPLACED:
	case WS_NOP:
	    break;
	}

	/* Remove the original. */
	if (is_rename) {
	    if (unlink(from_linkpath) < 0) {
		errout("Cannot remove '%s'.\n", from_linkpath);
		goto failed;
	    }
	}
    }

    snprintf(result, result_size, "%cSession '%s' %s to '%s'.", 1,
	    from_name, is_rename? "renamed": "copied",
	    to_name);
    return 0;

failed:
    ask_enter();
    return 0;
}

/**
 * Create a shortcut for a session.
 *
 * Prompts for a session name, if none is provided in argc/argv.
 *
 * @param[in] argc      Command argument count (from main menu prompt)
 * @param[in] argv      Command argumens (from main menu prompt)
 * @param[out] result   Result returned here
 * @param[in] result_size Size of 'result' buffer
 *
 * @return 0 for success, -1 for failure
 */
static int
new_shortcut(int argc, char **argv, char *result, size_t result_size)
{
    const char *name = NULL;
    src_t l = SRC_ERR;
    char from_path[MAX_PATH];
    FILE *f;
    ws_t rc;
    session_t s;

    if (argc > 0) {
	name = menu_existing_session(argv[0], true, &l, result, result_size);
	if (name == NULL) {
	    return 0;
	}
    }

    if (argc == 0) {
	new_screen(&empty_session, NULL, "\
Create Shortcut\n");

	if (get_existing_session("create shortcut for", admin(), &name, &l) < 0) {
	    return -1;
	} else if (name == NULL) {
	    return 0;
	}
    }

    switch (l) {
    case SRC_PUBLIC_DOCUMENTS:
	snprintf(from_path, MAX_PATH, "%s%s%s", public_documents_wc3270, name,
		SESS_SUFFIX);
	break;
    default:
    case SRC_DOCUMENTS:
	snprintf(from_path, MAX_PATH, "%s%s%s", documents_wc3270, name,
		SESS_SUFFIX);
	break;
    }

    /*
     * If public document but not admin, create shortcut on per-user desktop.
     */
    if (l == SRC_PUBLIC_DOCUMENTS && !admin()) {
	l = SRC_DOCUMENTS;
    }

    f = fopen(from_path, "r");
    if (f == NULL) {
	errout("Cannot open %s for reading: %s\n", from_path,
		strerror(errno));
	goto failed;
    } else if (!read_session(f, &s, NULL)) {
	fclose(f);
	printf("Cannot read '%s'.\n", from_path);
	goto failed;
    }
    fclose(f);

    rc = write_shortcut(&s, false, l, from_path, false);
    switch (rc) {
    case WS_NOP:
	break;
    case WS_ERR:
	return -1;
    case WS_FAILED:
	goto failed;
    case WS_CREATED:
    case WS_REPLACED:
	snprintf(result, result_size, "%cShortcut %s for '%s'.", 1,
		(rc == WS_CREATED)? "created": "replaced",
		name);
	break;
    }
    return 0;

failed:
    ask_enter();
    return 0;
}

/**
 * Re-create all desktop shortcuts.
 * Migration logic for Windows 11's use of Windows Terminal by default.
 *
 * @return SW_XXX
 */
static int
reshort(void)
{
    int rc;
    int i;
    const char *n;
    bool any = false;

    new_screen(&empty_session, NULL, "\
Re-Create Desktop Shortcuts\n\
\n\
This may be needed when upgrading to a new version of wc3270 that has corrected\n\
a shortcut-related issue, such as Ctrl-C/Ctrl-V not working correctly on\n\
Windows 10 or the use of Windows Terminal in Windows 11.");

    while (true) {
	printf("\nContinue? (y/n) [y] ");
	rc = getyn(TRUE);
	if (rc == YN_ERR) {
	    return SW_ERR;
	} else if (rc == FALSE) {
	    return SW_SUCCESS;
	} else if (rc == TRUE) {
	    break;
	}
    }
    printf("\n");

    for (i = 0; (n = xs_name(i + 1, NULL)) != NULL; i++) {
	char *session;
	char *shortcut;
	FILE *f;
	session_t s;
	int extra_height = 1;
	const wchar_t *font;
	int codepage = 0;
	char exepath[MAX_PATH];
	char args[MAX_PATH];
	HRESULT hres;
	bool public;

	fflush(stdout);
	any = true;
	if (i < xs_my.count) {
	    /* User session. */
	    public = false;
	    session = malloc(strlen(documents_wc3270) + strlen(n) +
		    strlen(SESS_SUFFIX) + 1);
	    sprintf(session, "%s%s" SESS_SUFFIX, documents_wc3270, n);
	    shortcut = malloc(strlen(desktop) + strlen(n) + strlen(".lnk") + 1);
	    sprintf(shortcut, "%s%s.lnk", desktop, n);
	} else {
	    /* Public session. */
	    if (!admin()) {
		continue;
	    }
	    public = true;
	    session = malloc(strlen(public_documents_wc3270) + strlen(n) +
		    strlen(SESS_SUFFIX) + 1);
	    sprintf(session, "%s%s" SESS_SUFFIX, public_documents_wc3270, n);
	    shortcut = malloc(strlen(public_desktop) + strlen(n) +
		    strlen(".lnk") + 1);
	    sprintf(shortcut, "%s%s.lnk", public_desktop, n);
	}

	if (access(shortcut, R_OK) != 0) {
	    /* No shortcut. */
	    yellowout("No shortcut for%s session '%s'.\n",
		    public? " public": "", n);
	    continue;
	}
	if ((f = fopen(session, "r")) == NULL) {
	    errout("Cannot open %s for reading: %s\n", session,
		    strerror(errno));
	    continue;
	}
	if (!read_session(f, &s, NULL)) {
	    errout("Cannot read session file %s\n", session);
	    fclose(f);
	    continue;
	}
	fclose(f);

	/* Re-create the desktop shorcut. */
	sprintf(exepath, "%swc3270.exe", installdir);
	sprintf(args, "+S \"%s\"", session);
	if (!(s.flags & WF_NO_MENUBAR)) {
	    extra_height += 2;
	}

	font = reg_font_from_host_codepage(s.font_name, s.codepage, &codepage, errout);

	hres = create_link(
		exepath,		/* path to executable */
		shortcut,		/* where to put the link */
		"wc3270 session",	/* description */
		args,			/* arguments */
		installdir,		/* working directory */
		(s.ov_rows?		/* console rows */
		    s.ov_rows: wrows[s.model]) + extra_height,
		s.ov_cols?		/* console cols */
		    s.ov_cols: wcols[s.model],
		(wchar_t *)font,	/* font */
		s.point_size,		/* point size */
		s.font_weight,		/* font weight */
		codepage);		/* code page */

	if (SUCCEEDED(hres)) {
	    greenout("Re-created desktop shortcut for%s session '%s'.\n",
		    public? " public": "", n);
	} else {
	    errout("Writing new shortcut for%s session '%s' failed.\n",
		    public? " public": "", n);
	}
    }
    if (any) {
	ask_enter();
    }
    return SW_SUCCESS;
}

/**
 * Initialize a set of session names from a directory.
 *
 * @param[in] dirname	Directory to search
 * @param[out] xsb	Returned list of entries
 * @param[in] location	Which directory this is (current/all)
 */
static void
xs_init_type(const char *dirname, xsb_t *xsb, src_t location)
{
    char dpath[MAX_PATH];
    HANDLE h;
    WIN32_FIND_DATA find_data;
    xs_t *xs;

    /* Check for migration complete. */
    snprintf(dpath, MAX_PATH, "%s%s", dirname, DONE_FILE);
    if (access(dpath, R_OK) == 0) {
	return;
    }

    sprintf(dpath, "%s*%s", dirname, SESS_SUFFIX);
    h = FindFirstFile(dpath, &find_data);
    if (h != INVALID_HANDLE_VALUE) {
	do {
	    char *sname;
	    size_t nlen;
	    xs_t *xss, *prev;

	    sname = find_data.cFileName;
	    nlen = strlen(sname) - strlen(SESS_SUFFIX);

	    if (location == SRC_PUBLIC_DOCUMENTS) {
		int skip = 0;
		xs_t *xsc;

		/*
		 * Skip public documents that are the same as private ones.
		 * This will get us into trouble.
		 */
		for (xsc = xs_my.list; xsc != NULL; xsc = xsc->next) {
		    char *n = xsc->name;

		    if (strlen(n) == nlen && !strncasecmp(n, sname, nlen)) {
			skip = 1;
			break;
		    }
		}
		if (skip) {
		    continue;
		}
	    }

	    xs = (xs_t *)malloc(sizeof(xs_t) + nlen + 1);
	    if (xs == NULL) {
		errout("Out of memory\n");
		ask_enter();
		exit(1);
	    }
	    xs->location = location;
	    xs->name = (char *)(xs + 1);
	    strncpy(xs->name, sname, nlen);
	    xs->name[nlen] = '\0';
	    for (xss = xsb->list, prev = NULL;
		 xss != NULL;
		 prev = xss, xss = xss->next) {
		if (strcasecmp(xs->name, xss->name) < 0) {
		    break;
		}
	    }
	    /* xs goes before xss, which may be NULL. */
	    xs->next = xss;
	    if (prev != NULL) {
		prev->next = xs;
	    } else {
		xsb->list = xs;
	    }
	    xsb->count++;
	} while (FindNextFile(h, &find_data) != 0);

	FindClose(h);
    }
}

/**
 * Free a set of session names.
 *
 * @param[in,out] xsb	Set of names to free
 */
static void
free_xs(xsb_t *xsb)
{
    xs_t *list;
    xs_t *next;

    list = xsb->list;
    while (list != NULL) {
	next = list->next;
	free(list);
	list = next;
    }

    xsb->count = 0;
    xsb->list = NULL;
}

/**
 * Initialize the session names.
 *
 * @param[in] include_public	if true, include public sessions
 */
static void
xs_init(bool include_public)
{
    free_xs(&xs_my);
    free_xs(&xs_public);
    num_xs = 0;

    xs_init_type(searchdir, &xs_my, SRC_DOCUMENTS);
    if (include_public) {
	xs_init_type(public_searchdir, &xs_public, SRC_PUBLIC_DOCUMENTS);
    }
    num_xs = xs_my.count + xs_public.count;
}

/**
 * Look up a session name by index.
 *
 * @param[in] n		Index (first session is 1)
 * @param[out] lp	Location of entry (current/all)
 *
 * @return Session name
 */
static const char *
xs_name(int n, src_t *lp)
{
    xs_t *xs;

    for (xs = xs_my.list; xs != NULL; xs = xs->next) {
	if (!--n) {
	    if (lp != NULL) {
		*lp = xs->location;
	    }
	    return xs->name;
	}
    }
    for (xs = xs_public.list; xs != NULL; xs = xs->next) {
	if (!--n) {
	    if (lp != NULL) {
		*lp = xs->location;
	    }
	    return xs->name;
	}
    }
    return NULL;
}

/**
 * Create or re-create a shortcut.
 *
 * @param[in] s		Session
 * @param[in] ask	If true, ask first
 * @param[in] src	Where the session file is (all or current user)
 * @param[in] sess_path	Pathname of session file
 * @param[in] change_shortcut If true, the shortcut needs updating
 *
 * @return ws_t (no-op, create, replace, error)
 */
static ws_t
write_shortcut(const session_t *s, bool ask, src_t src, const char *sess_path,
	bool change_shortcut)
{
    char linkpath[MAX_PATH];
    char exepath[MAX_PATH];
    char args[MAX_PATH];
    int shortcut_exists;
    int extra_height = 1;
    const wchar_t *font;
    int codepage = 0;
    HRESULT hres;

    /* If writing to the desktop, don't ask about a shortcut. */
    if (src == SRC_NONE ||
	src == SRC_PUBLIC_DESKTOP ||
	src == SRC_DESKTOP ||
	!strncasecmp(sess_path, desktop, strlen(desktop)) ||
	!strncasecmp(sess_path, public_desktop, strlen(public_desktop))) {
	return WS_NOP;
    }

    /* Ask about the shortcut. */
    sprintf(linkpath, "%s%s.lnk",
	    (src == SRC_PUBLIC_DOCUMENTS)? public_desktop: desktop,
	    s->session);
    shortcut_exists = (access(linkpath, R_OK) == 0);
    if (ask) {
	if (shortcut_exists && change_shortcut) {
	    printf("\nOne or more parameters changed that require replacing the desktop shortcut.");
	}
	for (;;) {
	    int rc;

	    printf("\n%s desktop shortcut (y/n) [y]: ",
		    shortcut_exists? "Replace": "Create");
	    rc = getyn(TRUE);
	    if (rc == YN_ERR) {
		return WS_ERR;
	    } else if (rc == FALSE) {
		return WS_NOP;
	    } else if (rc == TRUE) {
		break;
	    }
	}
    }

    /* Create the desktop shorcut. */
    sprintf(exepath, "%swc3270.exe", installdir);
    sprintf(args, "+S \"%s\"", sess_path);
    if (!(s->flags & WF_NO_MENUBAR)) {
	extra_height += 2;
    }

    font = reg_font_from_host_codepage(s->font_name, s->codepage, &codepage, errout);

    hres = create_link(
	    exepath,		/* path to executable */
	    linkpath,		/* where to put the link */
	    "wc3270 session",	/* description */
	    args,		/* arguments */
	    installdir,		/* working directory */
	    (s->ov_rows?	/* console rows */
		s->ov_rows: wrows[s->model]) + extra_height,
	    s->ov_cols?		/* console cols */
		s->ov_cols: wcols[s->model],
	    (wchar_t *)font,	/* font */
	    s->point_size,	/* point size */
	    s->font_weight,	/* font weight */
	    codepage);		/* code page */

    if (SUCCEEDED(hres)) {
	return shortcut_exists? WS_REPLACED: WS_CREATED;
    } else {
	printf("Writing shortcut '%s' failed\n", linkpath);
	return WS_FAILED;
    }
}

/**
 * One pass of the session wizard.
 *
 * @param[in] session_name	Name of session to edit, or NULL
 * @param[in] explicit_edit	If true, '-e' option was used (no need to
 * 				confirm they want to edit it)
 * @param[out] result		Buffer containing previous operation's result,
 * 				and to write current operation's result into
 * @param[in] result_size	Size of 'result' buffer
 *
 * @return Status of operation (success/error/user-quit)
 */
static sw_t
session_wizard(const char *session_name, bool explicit_edit, char *result,
	size_t result_size)
{
    session_t session;
    gs_t rc;
    src_t src;
    char save_session_name[STR_SIZE];
    char path[MAX_PATH];
    int argc;
    char **argv;
    ws_t wsrc;
    size_t sl;
    char *us = NULL;
    bool change_shortcut;
    bool modified = false;

    /* Start with nothing. */
    memset(&session, '\0', sizeof(session));

    /* Find the existing sessions. */
    xs_init(true);

    /* Intro screen. */
    if (session_name == NULL) {
	switch (main_menu(&argc, &argv, result)) {
	case MO_ERR:
	    return SW_ERR;
	case MO_QUIT:
	    return SW_QUIT;
	case MO_EDIT:
	    if (argc > 0) {
		session_name = menu_existing_session(argv[0], admin(), NULL,
			result, result_size);
		if (session_name == NULL) {
		    return SW_SUCCESS;
		}
	    } else {
		new_screen(&session, NULL, "\
Edit Session\n");
		if (get_existing_session("edit", admin(), &session_name,
			    NULL) < 0) {
		    return SW_ERR;
		} else if (session_name == NULL) {
		    return SW_SUCCESS;
		}
	    }
	    explicit_edit = true;
	    break;
	case MO_DELETE:
	    if (delete_session(argc, argv, result, result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_COPY:
	    if (rename_or_copy_session(argc, argv, false, result,
			result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_RENAME:
	    if (rename_or_copy_session(argc, argv, true, result,
			result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_SHORTCUT:
	    if (new_shortcut(argc, argv, result, result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_CREATE:
	    if (argc > 0) {
		if (!legal_session_name(argv[0], result, result_size)) {
		    return SW_SUCCESS;
		}
		session_name = argv[0];
	    }
	    /* fall through below */
	    break;
	case MO_MIGRATE: {
	    char *cmd = malloc(strlen(program) + strlen(" -U") + 1);

	    if (cmd == NULL) {
		errout("Out of memory.\n");
		return SW_ERR;
	    }
	    sprintf(cmd, "%s -U", program);
	    system(cmd);
	    free(cmd);
	    return SW_SUCCESS;
	}
	case MO_RESHORT:
	     return reshort();
	}
    } else {
	new_screen(&session, NULL, "");
    }

    /* Get the session name. */
    rc = get_session(session_name, &session, &us, path, explicit_edit, &src,
	    &modified);
    switch (rc) {
    case GS_NOEDIT_LEAVE:	/* Uneditable, and they don't want to overwrite
				   it. */
	if (us != NULL) {
	    free(us);
	}
	return SW_SUCCESS;
    default:
    case GS_ERR:		/* EOF */
	return SW_ERR;
    case GS_OVERWRITE:		/* Overwrite old (uneditable). */
	/* Clean out the session. */
	strcpy(save_session_name, session.session);
	memset(&session, '\0', sizeof(session));
	strcpy(session.session, save_session_name);
	if (us != NULL) {
	    free(us);
	    us = NULL;
	}
	/* fall through... */
    case GS_NEW:		/* New. */

	/* Get the host name, which defaults to the session name. */
	if (strchr(session.session, ' ') == NULL) {
	    strcpy(session.host, session.session);
	}
	if (get_host(&session) < 0) {
	    return SW_ERR;
	}

	/* Default eveything else. */
	session.port = 23;
	session.model = 4;
	strcpy(session.codepage, "bracket");
	strcpy(session.printerlu, ".");
	session.flags2 |= WF2_NEW_VHC_DEFAULT;
	/* fall through... */
    case GS_EDIT:		/* Edit existing file. */
	/* See what they want to change. */
	src = edit_menu(&session, &us,
		(rc == GS_OVERWRITE)? SP_REPLACE:
		 ((rc == GS_NEW)? SP_CREATE: SP_UPDATE),
		path, session.session, &change_shortcut, modified);
	if (src == SRC_ERR) {
	    return SW_ERR;
	} else if (src == SRC_NONE) {
	    if (rc == GS_NEW) {
		return SW_SUCCESS;
	    } else {
		break;
	    }
	} else if (src == SRC_PUBLIC_DOCUMENTS) {
	    /* All users. */
	    create_wc3270_folder(src);
	    snprintf(path, MAX_PATH, "%s%s%s", public_documents_wc3270,
		    session.session, SESS_SUFFIX);
	} else if (src == SRC_DOCUMENTS) {
	    /* Current user. */
	    create_wc3270_folder(src);
	    snprintf(path, MAX_PATH, "%s%s%s", documents_wc3270, session.session,
		    SESS_SUFFIX);
	} else if (src == SRC_PUBLIC_DESKTOP) {
	    snprintf(path, MAX_PATH, "%s%s%s", public_desktop, session.session,
		    SESS_SUFFIX);
	} else if (src == SRC_DESKTOP) {
	    snprintf(path, MAX_PATH, "%s%s%s", desktop, session.session,
		    SESS_SUFFIX);
	} /* else keep path as-is */

	/* Create the session file. */
	if (write_session_file(&session, us, path) < 0) {
	    if (us != NULL) {
		free(us);
		us = NULL;
	    }
	    goto failed;
	}
	snprintf(result, result_size, "%c%s session '%s'.", 1,
		(rc == GS_NEW)? "Created": "Updated", session.session);
	if (us != NULL) {
	    free(us);
	    us = NULL;
	}
	break;
    case GS_NOEDIT: /* Don't edit existing file, but we do have a copy of
		       the session. */
	break;
    }

    /* Ask about creating or updating the shortcut. */
    wsrc = write_shortcut(&session, true, src, path, change_shortcut);
    switch (wsrc) {
    case WS_NOP:
	break;
    case WS_ERR:
	return SW_ERR;
    case WS_FAILED:
	goto failed;
    case WS_CREATED:
    case WS_REPLACED:
	sl = strlen(result);

	snprintf(result + sl, result_size - sl,
		"%c%s shortcut '%s'.",
		sl? '\n': 1,
		(wsrc == WS_CREATED)? "Created": "Replaced",
		session.session);
	break;
    }

    return SW_SUCCESS;

failed:
    ask_enter();
    return SW_SUCCESS;
}

/**
 * Embed the selected keymaps in the session file.
 *
 * @param[in] session	Session
 * @param[in,out] f	File to append them to
 */
static void
embed_keymaps(const session_t *session, FILE *f)
{
    char keymaps[STR_SIZE];
    char *keymap;
    char *ptr = keymaps;
    km_t *km;
    char *pfx = "! Embedded user-defined keymaps\n";

    strcpy(keymaps, session->keymaps);
    while ((keymap = strtok(ptr, ",")) != NULL) {
	ptr = NULL;
	for (km = km_first; km != NULL; km = km->next) {
	    if (!strcasecmp(keymap, km->name)) {
		if (km->def_both) {
		    fprintf(f, "%swc3270.%s.%s:\\n\\\n%s\n",
			    pfx, ResKeymap, keymap, km->def_both);
		    pfx = "";
		}
		if (km->def_3270) {
		    fprintf(f, "%swc3270.%s.%s.3270:\\n\\\n%s\n",
			    pfx, ResKeymap, keymap,
			    km->def_3270);
		    pfx = "";
		}
		if (km->def_nvt) {
		    fprintf(f, "%swc3270.%s.%s.nvt:\\n\\\n%s\n",
			pfx, ResKeymap, keymap,
			km->def_nvt);
		    pfx = "";
		}
		break;
	    }
	}
    }
}

/**
 * Write miscellaneous user settings into an open file.
 *
 * @param[in] us	User settings, or NULL
 * @param[in] f		File to write into
 */
static void
write_user_settings(char *us, FILE *f)
{
    fprintf(f, "!\n\
! Note that in this file, backslash ('\\') characters are used to specify\n\
! escape sequences, such as '\\r' for a Carriage Return character or '\\t'\n\
! for a Tab character.  To include literal backslashes in this file, such as\n\
! in Windows pathnames or UNC paths, they must be doubled, for example:\n\
!\n\
!   Desired text            Must be specified this way\n\
!    C:\\xdir\\file            C:\\\\xdir\\\\file\n\
!    \\\\server\\printer        \\\\\\\\server\\\\printer\n\
!\n\
!*Additional resource definitions can go after this line.\n");

    /* Write out the user's previous extra settings. */
    if (us != NULL) {
	fprintf(f, "%s", us);
    }
}

/**
 * Write a session file.
 *
 * @param[in] session	Session to write
 * @param[in] us	User settings
 * @param[in] path	Pathname to write session into
 *
 * @return 0 for success, -1 for failure.
 */
static int
write_session_file(const session_t *session, char *us, const char *path)
{
    FILE *f;
    time_t t;
    int bracket;
    long eot;
    unsigned long csum;
    int i;
    char buf[1024];

    /* Make sure the wc3270 subdirectory exists. */
    if (!strncasecmp(path, documents_wc3270, strlen(documents_wc3270))) {
	create_wc3270_folder(SRC_DOCUMENTS);
    } else if (!strncasecmp(path, public_documents_wc3270,
		strlen(public_documents_wc3270))) {
	create_wc3270_folder(SRC_PUBLIC_DOCUMENTS);
    }

    f = fopen(path, "w+");
    if (f == NULL) {
	errout("Cannot create session file %s: %s", path, strerror(errno));
	return -1;
    }

    fprintf(f, "! wc3270 session '%s'\n", session->session);

    t = time(NULL);
    fprintf(f, "! Created or modified by the wc3270 %s Session Wizard %s",
	    wversion, ctime(&t));

    if (strcmp(session->host, CHOICE_NONE)) {
	bracket = (strchr(session->host, ':') != NULL);
	fprintf(f, "wc3270.%s: ", ResHostname);
	if (session->tls) {
	    fprintf(f, "L:");
	}
	if (session->luname[0]) {
	    fprintf(f, "%s@", session->luname);
	}
	fprintf(f, "%s%s%s",
		bracket? "[": "",
		session->host,
		bracket? "]": "");
	if (session->port != 23) {
	    fprintf(f, ":%d", (int)session->port);
	}
	fprintf(f, "\n");
    } else if (session->port != 23) {
	fprintf(f, "wc3270.%s: %d\n", ResPort, (int)session->port);
    }

    if (session->proxy_type[0]) {
	fprintf(f, "wc3270.%s: %s:%s%s%s%s%s%s%s%s%s\n",
		ResProxy,
		session->proxy_type,
		session->proxy_user[0]? session->proxy_user: "",
		session->proxy_password[0]? ":": "",
		session->proxy_password[0]? session->proxy_password: "",
		session->proxy_user[0]? "@": "",
		strchr(session->proxy_host, ':')? "[": "",
		session->proxy_host,
		strchr(session->proxy_host, ':')? "]": "",
		session->proxy_port[0]? ":": "",
		session->proxy_port);
    }

    fprintf(f, "wc3270.%s: %d\n", ResModel, (int)session->model);
    if (session->ov_rows || session->ov_cols) {
	fprintf(f, "wc3270.%s: %ux%u\n", ResOversize,
		session->ov_cols, session->ov_rows);
    }
    fprintf(f, "wc3270.%s: %s\n", ResCodePage, session->codepage);
    if (session->flags & WF_CROSSHAIR) {
	fprintf(f, "wc3270.%s: %s\n", ResCrosshair, ResTrue);
    }
    if (session->flags & WF_ALTCURSOR) {
	fprintf(f, "wc3270.%s: %s\n", ResAltCursor, ResTrue);
    }
    if (session->flags2 & WF2_CURSOR_BLINK) {
	fprintf(f, "wc3270.%s: %s\n", ResCursorBlink, ResTrue);
    }
    if (session->is_dbcs) {
	fprintf(f, "wc3270.%s: %s\n", ResAsciiBoxDraw, ResTrue);
    }

    if (session->wpr3287) {
	fprintf(f, "wc3270.%s: %s\n", ResPrinterLu, session->printerlu);
	if (session->printer[0]) {
	    fprintf(f, "wc3270.%s: %s\n", ResPrinterName,
		    session->printer);
	}
	if (session->printercp[0]) {
	    fprintf(f, "wc3270.%s: %s\n", ResPrinterCodepage,
		    session->printercp);
	}
    }

    if (session->keymaps[0]) {
	fprintf(f, "wc3270.%s: %s\n", ResKeymap, session->keymaps);
	if (session->flags & WF_EMBED_KEYMAPS) {
	    embed_keymaps(session, f);
	}
    }

    if (session->flags & WF_AUTO_SHORTCUT) {
	fprintf(f, "wc3270.%s: %s\n", ResAutoShortcut, ResTrue);
    }

    if (session->flags & WF_WHITE_BG) {
	fprintf(f, "\
! These resources set the background to white\n\
wc3270." ResConsoleColorForHostColor "NeutralBlack: 15\n\
wc3270." ResConsoleColorForHostColor "NeutralWhite: 0\n");
    }

    fprintf(f, "wc3270.%s: %s\n", ResVerifyHostCert,
	    (session->flags2 & WF2_NO_VERIFY_HOST_CERT)? ResFalse: ResTrue);

    if (session->accept_hostname[0]) {
	fprintf(f, "wc3270.%s: %s\n", ResAcceptHostname,
		session->accept_hostname);
    }

    if (session->flags & WF_NO_MENUBAR) {
	fprintf(f, "wc3270.%s: %s\n", ResMenuBar, ResFalse);
    }

    if (session->flags & WF_TRACE) {
	fprintf(f, "wc3270.%s: %s\n", ResTrace, ResTrue);
    }

    if (session->flags2 & WF2_ALWAYS_INSERT) {
	fprintf(f, "wc3270.%s: %s\n", ResAlwaysInsert, ResTrue);
    }

    /* Emit the warning. */
    fprintf(f, "\
!\n\
! The following block of text is used to read the contents of this file back\n\
! into the Session Wizard.  If any of the text from the top of the file\n\
! through the line below reading \"Additional resource definitions...\" is\n\
! modified, the Session Wizard will not be able to edit this file.\n\
!");

    /* Write out the session structure in hex. */
    for (i = 0; i < sizeof(*session); i++) {
	if (!(i % 32)) {
	    fprintf(f, "\n!x");
	}
	fprintf(f, "%02x", ((unsigned char *)session)[i]);
    }
    fprintf(f, "\n");

    /* Save where we are in the file. */
    fflush(f);
    eot = ftell(f);

    /* Go back and read what we wrote. */
    rewind(f);
    csum = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
	for (i = 0; buf[i]; i++) {
	    csum += buf[i] & 0xff;
	}
	if (ftell(f) >= eot) {
	    break;
	}
    }
    fflush(f);

    /* Write out the checksum and structure version. */
    fseek(f, 0, SEEK_END);
    fprintf(f, "!c%08lx %d\n", csum, WIZARD_VER);

    write_user_settings(us, f);

    fclose(f);

    printf("Wrote session file %s.\n", path);

    return 0;
}

/**
 * Make sure the console window is long enough.
 *
 * @param[in] rows	Number of rows desired
 *
 * @return 0 for success, -1 for failure
 */
static int
resize_window(int rows)
{
    int rv = 0;
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO info;

    do {
	/* Get a handle to the console. */
	h = CreateFile("CONOUT$",
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);
	if (h == NULL) {
	    rv = -1;
	    break;
	}

	/* Get its current geometry. */
	if (GetConsoleScreenBufferInfo(h, &info) == 0) {
	    rv = -1;
	    break;
	}

	/* If the buffer isn't big enough, make it bigger. */
	if (info.dwSize.Y < rows) {
	    COORD new_size;

	    new_size.X = info.dwSize.X;
	    new_size.Y = rows;

	    if (SetConsoleScreenBufferSize(h, new_size) == 0) {
		rv = -1;
		break;
	    }
	}

	/* If the window isn't big enough, make it bigger. */
	if (info.srWindow.Bottom - info.srWindow.Top < rows) {
	    SMALL_RECT sr;

	    sr.Top = 0;
	    sr.Bottom = rows;
	    sr.Left = 0;
	    sr.Right = info.srWindow.Right - info.srWindow.Left;

	    if (SetConsoleWindowInfo(h, TRUE, &sr) == 0) {
		rv = -1;
		break;
	    }
	}

    } while(0);

    if (h != NULL) {
	CloseHandle(h);
    }
    return rv;
}

/* Compute the values of the directories where user files live. */
static void
get_base_dirs(bool new_way)
{
    if (!new_way) {
	/* Old way: Use AppData instead of the Documents diretories. */
	searchdir = appdata_wc3270;
	public_searchdir = common_appdata_wc3270;
	return;
    }
}

/**
 * Usage message. Display syntax and exit.
 */
static void
w_usage(void)
{
    fprintf(stderr, "\
Usage: wc3270wiz [session-name]\n\
       wc3270wiz [-e] [session-file]\n\
       wc3270wiz -U[a]\n");
    fflush(stderr);
    exit(1);
}

/**
 * Main procedure.
 *
 * @param[in] argc	Command-line argument count
 * @param[in] argv	Command-line arguments
 *
 * @return Exit status
 */
int
main(int argc, char *argv[])
{
    sw_t rc;
    char *session_name = NULL;
    bool explicit_edit = false;
    bool upgrade = false;
    bool automatic_upgrade = false;
    DWORD name_size;
    char result[STR_SIZE];

    /*
     * Parse command-line arguments.
     * For now, there is only one -- the optional name of the session.
     */
    program = argv[0];
    if (argc > 1 && !strcmp(argv[1], "-U")) {
	upgrade = true;
	argc--;
	argv--;
    }
    if (argc > 1 && !strcmp(argv[1], "-Ua")) {
	upgrade = true;
	automatic_upgrade = true;
	argc--;
	argv--;
    }
    if (argc > 1 && !strcmp(argv[1], "-e")) {
	explicit_edit = true;
	argc--;
	argv++;
    }
    switch (argc) {
    case 1:
	break;
    case 2:
	session_name = argv[1];
	break;
    default:
	w_usage();
	break;
    }

    if (upgrade && explicit_edit) {
	w_usage();
    }

    /* Figure out the version. */
    if (get_version_info() < 0) {
	return 1;
    }

    /* Get some paths from Windows. */
    if (!get_dirs("wc3270", &installdir, &desktop, &appdata_wc3270,
		&public_desktop, &common_appdata_wc3270, &documents,
		&public_documents, &documents_wc3270, &public_documents_wc3270,
		&windirs_flags)) {
	return 1;
    }
    searchdir = documents_wc3270;
    public_searchdir = public_documents_wc3270;
    name_size = sizeof(username) / sizeof(TCHAR);
    if (GetUserName(username, &name_size) == 0) {
	errout("GetUserName failed, error %ld\n", (long)GetLastError());
	return 1;
    }

    /* Get the console input handle. */
    conin_handle = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (conin_handle == NULL) {
	errout("CreateFile(CONIN$) failed, error %ld\n", (long)GetLastError());
	return 1;
    }

    /* Resize the console window. */
    resize_window(44);

    signal(SIGINT, SIG_IGN);

    if (upgrade) {
	/* Do an upgrade. */
	get_base_dirs(false);
	save_keymaps(admin());
	xs_init(admin());
	rc = do_upgrade(automatic_upgrade);
    } else {
	get_base_dirs(true);
	save_keymaps(true);
	/* Display the main menu until they quit or something goes wrong. */
	result[0] = '\0';
	do {
	    rc = session_wizard(session_name, explicit_edit, result,
		    sizeof(result));
	    if (session_name != NULL) {
		    break;
	    }
	} while (rc == SW_SUCCESS);
    }

    /*
     * Wait for Enter before exiting, so the console window does not
     * disappear without the user seeing what it did.
     */
    if (rc != SW_QUIT) {
	printf("\n%sWizard %s. ",
		upgrade? "Migration ": "",
		(rc == SW_ERR)? "aborted": "complete");
	if (!automatic_upgrade) {
	    ask_enter();
	}
    }

    return 0;
}

/**
 * Check whether the current user is currently elevated (Vista or newer) or
 * in the Administrators group (XP).
 *
 * @return TRUE if administrator
 */
static BOOL
admin(void)
{
    BOOL b;
    SID_IDENTIFIER_AUTHORITY nt_authority = { SECURITY_NT_AUTHORITY };
    PSID administrators_group;

    if (getenv("NOTADMIN")) {
	return FALSE;
    }

    b = AllocateAndInitializeSid(&nt_authority, 2,
	    SECURITY_BUILTIN_DOMAIN_RID,
	    DOMAIN_ALIAS_RID_ADMINS,
	    0, 0, 0, 0, 0, 0,
	    &administrators_group);
    if (b) {
	if (!CheckTokenMembership( NULL, administrators_group, &b)) {
	    b = FALSE;
	}
	FreeSid(administrators_group);
    }
    return(b);
}

/**
 * Are there any wc3270 files in a directory?
 * 
 * @param[in] dirname	directory name
 *
 * @return true if there are any wc3270 files present
 */
static bool
any_in(char *dirname)
{
    char path[MAX_PATH];
    HANDLE h;
    WIN32_FIND_DATA find_data;
    bool any = false;

    snprintf(path, sizeof(path), "%s%s", dirname, DONE_FILE);
    if (access(path, R_OK) == 0) {
	return false;
    }

    snprintf(path, sizeof(path), "%s*" SESS_SUFFIX, dirname);
    if ((h = FindFirstFile(path, &find_data)) != INVALID_HANDLE_VALUE) {
	any = true;
	FindClose(h);
    }
    if (any) {
	return true;
    }

    snprintf(path, sizeof(path), "%s*" KEYMAP_SUFFIX, dirname);
    if ((h = FindFirstFile(path, &find_data)) != INVALID_HANDLE_VALUE) {
	any = true;
	FindClose(h);
    }
    return any;
}

/**
 * Check whether there are files to be migrated.
 */
static bool
ad_exist(void)
{
    return (any_in(appdata_wc3270) ||
	    (admin() && any_in(common_appdata_wc3270)));
}

/*********** Migration Wizard. ***********/

/* Write a wchar_t string to a file. */
static void
wwrite(FILE *f, wchar_t *s)
{
    fwrite(s, sizeof(wchar_t), wcslen(s), f);
}

/* Create a wc3270 folder. */
static void
create_wc3270_folder(src_t src)
{
    char *parent = (src == SRC_DOCUMENTS)? documents: public_documents;
    char wc3270_dir[MAX_PATH];
    char desktop_ini[MAX_PATH];
    char wc3270_exe[MAX_PATH];
    wchar_t lwc3270_exe[MAX_PATH];

    /* Create My Documents\wc3270. */
    snprintf(wc3270_dir, MAX_PATH, "%swc3270", parent);
    if (access(wc3270_dir, R_OK) != 0) {

	/* Create the folder. */
	if (_mkdir(wc3270_dir) < 0) {
	    errout("Cannot create %s: %s\n", wc3270_dir,
		    strerror(errno));
	    ask_enter();
	    exit(1);
	}
	printf("Created folder %s.\n", wc3270_dir);
    }

    /*
     * Mark it read-only, so file explorer looks for Desktop.ini.
     * Also clear the SYSTEM flag, in case it was set by an earlier version.
     */
    SetFileAttributes(wc3270_dir,
	    FILE_ATTRIBUTE_READONLY |
	    (GetFileAttributes(wc3270_dir) & ~FILE_ATTRIBUTE_SYSTEM));

    snprintf(desktop_ini, MAX_PATH, "%swc3270\\Desktop.ini", parent);
    if (access(desktop_ini, R_OK) != 0) {

	/* Create Desktop.ini. */
	FILE *f = fopen(desktop_ini, "wb");

	if (f == NULL) {
	    errout("Cannot create %s: %s\n", desktop_ini, strerror(errno));
	    return;
	}
	fwrite("\xff\xfe", 1, 2, f); /* BOM */
	wwrite(f, L"[.ShellClassInfo]\r\n");
	wwrite(f, L"ConfirmFileOp=0\r\n");
	wwrite(f, L"IconFile=");
	snprintf(wc3270_exe, MAX_PATH, "%swc3270.exe", installdir);
	mbstowcs(lwc3270_exe, wc3270_exe, strlen(wc3270_exe) + 1);
	wwrite(f, lwc3270_exe);
	wwrite(f, L"\r\n");
	wwrite(f, L"IconIndex=0\r\n");
	fclose(f);

	/* Make it a hidden system file. */
	if (!SetFileAttributes(desktop_ini,
		    FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN)) {
	    errout("SetFileAttributes(%s) failed", desktop_ini);
	    return;
	}
    }
}

/* Copy one session file (Migration Wizard). */
static sw_t
migrate_session(xs_t *xs, int automatic, bool fully_automatic)
{
    FILE *f, *g;
    int c;
    char link_path[MAX_PATH];
    char from_path[MAX_PATH];
    char to_path[MAX_PATH];
    session_t s;
    char exepath[MAX_PATH];
    char args[MAX_PATH];
    HRESULT hres;
    int rc;
    src_t to_src = xs->location;
    bool shortcut_exists;

    if (!automatic) {
	printf("\nFound ");
	if (xs->location == SRC_DOCUMENTS) {
	    printf("user '%s'", username);
	} else {
	    printf("shared");
	}
	printf(" session '%s'.", xs->name);

	if (admin()) {
	    do {
		char answer[16];
		size_t sl;

		printf("\n\
Copy session to My Documents, Public Documents or neither?\n\
 (my/public/neither) [%s] ",
			xs->location == SRC_DOCUMENTS? "my": "public");
		if (!get_input(answer, sizeof(answer))) {
		    return SW_ERR;
		}
		sl = strlen(answer);
		if (!sl) {
		    break;
		}
		if (!strncasecmp(answer, "quit", sl)) {
		    return SW_QUIT;
		}
		if (!strncasecmp(answer, "neither", sl)) {
		    return SW_SUCCESS;
		}
		if (!strncasecmp(answer, "my", sl)) {
		    to_src = SRC_DOCUMENTS;
		    break;
		}
		if (!strncasecmp(answer, "public", sl)) {
		    to_src = SRC_PUBLIC_DOCUMENTS;
		    break;
		}
		errout("Please answer 'my', 'public' or 'neither'.\n");
	    } while (true);
	} else {
	    do {
		int rc;

		printf("\nCopy session to My Documents? (y/n) [y]: ");
		rc = getyn(true);
		if (rc == YN_ERR) {
		    return SW_ERR;
		}
		if (rc == FALSE) {
		    return SW_SUCCESS;
		}
		if (rc == TRUE) {
		    to_src = SRC_DOCUMENTS;
		    break;
		}
	    } while (true);
	}
    }

    snprintf(from_path, MAX_PATH, "%s%s" SESS_SUFFIX,
	    (xs->location == SRC_DOCUMENTS)?
		appdata_wc3270: common_appdata_wc3270,
	    xs->name);
    snprintf(to_path, MAX_PATH, "%s%s" SESS_SUFFIX,
	    (to_src == SRC_DOCUMENTS)? documents_wc3270:
				       public_documents_wc3270,
	    xs->name);

    /* Check for overwrite. */
    if (!fully_automatic) {
	if (access(to_path, R_OK) == 0) {
	    do {
		printf("\nReplace %s? (y/n) [y]: ", to_path);
		rc = getyn(TRUE);
		if (rc == YN_ERR) {
		    return SW_ERR;
		} else if (rc == FALSE) {
		    return SW_SUCCESS;
		}
	    } while (rc == YN_RETRY);
	}
    }

    f = fopen(from_path, "r");
    if (f == NULL) {
	errout("Cannot open %s for reading: %s\n", from_path,
		strerror(errno));
	return SW_ERR;
    }
    create_wc3270_folder(to_src);
    g = fopen(to_path, "w");
    if (g == NULL) {
	errout("Cannot open %s for writing: %s\n", to_path,
		strerror(errno));
	fclose(f);
	return SW_ERR;
    }
    while ((c = fgetc(f)) != EOF) {
	fputc(c, g);
    }
    fclose(f);
    fclose(g);
    printf("Copied session '%s' to %s.\n", xs->name, to_path);

    snprintf(link_path, MAX_PATH, "%s%s.lnk",
	    (xs->location == SRC_DOCUMENTS)? desktop: public_desktop,
	    xs->name);
    shortcut_exists = (access(link_path, R_OK) == 0);

    if (automatic) {
	/* Automatic -- only replace the shortcut it if exists. */
	if (!shortcut_exists) {
	    return SW_SUCCESS;
	}
    } else {
	/* Manual -- ask. */
	do {
	    printf("\n%s desktop shortcut? (y/n) [y]: ",
		    shortcut_exists? "Replace": "Create");
	    rc = getyn(TRUE);
	    if (rc == YN_ERR) {
		return SW_ERR;
	    } else if (rc == FALSE) {
		return SW_SUCCESS;
	    }
	} while (rc == YN_RETRY);
    }

    /* Read in the session. */
    f = fopen(to_path, "r");
    if (!read_session(f, &s, NULL)) {
	errout("Invalid session file '%s'.\n", to_path);
	fclose(f);
	return SW_ERR;
    }
    fclose(f);

    /* Create the shortcut. */
    snprintf(exepath, MAX_PATH, "%s%s", installdir, "wc3270.exe");
    snprintf(args, MAX_PATH, "+S \"%s\"", to_path);
    hres = create_shortcut(&s, exepath, link_path, args, installdir);
    if (!SUCCEEDED(hres)) {
	errout("Cannot create shortcut '%s'.\n", link_path);
	return SW_ERR;
    }
    printf("%s shortcut %s\n", shortcut_exists? "Replaced": "Created",
	    link_path);

    /* Done. */
    return SW_SUCCESS;
}

/* Copy one keymap (Migration Wizard). */
static sw_t
migrate_one_keymap(const char *from_dir, const char *to_dir, const char *name,
	const char *suffix, bool fully_automatic)
{
    char from_path[MAX_PATH];
    char to_path[MAX_PATH];
    FILE *f, *g;
    int c;

    /* Construct the paths. */
    snprintf(from_path, MAX_PATH, "%s%s%s%s",
	    from_dir, name, KEYMAP_SUFFIX, suffix);
    snprintf(to_path, MAX_PATH, "%s%s%s%s",
	    to_dir, name, KEYMAP_SUFFIX, suffix);

    if (!fully_automatic) {
	/* Check for overwrite. */
	if (access(to_path, R_OK) == 0) {
	    int rc;

	    do {
		printf("\nReplace %s? (y/n) [y]: ", to_path);
		rc = getyn(TRUE);
		if (rc == TRUE) {
		    break;
		}
		if (rc == FALSE) {
		    return SW_SUCCESS;
		}
		if (rc == YN_ERR)
		{
		    return SW_ERR;
		}
	    } while (rc == YN_RETRY);
	}
    }

    /* Create the documents folder. */
    if (!strcasecmp(to_dir, documents_wc3270)) {
	create_wc3270_folder(SRC_DOCUMENTS);
    } else {
	create_wc3270_folder(SRC_PUBLIC_DOCUMENTS);
    }

    /* Copy. */
    f = fopen(from_path, "r");
    if (f == NULL) {
	errout("Cannot open %s for reading: %s\n", from_path,
		strerror(errno));
	return SW_ERR;
    }
    g = fopen(to_path, "w");
    if (g == NULL) {
	errout("Cannot open %s for reading: %s\n", to_path,
		strerror(errno));
	fclose(f);
	return SW_ERR;
    }
    while ((c = fgetc(f)) != EOF) {
	fputc(c, g);
    }

    /* Done. */
    fclose(f);
    fclose(g);
    printf("Copied keymap '%s' to %s.\n", name, to_path);
    return SW_SUCCESS;
}

/* Copy the keymaps (Migration Wizard). */
static sw_t
migrate_keymaps(bool fully_automatic)
{
    km_t *km;
    sw_t sw;

    for (km = km_first; km != NULL; km = km->next)
    {
	char *from_dir;
	char to_dir[MAX_PATH];

	switch (km->src) {
	case SRC_DOCUMENTS:
	    from_dir = appdata_wc3270;
	    snprintf(to_dir, MAX_PATH, "%swc3270\\", documents);
	    break;
	case SRC_PUBLIC_DOCUMENTS:
	    from_dir = common_appdata_wc3270;
	    snprintf(to_dir, MAX_PATH, "%swc3270\\", public_documents);
	    break;
	default:
	    continue;
	}

	if (km->def_both != NULL) {
	    sw = migrate_one_keymap(from_dir, to_dir, km->name, "",
		    fully_automatic);
	    if (sw != SW_SUCCESS) {
		return sw;
	    }
	}
	if (km->def_3270 != NULL) {
	    sw = migrate_one_keymap(from_dir, to_dir, km->name, KM_3270,
		    fully_automatic);
	    if (sw != SW_SUCCESS) {
		return sw;
	    }
	}
	if (km->def_nvt != NULL) {
	    sw = migrate_one_keymap(from_dir, to_dir, km->name, KM_NVT,
		    fully_automatic);
	    if (sw != SW_SUCCESS) {
		return sw;
	    }
	}
    }

    return SW_SUCCESS;
}

/* Do an upgrade. */
static sw_t
do_upgrade(bool automatic_from_cmdline)
{
    char done_path[MAX_PATH];
    static char wizard[] = "wc3270 Migration Wizard";
    int nkm = 0;
    int nf = 0;
    int rc;
    int automatic;
    xs_t *xs;
    FILE *f;

    /* If there are no sessions and no keymaps, we're done. */
    if (km_first) {
	km_t *km;

	for (km = km_first; km != NULL; km = km->next)
	{
	    if (km->src != SRC_NONE) {
		nkm++;
	    }
	}
    }
    if (!xs_my.count && !xs_public.count && !nkm) {
	printf("No session files or keymaps to migrate.\n");
	return SW_QUIT;
    }

    if (!automatic_from_cmdline) {
	/* Say hello. */
	cls();
	reverseout("%s%*s%s\n",
		wizard,
		(int)(79 - strlen(wizard) - strlen(wversion)), " ",
		wversion);

	/* Ask if they want to upgrade. */
	printf("\n\
wc3270 %s no longer keeps user-defined files in AppData. Session and\n\
keymap files are kept in Documents folders instead.\n\n\
The following files were found in %s:\n",
		wversion,
		admin()? "wc3270 AppData folders":
		         "your wc3270 AppData folder");
	if (xs_my.count || xs_public.count) {
	    int nxs = xs_my.count + xs_public.count;

	    printf(" %d session file%s\n", nxs, (nxs != 1)? "s": "");
	    nf = nxs;
	}
	if (nkm) {
	    printf(" %d keymap file%s\n", nkm, (nkm != 1)? "s": "");
	    nf += nkm;
	}

	while (true) {
	    printf("\nCopy %s to %s? (y/n) [y]: ",
		    (nf == 1)? "this file": "these files",
		    admin()? "Documents folders": "My Documents");
	    rc = getyn(TRUE);
	    if (rc == YN_ERR) {
		return SW_ERR;
	    }
	    if (rc == FALSE) {
		return SW_SUCCESS;
	    }
	    if (rc == TRUE) {
		break;
	    }
	}
	printf("\n\
The files can be copied automatically, which means that:\n\
- Session files and keymap files in your wc3270 AppDefaults folder will be\n\
  copied to My Documents.\n");
	if (admin()) {
	    printf("\
- Session files and keymap files in the all-users wc3270 AppDefaults folder\n\
  will be copied to Public Documents.\n");
	}
	printf("\
- Existing desktop shortcuts will be re-written to point at the new sessions,\n\
  which means that any customizations will be lost.\n");

	while (true) {
	    printf("\nCopy automatically? (y/n) [y]: ");
	    automatic = getyn(TRUE);
	    if (automatic == YN_ERR) {
		return SW_ERR;
	    }
	    if (automatic == TRUE || automatic ==FALSE) {
		break;
	    }
	}
	printf("\n");
    } else {
	/* Just do it all automatically. */
	automatic = TRUE;
    }

    /* Copy each session file. */
    for (xs = xs_my.list; xs != NULL; xs = xs->next) {
	rc = migrate_session(xs, automatic, automatic_from_cmdline);
	if (rc != SW_SUCCESS) {
	    return rc;
	}
    }
    for (xs = xs_public.list; xs != NULL; xs = xs->next) {
	rc = migrate_session(xs, automatic, automatic_from_cmdline);
	if (rc != SW_SUCCESS) {
	    return rc;
	}
    }

    /* Copy each keymap. */
    rc = migrate_keymaps(automatic_from_cmdline);
    if (rc != SW_SUCCESS) {
	return rc;
    }

    /* Don't do this again. */
    snprintf(done_path, sizeof(done_path), "%s%s", searchdir, DONE_FILE);
    if ((f = fopen(done_path, "w")) != NULL) {
	fclose(f);
    }
    if (admin()) {
	snprintf(done_path, sizeof(done_path), "%s%s", public_searchdir,
		DONE_FILE);
	if ((f = fopen(done_path, "w")) != NULL) {
	    fclose(f);
	}
    }

    /* Done. */
    return SW_SUCCESS;
}
