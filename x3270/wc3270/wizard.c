/*
 * Copyright (c) 2006-2014, Paul Mattes.
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
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "proxy.h"
#include "resources.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#include <wincon.h>
#include <lmcons.h>

#include "winversc.h"
#include "shortcutc.h"
#include "windirsc.h"
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

enum {
    MN_NONE = 0,
    MN_HOST,		/* host name */
    MN_LU,		/* logical unit */
    MN_PORT,		/* TCP port */
    MN_MODEL,		/* model number */
    MN_OVERSIZE,	/* oversize */
    MN_CHARSET,		/* character set */
    MN_SSL,		/* SSL tunnel */
    MN_VERIFY,		/* verify host certificate */
    MN_PROXY,		/* use proxy host */
    MN_PROXY_SERVER,	/* proxy host name */
    MN_PROXY_PORT,	/* proxy port number */
    MN_3287,		/* printer session */
    MN_3287_MODE,	/* printer mode */
    MN_3287_LU,		/* printer logical unit */
    MN_3287_PRINTER,	/* printer Windows printer */
    MN_3287_CODEPAGE,	/* printer code page */
    MN_KEYMAPS,		/* keymaps */
    MN_EMBED_KEYMAPS,	/* embed keymaps */
    MN_FONT_SIZE,	/* font size */
    MN_BG,		/* background color */
    MN_MENUBAR,		/* menu bar */
    MN_TRACE,		/* trace at start-up */
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
    SRC_ALL,		/* success, in all-users AppData */
    SRC_CURRENT,	/* success, in current user AppData */
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

/* Aliases for obsolete character set names. */
struct {
    char	*alias;
    char	*real;
} charset_alias[] = {
    { "japanese-290",  "japanese-kana" },
    { "japanese-1027", "japanese-latin" },
    { NULL, NULL }
};

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
    { NULL,		NULL }
};

static struct {
    char *name;
    char *protocol;
    char *port;
} proxies[] = {
    { PROXY_HTTP,	"HTTP tunnel (RFC 2817, e.g., squid)",	PORT_HTTP },
    { PROXY_PASSTHRU,	"Sun telnet-passthru",			NULL   },
    { PROXY_SOCKS4,	"SOCKS version 4",			PORT_SOCKS4 },
    { PROXY_SOCKS5,	"SOCKS version 5 (RFC 1928)",		PORT_SOCKS5 },
    { PROXY_TELNET,	"None (just send 'connect host port')",	NULL   },
    { NULL,		NULL,					NULL   }
};

static int write_session_file(const session_t *s, char *us, const char *path);

static char *mya = NULL;
static char *installdir = NULL;
static char *desktop = NULL;
static char *common_desktop = NULL;
static char *commona = NULL;
static int installed = FALSE;
static TCHAR username[UNLEN + 1];

static int get_printerlu(session_t *s, int explain);

static int num_xs;
static const char *xs_name(int n, src_t *lp);
static void xs_init(void);
typedef struct xs {	/* Existing session: */
    src_t location;	/*  location (current user or all users) */
    char *name;		/*  session name */
    struct xs *next;	/*  list linkage */
} xs_t;
typedef struct {	/* Set of existing sessions: */
    int count;		/*  count */
    xs_t *list;		/*  list of sessions */
} xsb_t;
static xsb_t xs_current;	/* current-user sessions */
static xsb_t xs_all;		/* all-users sessions */

static session_t empty_session;

static void write_user_settings(char *us, FILE *f);
static void display_sessions(int with_numbers);
static ws_t write_shortcut(const session_t *s, int ask, src_t src,
	const char *path);

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
    char *s;
    size_t sl;

    /* Make sure all of the output gets out. */
    fflush(stdout);

    /* Get the raw input from stdin. */
    if (fgets(buf, bufsize, stdin) == NULL) {
	    return NULL;
    }

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

    if (!strncasecmp(yn, "yes", strlen(yn))) {
	    return TRUE;
    } else if (!strncasecmp(yn, "no", strlen(yn))) {
	    return FALSE;
    }

    printf("\nPlease answer (y)es or (n)o.");
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
	printf("\nEnter Windows printer name: [%s] ",
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
	if (!strcmp(printername, "default")) {
	    printername[0] = '\0';
	}
	if (strchr(printername, '!') || strchr(printername, ',')) {
	    printf("\nInvalid printer name.");
	    continue;
	} else {
	    break;
	}
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
} km_t;
km_t *km_first = NULL;
km_t *km_last = NULL;

/**
 * Save a keymap name. Return its node.
 *
 * @param[in] path		Pathname of keymap file
 * @param[in] keymap_name	Name of keymap
 * @param[in] description	Keymap description, or NULL
 *
 * @return Keymap node, possibly newly-allocated.
 */
static km_t *
save_keymap_name(const char *path, char *keymap_name, const char *description)
{
    km_t *km;
    size_t sl;
    km_t *kms;
    FILE *f;
    enum { KMF_BOTH, KMF_3270, KMF_NVT } km_mode = KMF_BOTH;
    char **def = NULL;

    km = (km_t *)malloc(sizeof(km_t));
    if (km == NULL) {
	fprintf(stderr, "Out of memory\n");
	return NULL;
    }
    memset(km, '\0', sizeof(km_t));
    strcpy(km->name, keymap_name);
    km->description[0] = '\0';
    sl = strlen(km->name);

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
		    fprintf(stderr, "Out of memory\n");
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
 * Initialize the set of available keymaps.
 *
 * Adds the builtin keymaps to a database, then searches the two AppData
 * directories for user-defined keymaps and adds those.
 */
static void
save_keymaps(void)
{
    int i;
    char dpath[MAX_PATH];
    char fpath[MAX_PATH];
    HANDLE h;
    WIN32_FIND_DATA find_data;

    for (i = 0; builtin_keymaps[i].name != NULL; i++) {
	(void) save_keymap_name(NULL, builtin_keymaps[i].name,
		builtin_keymaps[i].description);
    }
    sprintf(dpath, "%s*%s", mya, KEYMAP_SUFFIX);
    h = FindFirstFile(dpath, &find_data);
    if (h != INVALID_HANDLE_VALUE) {
	do {
	    sprintf(fpath, "%s%s", mya, find_data.cFileName);
	    (void) save_keymap_name(fpath, find_data.cFileName, NULL);
	} while (FindNextFile(h, &find_data) != 0);
	FindClose(h);
    }
    if (commona != NULL) {
	sprintf(dpath, "%s*%s", commona, KEYMAP_SUFFIX);
	h = FindFirstFile(dpath, &find_data);
	if (h != INVALID_HANDLE_VALUE) {
	    do {
		sprintf(fpath, "%s%s", commona, find_data.cFileName);
		(void) save_keymap_name(fpath, find_data.cFileName, NULL);
	    } while (FindNextFile(h, &find_data) != 0);
	    FindClose(h);
	}
    }
    sprintf(dpath, "%s*%s", installdir, KEYMAP_SUFFIX);
    h = FindFirstFile(dpath, &find_data);
    if (h != INVALID_HANDLE_VALUE) {
	do {
	    sprintf(fpath, "%s%s", installdir, find_data.cFileName);
	    (void) save_keymap_name(fpath, find_data.cFileName, NULL);
	} while (FindNextFile(h, &find_data) != 0);
	FindClose(h);
    }
}

/**
 * Fix up a UNC printer path in an old session file.
 *
 * The session wizard was originally written without understanding that
 * backslashes needed to be doubled. So it created session files with UNC
 * printer paths using incorrect syntax. This function patches that up.
 *
 * @param[in,out] s	Session
 *
 * @return 1 if the name needed fixing, 0 otherwise.
 */
static int
fixup_printer(session_t *s)
{
    char buf[STR_SIZE];
    int i, j;
    char c;

    if (s->printer[0] == '\\' &&
	s->printer[1] == '\\' &&
	s->printer[2] != '\\') {
	/*
	 * The session file was created by an earlier version of the
	 * session wizard, and contains a UNC printer path that has
	 * not had its backslashes expanded.  Expand them.
	 */
	j = 0;
	for (i = 0; i < (STR_SIZE - 1) && (c = s->printer[i]) != '\0'; i++) {
	    if (c == '\\') {
		if (j < (STR_SIZE - 1)) {
		    buf[j++] = '\\';
		}
		if (j < (STR_SIZE - 1)) {
		    buf[j++] = '\\';
		}
	    } else {
		if (j < (STR_SIZE - 1)) {
			buf[j++] = c;
		}
	    }
	}
	buf[j] = '\0';
	strncpy(s->printer, buf, STR_SIZE);
	return 1;
    } else {
	return 0;
    }
}

/**
 * Reformat a quoted UNC path for display.
 *
 * @param[in] expanded		UNC path in session file (quoted) format
 * @param[out] condensed	UNC path in display format
 *
 * @return 1 if it was reformatted, 0 otherwise.
 */
static int
redisplay_printer(const char *expanded, char *condensed)
{
    int i;
    int j;
    int bsl = 0;
    int reformatted = 0;

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
	    if (c == '\\') {
		reformatted = 1;
		bsl = 0;
	    } else {
		goto abort;
	    }
	} else {
	    condensed[j++] = c;
	    if (c == '\\') {
		    bsl = 1;
	    }
	}
    }

    return reformatted;

abort:
    strcpy(condensed, expanded);
    return 0;
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
    system("cls");
    printf("%s%*s%s\n",
	    wizard,
	    79 - strlen(wizard) - strlen(wversion), " ",
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
    int requires_xs;	/*  if TRUE, requires existing sessions */
    int num_params;	/*  number of command-line parameters to accept */
} main_option[] = {
    { NULL, NULL, FALSE, 0 }, /* intentional hole */
    { "Create new session", "new",      "create", FALSE, 1 },
    { "Edit session",       "edit",     NULL,     TRUE, 1 },
    { "Delete session",     "delete",   "rm",     TRUE, 1 },
    { "Copy session",       "copy",     "cp",     TRUE, 2 },
    { "Rename session",     "rename",   "mv",     TRUE, 2 },
    { "Create shortcut",    "shortcut", NULL,     TRUE, 1 },
    { "Quit",               "quit",     "exit",   FALSE, 0 },
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

    display_sessions(0);

    printf("\n");
    for (i = MO_FIRST; main_option[i].text != NULL; i++) {
	if (!num_xs && main_option[i].requires_xs) {
	    continue;
	}
	printf("  %d. %s (%s)\n",
		i, main_option[i].text, main_option[i].name);
    }

    for (;;) {
	size_t sl;
	int mo;

	if (result && result[0]) {
	    printf("\n%s", result);
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
	    printf("\nWow, am I confused.\n");
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
		printf("\nUnknown command.");
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

	printf("\nUnknown command.");
	continue;

    extra:
	printf("\nExtra parameter(s).");
	continue;
    }
}

/**
 * Search a well-defined series of locations for a session file.
 *
 * @param[in] session_name	Name of session
 * @param[out] path		Returned pathname
 *
 * @return SRC_XXX enumeration:
 * 	   SRC_CURRENT session is in current user's AppData directory
 * 	   SRC_ALL session is in all-users AppData directory
 * 	   SRC_OTHER session is somewhere else
 */
static src_t
find_session_file(const char *session_name, char *path)
{
    /* Try user's AppData. */
    snprintf(path, MAX_PATH, "%s%s%s", mya, session_name, SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_CURRENT;
    }

    /* Not there.  Try common AppData. */
    if (commona != NULL) {
	snprintf(path, MAX_PATH, "%s%s%s", commona, session_name, SESS_SUFFIX);
	if (access(path, R_OK) == 0) {
	    return SRC_ALL;
	}
    }

    /* Not there.  Try installdir. */
    snprintf(path, MAX_PATH, "%s%s%s", installdir, session_name, SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_OTHER;
    }

    /* Not there.  Try cwd. */
    snprintf(path, MAX_PATH, "%s%s", session_name, SESS_SUFFIX);
    if (access(path, R_OK) == 0) {
	return SRC_OTHER;
    }

    /*
     * Put the new one in the user's AppData.
     * I don't think this value is actually used.
     */
    snprintf(path, MAX_PATH, "%s%s%s", mya, session_name, SESS_SUFFIX);
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
 * @return TRUE for success, FALSE for error.
 */
#define SESSION_NAME_ERR \
"Illegal character(s).\n\
Session names can only have letters, numbers, spaces, underscores and dashes."
static int
legal_session_name(const char *name, char *result, size_t result_size)
{
    if (strspn(name, LEGAL_CNAME) != strlen(name)) {
	if (result != NULL) {
	    snprintf(result, result_size, "%s", SESSION_NAME_ERR);
	} else {
	    printf("\n%s", SESSION_NAME_ERR);
	}
	return FALSE;
    } else {
	return TRUE;
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
 * @param[in] explicit_edit	If TRUE, -e was passed on command line; skip
 * 				the 'exists. Edit?' dialog
 * @param[out] src		Where the session file was found, if it exists
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
	int explicit_edit, src_t *src)
{
    FILE *f;
    int rc;
    int editable;

    *src = SRC_OTHER;

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
		if (!strncmp(path, mya, strlen(mya)) &&
			path[strlen(mya)] == '\\') {
		    *src = SRC_CURRENT;
		} else if (commona != NULL && !strncmp(path, commona,
						    strlen(commona)) &&
			path[strlen(commona)] == '\\') {
		    *src = SRC_ALL;
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
	    if (fixup_printer(s)) {
		    printf("\n"
"NOTE: This session file contains a UNC printer name that needs to be updated\n"
" to be compatible with the current version of wc3270.  Even if you do not\n"
" need to make any other changes to the session, please select the Edit and\n"
" Update options to have this name automatically corrected.\n");
	    }
	}

	if (editable) {
	    if (explicit_edit) {
		return GS_EDIT; /* edit it */
	    }
	    for (;;) {
		printf("\nSession '%s' exists", s->session);
		switch (*src) {
		case SRC_ALL:
		    printf(" (defined for all users)");
		    break;
		case SRC_CURRENT:
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
    OSVERSIONINFO info;
    int has_ipv6 = 1;

    /* Win2K and earlier is IPv4-only.  WinXP and later can have IPv6. */
    memset(&info, '\0', sizeof(info));
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(&info) == 0 || info.dwMajorVersion < 5 ||
	    (info.dwMajorVersion == 5 && info.dwMinorVersion < 1)) {
	has_ipv6 = 0;
    }

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
number, character set, etc.), enter '" CHOICE_NONE "'."

    if (has_ipv6) {
	new_screen(s, NULL, COMMON_HOST_TEXT1 ", " COMMON_HOST_TEXT2 " or "
		IPV6_HOST_TEXT "." COMMON_HOST_TEXT3);
    } else {
	new_screen(s, NULL, COMMON_HOST_TEXT1 " or " COMMON_HOST_TEXT2 "."
		COMMON_HOST_TEXT3);
    }

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
	    printf("\nInvalid character '%c' in host name.",
		    buf[n_good]);
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
	    printf("\nInvalid port.");
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
	    printf("\nLU name contains invalid character '%c'", buf[n_good]);
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
	    printf("\nInvalid model number.");
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
	    printf("[%u x %u]: ", s->ov_rows, s->ov_cols);
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
	    printf("\nPlease enter oversize in the form 'rows x cols'.");
	    continue;
	} else if ((int)r < wrows[s->model] || (int)c < wcols[s->model]) {
	    printf("\nOversize must be larger than the default for a model %d "
		    "(%u x %u).",
		    (int)s->model, wrows[s->model], wcols[s->model]);
	    continue;
	} else if (r > 255 || c > 255) {
	    printf("\nRows and columns must be 255 or less.");
	    continue;
	} else if (r * c > 0x4000) {
	    printf("\nThe total screen area (rows multiplied by columns) must "
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
    if (windows_major_version < 5) {
	printf("\n\
Note: wc3270 DBCS is supported only on Windows XP and later.\n");
	goto any_key;
    }
    if (windows_major_version == 5) {
	printf("\n\
Note: wc3270 DBCS support on Windows XP requires installation of Windows East\n\
Asian language support.\n");
	goto any_key;
    }
    if (windows_major_version >= 6) {
	printf("\n\
Note: wc3270 DBCS support on Windows Vista and later requires setting the\n\
Windows System Locale to a matching language.\n");
	goto any_key;
    }

    return;

any_key:
    printf("[Press Enter to continue] ");
    fflush(stdout);
    (void) getchar();
}

/**
 * Prompt for a character set.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for error
 */
static int
get_charset(session_t *s)
{
    char buf[STR_SIZE];
    unsigned i, k;
    char *ptr;
    unsigned long u;
    int was_dbcs = s->is_dbcs;

    new_screen(s, NULL, "\
Character Set\n\
\n\
This specifies the EBCDIC character set (code page) used by the host.");

    printf("\
\nAvailable character sets:\n\n\
  #  Name                Host CP      #  Name                Host CP\n\
 --- ------------------- --------    --- ------------------- --------\n");
    k = 0;
    for (i = 0; charsets[i].name != NULL; i++) {
	int j;

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
	    j += num_charsets / 2;
	    k++;
	}
	printf(" %2d. %-*s %-*s",
		j + 1,
		CS_WIDTH, charsets[j].name,
		CP_WIDTH, charsets[j].hostcp);
    }
    printf("\n");
    for (;;) {
	printf("\nCharacter set: [%s] ", s->charset);
	if (get_input(buf, sizeof(buf)) == NULL) {
	    return -1;
	}
	if (!buf[0]) {
	    break;
	}
	/* Check for numeric value. */
	u = strtoul(buf, &ptr, 10);
	if (u > 0 && u <= i && *ptr == '\0') {
	    strcpy(s->charset, charsets[u - 1].name);
	    s->is_dbcs = charsets[u - 1].is_dbcs;
	    break;
	}
	/* Check for alias. */
	for (i = 0; charset_alias[i].alias != NULL; i++) {
	    if (!strcmp(buf, charset_alias[i].alias)) {
		strcpy(buf, charset_alias[i].real);
		break;
	    }
	}
	/* Check for name match. */
	for (i = 0; charsets[i].name != NULL; i++) {
	    if (!strcmp(buf, charsets[i].name)) {
		strcpy(s->charset, charsets[i].name);
		s->is_dbcs = charsets[i].is_dbcs;
		break;
	    }
	}

	if (charsets[i].name != NULL) {
	    break;
	}
	printf("\nInvalid character set name.");
    }

    if (!was_dbcs && s->is_dbcs) {
	dbcs_check();
    }

    return 0;
}

#if defined(HAVE_LIBSSL) /*[*/
/**
 * Prompt for SSL tunnel mode.
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_ssl(session_t *s)
{
    new_screen(s, NULL, "\
SSL Tunnel\n\
\n\
This option causes wc3270 to first create a tunnel to the host using the\n\
Secure Sockets Layer (SSL), then to run the TN3270 session inside the tunnel.");

    do {
	printf("\nUse an SSL tunnel? (y/n) [%s] ", s->ssl? "y" : "n");
	fflush(stdout);
	s->ssl = getyn(s->ssl);
	if (s->ssl == YN_ERR) {
	    return -1;
	}
    } while (s->ssl < 0);
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
if an SSL tunnel is used, or if the TELNET TLS option is negotiated.  If the\n\
certificates are not valid, the connection will be aborted.");

    do {
	printf("\nVerify host certificates? (y/n) [%s] ",
		(s->flags & WF_VERIFY_HOST_CERTS)? "y" : "n");
	fflush(stdout);
	rc = getyn((s->flags & WF_VERIFY_HOST_CERTS) != 0);
	switch (rc) {
	case YN_ERR:
	    return -1;
	case TRUE:
	    s->flags |= WF_VERIFY_HOST_CERTS;
	    break;
	case FALSE:
	    s->flags &= ~WF_VERIFY_HOST_CERTS;
	    break;
	}
    } while (rc < 0);
    return 0;
}
#endif /*]*/

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
	    printf("\nServer name cannot include '[' or ']'.");
	    continue;
	}
	strcpy(s->proxy_host, hbuf);
	break;
    }
    return 0;
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
    char pbuf[STR_SIZE];
    int i;

    for (i = 0; proxies[i].name != NULL; i++) {
	if (!strcmp(s->proxy_type, proxies[i].name))
	    break;
    }
    if (proxies[i].name == NULL) {
	printf("Internal error\n");
	return -1;
    }

    for (;;) {
	unsigned long l;
	char *ptr;

	if (s->proxy_port[0]) {
	    printf("\nProxy server TCP port: [%s] ", s->proxy_port);
	} else if (proxies[i].port != NULL) {
	    printf("\nProxy server TCP port: [%s] ", proxies[i].port);
	} else {
	    printf("\nProxy server TCP port: ");
	}
	if (get_input(pbuf, STR_SIZE) == NULL) {
	    return -1;
	} else if (!strcmp(pbuf, "default") && proxies[i].port != NULL) {
	    strcpy(s->proxy_port, proxies[i].port);
	    break;
	} else if (!pbuf[0]) {
	    if (s->proxy_port[0]) {
		break;
	    } else if (proxies[i].port != NULL) {
		strcpy(s->proxy_port, proxies[i].port);
		break;
	    } else {
		continue;
	    }
	}
	l = strtoul(pbuf, &ptr, 10);
	if (l == 0 || *ptr != '\0' || (l & ~0xffffL)) {
	    printf("\nInvalid port.");
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
    int i, j;
    char tbuf[STR_SIZE];
    char old_proxy[STR_SIZE];

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
	for (j = 0; proxies[j].name != NULL; j++) {
	    if (!strcasecmp(tbuf, proxies[j].name)) {
		break;
	    }
	}
	if (proxies[j].name != NULL) {
	    strcpy(s->proxy_type, tbuf);
	    break;
	}
	n = atoi(tbuf);
	if (n > 0 && n <= i+1) {
	    if (n == 1) {
		s->proxy_type[0] = '\0';
		s->proxy_host[0] = '\0';
		s->proxy_port[0] = '\0';
		return 0;
	    } else {
		j = n - 2;
		strcpy(s->proxy_type, proxies[j].name);
		break;
	    }
	}
	printf("\nInvalid proxy type.");
    }

    /* If the type changed, the rest of the information is invalid. */
    if (strcmp(old_proxy, s->proxy_type)) {
	s->proxy_host[0] = '\0';
	s->proxy_port[0] = '\0';

	if (get_proxy_server(s) < 0) {
	    return -1;
	}

	if (proxies[j].port != NULL) {
	    strcpy(s->proxy_port, proxies[j].port);
	} else if (get_proxy_server_port(s) < 0) {
	    return -1;
	}
    }

    return 0;
}

/**
 * Prompt for wpr3287 session
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_wpr3287(session_t *s)
{
    new_screen(s, NULL, "\
wpr3287 Session\n\
\n\
This option allows wc3270 to automatically start a wpr3287 printer session\n\
when it connects to the host, allowing the host to direct print jobs to a\n\
Windows printer.");

    do {
	printf("\nAutomatically start a wpr3287 printer session? (y/n) [n] ");
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
 * Prompt for wpr3287 session mode (associate/LU)
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
wpr3287 Session -- Printer Mode\n\
\n\
The wpr3287 printer session can be configured in one of two ways.  The first\n\
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
 * Prompt for wpr3287 session LU name
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_printerlu(session_t *s, int explain)
{
    if (explain) {
	new_screen(s, NULL, "\
wpr3287 Session -- Printer Logical Unit (LU) Name\n\
\n\
If the wpr3287 printer session is associated with a particular Logical Unit,\n\
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
 * Prompt for wpr3287 session printer name
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

    new_screen(s, NULL, "\
wpr3287 Session -- Windows Printer Name\n\
\n\
The wpr3287 session can use the Windows default printer as its real printer,\n\
or you can specify a particular Windows printer.  You can specify a local\n\
printer, or specify a remote printer with a UNC path, e.g.,\n\
'\\\\server\\printer22'.  You can specify the Windows default printer with\n\
the name 'default'.");

    (void) redisplay_printer(s->printer, cbuf);

    enum_printers();
    if (num_printers) {
	printf("\nWindows printers (system default is '*'):\n");
	for (i = 0; i < num_printers; i++) {
	    printf(" %2d. %c %s\n", i + 1,
		    strcasecmp(default_printer,
			printer_info[i].pName)? ' ': '*',
		    printer_info[i].pName);
	    if (!strcasecmp(cbuf, printer_info[i].pName)) {
		matching_printer = i;
	    }
	}
	printf(" %2d.   Other\n", num_printers + 1);
	if (cbuf[0] && matching_printer < 0) {
	    matching_printer = num_printers;
	}
	for (;;) {
	    if (s->printer[0]) {
		    printf("\nEnter Windows printer (1-%d): [%d] ",
			    num_printers + 1, matching_printer + 1);
	    } else {
		    printf("\nEnter Windows printer (1-%d): [use system "
			    "default] ",
			    num_printers + 1);
	    }
	    fflush(stdout);
	    if (get_input(tbuf, STR_SIZE) == NULL) {
		return -1;
	    } else if (!tbuf[0]) {
		if (!s->printer[0] || matching_printer < (int)num_printers) {
		    break;
		}
		/*
		 * An interesting hack. If they entered nothing, and the
		 * default is 'other', pretend they typed in the number for
		 * 'other'.
		 */
		snprintf(tbuf, sizeof(tbuf), "%d",
			matching_printer + 1);
	    } else if (!strcmp(tbuf, "default")) {
		s->printer[0] = '\0';
		break;
	    }
	    u = strtoul(tbuf, &ptr, 10);
	    if (*ptr != '\0' || u == 0 || u > num_printers + 1) {
		continue;
	    } else if (u == num_printers + 1) {
		if (get_printer_name(cbuf, tbuf, STR_SIZE) < 0) {
		    return -1;
		}
		strcpy(s->printer, tbuf);
		break;
	    }
	    strcpy(s->printer, printer_info[u - 1].pName);
	    break;
	}
    } else {
	if (get_printer_name(cbuf, tbuf, STR_SIZE) < 0) {
	    return -1;
	}
	strcpy(s->printer, tbuf);
    }

    /*
     * If the resulting printer name is a UNC path, double the
     * backslashes.
     */
    (void) fixup_printer(s);
    return 0;
}

/**
 * Prompt for wpr3287 session printer code page
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
wpr3287 Session -- Printer Code Page\n\
\n\
By default, wpr3287 uses the system's default ANSI code page.  You can\n\
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
	    printf("\nInvald code page.");
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
	}
	printf("\n");
    }

    for (;;) {
	char inbuf[STR_SIZE];
	char tknbuf[STR_SIZE];
	char *t;
	char *buf;
	int wrong = FALSE;

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
	wrong = FALSE;
	buf = tknbuf;
	while (!wrong && (t = strtok(buf, ",")) != NULL) {
	    buf = NULL;
	    for (km = km_first; km != NULL; km = km->next) {
		if (!strcasecmp(t, km->name)) {
		    break;
		}
	    }
	    if (km == NULL) {
		printf("Invalid keymap name '%s'.", t);
		wrong = TRUE;
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
 * Prompt for screen font size
 *
 * @param[in,out] s	Session
 *
 * @return 0 for success, -1 for failure
 */
static int
get_fontsize(session_t *s)
{
    new_screen(s, NULL, "\
Font Size\n\
\n\
Allows the font size (character height in pixels) to be specified for the\n\
wc3270 window.  The size must be between 5 and 72.  The default is 12.");

    for (;;) {
	char inbuf[STR_SIZE];
	unsigned long u;
	char *ptr;

	printf("\nFont size (5 to 72) [%u]: ",
		s->point_size? s->point_size: 12);
	fflush(stdout);
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	} else if (!inbuf[0]) {
	    break;
	} else if (!strcasecmp(inbuf, CHOICE_NONE)) {
	    s->point_size = 0;
	    break;
	}
	u = strtoul(inbuf, &ptr, 10);
	if (*ptr != '\0' || u == 0 || u < 5 || u > 72) {
	    printf("\nInvalid font size.");
	    continue;
	}
	s->point_size = (unsigned char)u;
	break;
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
be left in the wc370 AppData directory.");

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
This option will start up the Windows Notapad editor to allow you to edit\n\
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
	printf("Error creating temporary session file name.\n");
	goto failed;
    }
    f = fopen(t, "w");
    if (f == NULL) {
	printf("Error creating temporary session file.\n");
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
	printf("Error reading back temporary session file.\n");
	goto failed;
    }
    new_us = NULL;
    if (read_user_settings(f, &new_us) == 0) {
	printf("Error reading back temporary session file.\n");
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
    printf("[Press <Enter>] ");
    fflush(stdout);
    (void) fgets(buf, 2, stdin);
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
 * AppData).
 *
 * @param[in] s		Session
 *
 * @return 0 for success, -1 for failure
 */
static src_t
get_src(const char *name, src_t def)
{
    char ac[STR_SIZE];

    /* Ask where they want the file. */
    for (;;) {
	printf("\nCreate '%s' for all users or current user '%s'? "
		"(all/current) [%s] ",
		name, username, (def == SRC_CURRENT)? "current": "all");
	fflush(stdout);
	if (get_input(ac, STR_SIZE) == NULL) {
	    return SRC_ERR;
	} else if (!ac[0]) {
	    return def;
	} else if (!strncasecmp(ac, "all", strlen(ac))) {
	    return SRC_ALL;
	} else if (!strncasecmp(ac, "current", strlen(ac)) ||
		   !strcasecmp(ac, username)) {
	    return SRC_CURRENT;
	} else if (!strncasecmp(ac, "quit", strlen(ac))) {
	    return SRC_NONE;
	} else {
	    printf("\nPlease answer (a)ll or (c)urrent.");
	}
    }
}

/**
 * Display the current settings for a session and allow them to be edited.
 *
 * @param[in,out] s	Session
 * @param[in,out] us	User settings
 * @param[in] how	How session is being edited (replace/create/update)
 * @param[in] path	Session pathname
 * @param[in] session_name Name of session
 *
 * @return 0 for success, -1 for failure
 */
static src_t
edit_menu(session_t *s, char **us, sp_t how, const char *path,
	const char *session_name)
{
    int rc;
    char choicebuf[32];
    session_t old_session;
    char *old_us = NULL;
    src_t ret = SRC_NONE;

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
	    printf("Out of memory.\n");
	    exit(1);
	}
    }

    for (;;) {
	int done = 0;
	char *cp = "?";
	int i;

	for (i = 0; charsets[i].name != NULL; i++) {
	    if (!strcmp(charsets[i].name, s->charset)) {
		cp = charsets[i].hostcp;
		break;
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
	printf("%3d. Character Set .......... : %s (CP %s)\n",
		MN_CHARSET, s->charset, cp);
#if defined(HAVE_LIBSSL) /*[*/
	printf("%3d. SSL Tunnel ............. : %s\n", MN_SSL,
		s->ssl? "Yes": "No");
	printf("%3d. Verify host certificates : %s\n", MN_VERIFY,
		(s->flags & WF_VERIFY_HOST_CERTS)? "Yes": "No");
#endif /*]*/
	printf("%3d. Proxy .................. : %s\n", MN_PROXY,
		s->proxy_type[0]? s->proxy_type: DISPLAY_NONE);
	if (s->proxy_type[0]) {
	    printf("%3d.  Proxy Server .......... : %s\n",
		    MN_PROXY_SERVER, s->proxy_host);
	    if (s->proxy_port[0]) {
		printf("%3d.  Proxy Server TCP Port . : %s\n",
			MN_PROXY_PORT, s->proxy_port);
	    }
	}
	printf("%3d. wpr3287 Printer Session  : %s\n", MN_3287,
		s->wpr3287? "Yes": "No");
	if (s->wpr3287) {
	    char pbuf[STR_SIZE];

	    printf("%3d.  wpr3287 Mode .......... : ",
		    MN_3287_MODE);
	    if (!strcmp(s->printerlu, ".")) {
		printf("Associate\n");
	    } else {
		printf("LU\n");
		printf("%3d.  wpr3287 LU ............ : %s\n",
			MN_3287_LU, s->printerlu);
	    }
	    (void) redisplay_printer(s->printer, pbuf);
	    printf("%3d.  wpr3287 Windows printer : %s\n",
		    MN_3287_PRINTER,
		    s->printer[0]? pbuf: "(system default)");
	    printf("%3d.  wpr3287 Code Page ..... : ",
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
	printf("%3d. Font Size .............. : %u\n",
		MN_FONT_SIZE,
		s->point_size? s->point_size: 12);
	printf("%3d. Background Color ....... : %s\n", MN_BG,
		(s->flags & WF_WHITE_BG)? "white": "black");
	printf("%3d. Menu Bar ............... : %s\n", MN_MENUBAR,
		(s->flags & WF_NO_MENUBAR)? "No": "Yes");
	printf("%3d. Trace at start-up ...... : %s\n", MN_TRACE,
		(s->flags & WF_TRACE)? "Yes": "No");
	printf("%3d. Edit miscellaneous resources with Notepad\n",
		MN_NOTEPAD);

	for (;;) {
	    int invalid = 0;
	    int was_wpr3287 = 0;

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
	    case MN_CHARSET:
		if (get_charset(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
#if defined(HAVE_LIBSSL) /*[*/
	    case MN_SSL:
		if (get_ssl(s) < 0) {
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
#endif /*]*/
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
		    printf("Invalid entry.\n");
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
		    printf("Invalid entry.\n");
		    invalid = 1;
		}
		break;
	    case MN_3287:
		was_wpr3287 = s->wpr3287;
		if (get_wpr3287(s) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		if (s->wpr3287 && !was_wpr3287) {
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
		    printf("Invalid entry.\n");
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
		    printf("Invalid entry.\n");
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
		    printf("Invalid entry.\n");
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
		    printf("Invalid entry.\n");
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
	    case MN_FONT_SIZE:
		if (get_fontsize(s) < 0) {
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
	    case MN_NOTEPAD:
		if (run_notepad(s, us) < 0) {
		    ret = SRC_ERR;
		    goto done;
		}
		break;
	    default:
		printf("\nInvalid entry.");
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

    /* Ask if they want to write the file. */
    if (memcmp(s, &old_session, sizeof(session_t)) ||
	((old_us != NULL) ^ (*us != NULL)) ||
	(old_us != NULL && strcmp(old_us, *us))) {
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
	ret = get_src(session_name, SRC_CURRENT);
	goto done;
    }

    /* Return where the file ended up. */
    if (!strncasecmp(mya, path, strlen(mya))) {
	ret = SRC_CURRENT;
	goto done;
    } else if (!strncasecmp(commona, path, strlen(commona))) {
	ret = SRC_ALL;
	goto done;
    } else {
	ret = SRC_OTHER;
	goto done;
    }

done:
    if (old_us != NULL) {
	free(old_us);
    }
    return ret;
}

/**
 * Translate a wc3270 character set name to a font for the console.
 *
 * @param[in] cset	Character set name
 * @param[out] codepage	Windows codepage
 *
 * @return Font name
 */
static wchar_t *
reg_font_from_cset(const char *cset, int *codepage)
{
    unsigned i, j;
    wchar_t *cpname = NULL;
    wchar_t data[1024];
    DWORD dlen;
    HKEY key;
    static wchar_t font[1024];
    DWORD type;

    *codepage = 0;

    /* Search the table for a match. */
    for (i = 0; charsets[i].name != NULL; i++) {
	if (!strcmp(cset, charsets[i].name)) {
	    cpname = charsets[i].codepage;
	    break;
	}
    }

    /* If no match, use Lucida Console. */
    if (cpname == NULL) {
	return L"Lucida Console";
    }

    /*
     * Look in the registry for the console font associated with the
     * Windows code page.
     */
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\Microsoft\\Windows NT\\CurrentVersion\\"
		"Console\\TrueTypeFont",
		0,
		KEY_READ,
		&key) != ERROR_SUCCESS) {
	printf("RegOpenKey failed -- cannot find font\n");
	return L"Lucida Console";
    }
    dlen = sizeof(data);
    if (RegQueryValueExW(key,
		cpname,
		NULL,
		&type,
		(LPVOID)data,
		&dlen) != ERROR_SUCCESS) {
	/* No codepage-specific match, try the default. */
	dlen = sizeof(data);
	if (RegQueryValueExW(key, L"0", NULL, &type, (LPVOID)data,
		    &dlen) != ERROR_SUCCESS) {
	    RegCloseKey(key);
	    printf("RegQueryValueEx failed -- cannot find font\n");
	    return L"Lucida Console";
	}
    }
    RegCloseKey(key);
    if (type == REG_MULTI_SZ) {
	for (i = 0; i < dlen/sizeof(wchar_t); i++) {
	    if (data[i] == 0x0000) {
		break;
	    }
	}
	if (i+1 >= dlen/sizeof(wchar_t) || data[i+1] == 0x0000) {
	    printf("Bad registry value -- cannot find font\n");
	    return L"Lucida Console";
	}
	i++;
    } else {
	i = 0;
    }
    for (j = 0; i < dlen; i++, j++) {
	if (j == 0 && data[i] == L'*') {
	    i++;
	} else if ((font[j] = data[i]) == 0x0000) {
		break;
	}
    }
    *codepage = _wtoi(cpname);
    return font;
}

/**
 * Print the prefix for a session name (ordinal or blank)
 *
 * @param[in] n			Ordinal to display
 * @param[in] with_numbers 	If TRUE, display number, otherwise blanks
 */
static void
print_n(int n, int with_numbers)
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
 * @param[in] with_numbers	If TRUE, display with ordinals
 */
static void
display_sessions(int with_numbers)
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

	if (i == 0 && xs_current.count != 0) {
	    printf("Sessions for user '%s':\n", username);
	} else if (i == xs_current.count) {
	    if (col) {
		printf("\n");
		col = 0;
	    }
	    printf("Sessions for all users:\n");
	}

	slen = strlen(n);

    retry:
	switch (col) {
	default:
	case 0:
	    print_n(i, with_numbers);
	    printf(" %s", n);
	    if (slen <= 15) { /* fits in column 0 */
		printf("%*s", 15 - slen, "");
		col = 1;
	    } else if (slen <= 15 + 20) { /* covers 0 and 1 */
		printf("%*s", 15 + 20 - slen, "");
		col = 2;
	    } else if (slen <= 15 + 20 + 20) { /* covers 0, 1, 2 */
		printf("%*s", 15 + 20 + 20 - slen, "");
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
		printf("%*s", 15 - slen, "");
		col = 2;
	    } else if (slen <= 15 + 20) { /* covers 1 and 2 */
		printf("%*s", 15 + 20 - slen, "");
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
		printf("%*s", 15 - slen, "");
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
 * @param[in] why	Name of operation in progress
 * @param[out] name	Returned selected session name
 * @param[out] lp	Returned selected session location
 *
 * @return -1 for error, 0 for success
 * If no name is chosen, returns 0, but also returns NULL in name.
 */
static int
get_existing_session(const char *why, const char **name, src_t *lp)
{
    char nbuf[64];

    display_sessions(1);

    for (;;) {
	int n;

	printf("\nEnter session name or number (1..%d) to %s, or 'q' "
		"to quit: ", num_xs, why);
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

	    for (i = 0; i < num_xs; i++) {
		if (!strcasecmp(nbuf, xs_name(i + 1, NULL))) {
		    *name = xs_name(i + 1, lp);
		    return 0;
		}
	    }
	    printf("\nNo such session.");
	    continue;
	} else if (n < 0 || n > num_xs) {
	    printf("\nNo such session.");
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
 * @param[out] lp		Returned location of session
 * @param[out] result		Buffer to put error message in
 * @param[in] result_size	Size of 'result' buffer
 *
 * @return Session name, or NULL if not found
 */
static char *
menu_existing_session(char *name, src_t *lp, char *result, size_t result_size)
{
    int i;

    for (i = 0; i < num_xs; i++) {
	if (!strcasecmp(name, xs_name(i + 1, lp))) {
	    break;
	}
    }
    if (i >= num_xs) {
	snprintf(result, result_size, "No such session: '%s'", name);
	return NULL;
    } else {
	return name;
    }
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

    printf("[Press <Enter>] ");
    fflush(stdout);
    (void) fgets(buf, sizeof(buf), stdin);
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
    src_t l;
    char path[MAX_PATH];

    if (argc > 0) {
	name = menu_existing_session(argv[0], &l, result, result_size);
	if (name == NULL) {
	    return 0;
	}
    }

    if (argc == 0) {
	new_screen(&empty_session, NULL, "\
Delete Session\n");

	if (get_existing_session("delete", &name, &l) < 0) {
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

    snprintf(path, MAX_PATH, "%s%s%s", (l == SRC_CURRENT)? mya: commona,
	    name, SESS_SUFFIX);
    if (unlink(path) < 0) {
	printf("\nDelete of '%s' failed: %s\n", path, strerror(errno));
	goto failed;
    }
    snprintf(path, MAX_PATH, "%s%s.lnk",
	    (l == SRC_CURRENT)? desktop: common_desktop, name);
    if (access(path, R_OK) == 0 && unlink(path) < 0) {
	printf("\nDelete of '%s' failed: %s\n", path, strerror(errno));
	goto failed;
    }

    snprintf(result, result_size, "Session '%s' deleted.", name);
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
 * @param[in] is_rename	TRUE if rename, FALSE if copy
 * @param[out] result	Result returned here
 * @param[in] result_size Size of 'result' buffer
 *
 * @return 0 for success, -1 for failure
 */
static int
rename_or_copy_session(int argc, char **argv, int is_rename, char *result,
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
	from_name = menu_existing_session(argv[0], &from_l, result,
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
	if (get_existing_session(is_rename? "rename": "copy", &from_name,
		    &from_l) < 0) {
	    return -1;
	} else if (from_name == NULL) {
	    return 0;
	}
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
	    } else if (to_name[0] == 'q' || to_name[0] == 'Q') {
		return 0;
	    }
	}
	for (i = 0; i < num_xs; i++) {
	    if (!strcasecmp(to_name, xs_name(i + 1, NULL))) {
		break;
	    }
	}
	if (i < num_xs) {
	    printf("\nSession '%s' already exists. To replace it, you must "
		    "delete it first.", to_name);
	    continue;
	}
	if (!legal_session_name(to_name, NULL, 0)) {
	    continue;
	}
	break;
    }

    switch (from_l) {
    case SRC_ALL:
	snprintf(from_path, MAX_PATH, "%s%s%s", commona, from_name,
		SESS_SUFFIX);
	break;
    default:
    case SRC_CURRENT:
	snprintf(from_path, MAX_PATH, "%s%s%s", mya, from_name, SESS_SUFFIX);
	break;
    }

    switch ((to_l = get_src(to_name, from_l))) {
    case SRC_ALL:
	snprintf(to_path, MAX_PATH, "%s%s%s", commona, to_name, SESS_SUFFIX);
	break;
    case SRC_CURRENT:
	snprintf(to_path, MAX_PATH, "%s%s%s", mya, to_name, SESS_SUFFIX);
	break;
    case SRC_NONE:
	return 0;
    default:
	return -1;
    }

    /* Read in the existing session. */
    f = fopen(from_path, "r");
    if (f == NULL) {
	perror(from_path);
	return -1;
    }
    if (!read_session(f, &s, &us)) {
	fclose(f);
	printf("Cannot read '%s'.\n", from_path);
	goto failed;
    }
    fclose(f);

    /* Change its name and write it back out. */
    strncpy(s.session, to_name, STR_SIZE);
    if (write_session_file(&s, us, to_path) < 0) {
	printf("Cannot write '%s'.\n", to_path);
	goto failed;
    }

    /* Remove the orginal. */
    if (is_rename) {
	if (unlink(from_path) < 0) {
	    printf("Cannot remove '%s'.\n", from_path);
	    goto failed;
	}
    }

    /* See about the shortcut as well. */
    snprintf(from_linkpath, sizeof(from_linkpath), "%s%s.lnk",
	    (from_l == SRC_ALL)? common_desktop: desktop, from_name);
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
	wsrc = write_shortcut(&s, FALSE, to_l, to_path);
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
		printf("Cannot remove '%s'.\n", from_linkpath);
		goto failed;
	    }
	}
    }

    snprintf(result, result_size, "Session '%s' %s to '%s'.",
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
    src_t l;
    char from_path[MAX_PATH];
    FILE *f;
    ws_t rc;
    session_t s;

    if (argc > 0) {
	name = menu_existing_session(argv[0], &l, result, result_size);
	if (name == NULL) {
	    return 0;
	}
    }

    if (argc == 0) {
	new_screen(&empty_session, NULL, "\
Create Shortcut\n");

	if (get_existing_session("create shortcut for", &name, &l) < 0) {
	    return -1;
	} else if (name == NULL) {
	    return 0;
	}
    }

    switch (l) {
    case SRC_ALL:
	snprintf(from_path, MAX_PATH, "%s%s%s", commona, name, SESS_SUFFIX);
	break;
    default:
    case SRC_CURRENT:
	snprintf(from_path, MAX_PATH, "%s%s%s", mya, name, SESS_SUFFIX);
	break;
    }

    f = fopen(from_path, "r");
    if (f == NULL) {
	perror(from_path);
	goto failed;
    } else if (!read_session(f, &s, NULL)) {
	fclose(f);
	printf("Cannot read '%s'.\n", from_path);
	goto failed;
    }
    fclose(f);

    rc = write_shortcut(&s, FALSE, l, from_path);
    switch (rc) {
    case WS_NOP:
	break;
    case WS_ERR:
	return -1;
    case WS_FAILED:
	goto failed;
    case WS_CREATED:
    case WS_REPLACED:
	snprintf(result, result_size, "Shortcut %s for '%s'.",
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

    sprintf(dpath, "%s*%s", dirname, SESS_SUFFIX);
    h = FindFirstFile(dpath, &find_data);
    if (h != INVALID_HANDLE_VALUE) {
	do {
	    char *sname;
	    size_t nlen;
	    xs_t *xss, *prev;

	    sname = find_data.cFileName;
	    nlen = strlen(sname) - strlen(SESS_SUFFIX);

	    if (location == SRC_ALL) {
		int skip = 0;
		xs_t *xsc;

		for (xsc = xs_current.list; xsc != NULL; xsc = xsc->next) {
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
		printf("Out of memory\n");
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
	free(list->name);
	free(list);
	list = next;
    }

    xsb->count = 0;
    xsb->list = NULL;
}

/**
 * Initialize the session names.
 */
static void
xs_init(void)
{
    free_xs(&xs_current);
    free_xs(&xs_all);
    num_xs = 0;

    xs_init_type(mya, &xs_current, SRC_CURRENT);
    if (commona != NULL) {
	xs_init_type(commona, &xs_all, SRC_ALL);
    }
    num_xs = xs_current.count + xs_all.count;
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

    for (xs = xs_current.list; xs != NULL; xs = xs->next) {
	if (!--n) {
	    if (lp != NULL) {
		*lp = xs->location;
	    }
	    return xs->name;
	}
    }
    for (xs = xs_all.list; xs != NULL; xs = xs->next) {
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
 * @param[in] ask	If TRUE, ask first
 * @param[in] src	Where the session file is (all or current user)
 * @param[in] sess_path	Pathname of session file
 *
 * @return ws_t (no-op, create, replace, error)
 */
static ws_t
write_shortcut(const session_t *s, int ask, src_t src, const char *sess_path)
{
    char linkpath[MAX_PATH];
    char exepath[MAX_PATH];
    char args[MAX_PATH];
    int shortcut_exists;
    int extra_height = 1;
    wchar_t *font;
    int codepage = 0;
    HRESULT hres;

    /* Ask about the shortcut. */
    sprintf(linkpath, "%s%s.lnk",
	    (src == SRC_ALL)? common_desktop: desktop,
	    s->session);
    shortcut_exists = (access(linkpath, R_OK) == 0);
    if (ask) {
	for (;;) {
	    int rc;

	    printf("\n%s desktop shortcut (y/n) [%s]: ",
		    shortcut_exists? "Replace": "Create",
		    installed? "y": "n");
	    rc = getyn(installed == TRUE);
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

    font = reg_font_from_cset(s->charset, &codepage);

    hres = CreateLink(
	    exepath,		/* path to executable */
	    linkpath,		/* where to put the link */
	    "wc3270 session",	/* description */
	    args,			/* arguments */
	    installdir,		/* working directory */
	    (s->ov_rows?	/* console rows */
		s->ov_rows: wrows[s->model]) + extra_height,
	    s->ov_cols?	/* console cols */
		s->ov_cols: wcols[s->model],
	    font,			/* font */
	    s->point_size,	/* point size */
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
 * @param[in] explicit_edit	If TRUE, '-e' option was used (no need to
 * 				confirm they want to edit it)
 * @param[out] result		Buffer containing previous operation's result,
 * 				and to write current operation's result into
 * @param[in] result_size	Size of 'result' buffer
 *
 * @return Status of operation (success/error/user-quit)
 */
static sw_t
session_wizard(const char *session_name, int explicit_edit, char *result,
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

    /* Start with nothing. */
    (void) memset(&session, '\0', sizeof(session));

    /* Find the existing sessions. */
    xs_init();

    /* Intro screen. */
    if (session_name == NULL) {
	switch (main_menu(&argc, &argv, result)) {
	case MO_ERR:
	    return SW_ERR;
	case MO_QUIT:
	    return SW_QUIT;
	case MO_EDIT:
	    if (argc > 0) {
		session_name = menu_existing_session(argv[0], NULL, result,
			result_size);
		if (session_name == NULL) {
		    return SW_SUCCESS;
		}
	    } else {
		new_screen(&session, NULL, "\
Edit Session\n");
		if (get_existing_session("edit", &session_name, NULL) < 0) {
		    return SW_ERR;
		} else if (session_name == NULL) {
		    return SW_SUCCESS;
		}
	    }
	    explicit_edit = TRUE;
	    break;
	case MO_DELETE:
	    if (delete_session(argc, argv, result, result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_COPY:
	    if (rename_or_copy_session(argc, argv, FALSE, result,
			result_size) < 0) {
		return SW_ERR;
	    } else {
		return SW_SUCCESS;
	    }
	case MO_RENAME:
	    if (rename_or_copy_session(argc, argv, TRUE, result,
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
	}
    } else {
	new_screen(&session, NULL, "");
    }

    /* Get the session name. */
    rc = get_session(session_name, &session, &us, path, explicit_edit, &src);
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
	strcpy(session.charset, "bracket");
	strcpy(session.printerlu, ".");
	/* fall through... */
    case GS_EDIT:		/* Edit existing file. */
	/* See what they want to change. */
	src = edit_menu(&session, &us,
		(rc == GS_OVERWRITE)? SP_REPLACE:
		 ((rc == GS_NEW)? SP_CREATE: SP_UPDATE),
		path, session.session);
	if (src == SRC_ERR) {
	    return SW_ERR;
	} else if (src == SRC_NONE) {
	    if (rc == GS_NEW) {
		return SW_SUCCESS;
	    } else {
		break;
	    }
	} else if (src == SRC_ALL) {
	    /* All users. */
	    snprintf(path, MAX_PATH, "%s%s%s", commona, session.session,
		    SESS_SUFFIX);
	} else if (src == SRC_CURRENT) {
	    /* Current user. */
	    snprintf(path, MAX_PATH, "%s%s%s", mya, session.session,
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
	snprintf(result, result_size, "Wrote session '%s'.", session.session);
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
    wsrc = write_shortcut(&session, TRUE, src, path);
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
		"%s%s shortcut '%s'.",
		sl? "\n": "",
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

    f = fopen(path, "w+");
    if (f == NULL) {
	printf("Cannot create session file: %s", strerror(errno));
	return -1;
    }

    fprintf(f, "! wc3270 session '%s'\n", session->session);

    t = time(NULL);
    fprintf(f, "! Created or modified by the wc3270 %s Session Wizard %s",
	    wversion, ctime(&t));

    if (strcmp(session->host, CHOICE_NONE)) {
	bracket = (strchr(session->host, ':') != NULL);
	fprintf(f, "wc3270.%s: ", ResHostname);
	if (session->ssl) {
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
	fprintf(f, "wc3270.%s: %s:%s%s%s%s%s\n",
		ResProxy,
		session->proxy_type,
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
    fprintf(f, "wc3270.%s: %s\n", ResCharset, session->charset);
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

    if (session->flags & WF_VERIFY_HOST_CERTS) {
	fprintf(f, "wc3270.%s: %s\n", ResVerifyHostCert, ResTrue);
    }

    if (session->flags & WF_NO_MENUBAR) {
	fprintf(f, "wc3270.%s: %s\n", ResMenuBar, ResFalse);
    }

    if (session->flags & WF_TRACE) {
	    fprintf(f, "wc3270.%s: %s\n", ResTrace, ResTrue);
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

/**
 * Usage message. Display syntax and exit.
 */
static void
w_usage(void)
{
    fprintf(stderr, "Usage: wc3270wiz [session-name]\n"
	    "       wc3270wiz [session-file]\n");
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
    char *program = argv[0];
    int explicit_edit = FALSE;
    DWORD name_size;
    char result[STR_SIZE];

    /*
     * Parse command-line arguments.
     * For now, there is only one -- the optional name of the session.
     */
    program = argv[0];
    if (argc > 1 && !strcmp(argv[1], "-e")) {
	explicit_edit = TRUE;
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

    /* Figure out the version. */
    if (get_version_info() < 0) {
	return 1;
    }

    /* Get some paths from Windows. */
    if (get_dirs(program, "wc3270", &installdir, &desktop, &mya,
		&common_desktop, &commona, &installed) < 0) {
	return 1;
    }
    name_size = sizeof(username) / sizeof(TCHAR);
    if (GetUserName(username, &name_size) == 0) {
	fprintf(stderr, "GetUserName failed, error %ld\n",
		(long)GetLastError());
	return 1;
    }

    /* Resize the console window. */
    resize_window(44);

    signal(SIGINT, SIG_IGN);

    save_keymaps();

    /* Display the main menu until they quit or something goes wrong. */
    result[0] = '\0';
    do {
	rc = session_wizard(session_name, explicit_edit, result,
		sizeof(result));
	if (session_name != NULL) {
		break;
	}
    } while (rc == SW_SUCCESS);

    /*
     * Wait for Enter before exiting, so the console window does not
     * disappear without the user seeing what it did.
     */
    if (rc != SW_QUIT) {
	printf("\nWizard %s. ", (rc == SW_ERR)? "aborted": "complete");
	ask_enter();
    }

    return 0;
}
