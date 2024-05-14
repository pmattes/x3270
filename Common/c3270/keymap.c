/*
 * Copyright (c) 2000-2024 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	keymap.c
 *		A curses-based 3270 Terminal Emulator
 *		Keyboard mapping
 */

#include "globals.h"
#include <errno.h>
#include "appres.h"
#include "resources.h"

#include "actions.h"
#include "glue.h"
#include "host.h"
#include "keymap.h"
#include "names.h"
#include "popups.h"
#include "screen.h"
#include "status.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "unicodec.h"
#include "utils.h"
#include "varbuf.h"
#include "vstatus.h"

#if defined(HAVE_NCURSESW_NCURSES_H) /*[*/
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H) /*][*/
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H) /*][*/
#include <ncurses.h>
#else /*][*/
#include <curses.h>
#endif /*]*/

#define KM_3270_ONLY	0x0010	/* used in 3270 mode only */
#define KM_NVT_ONLY	0x0020	/* used in NVT mode only */
#define KM_INACTIVE	0x0040	/* wrong NVT/3270 mode, or overridden */

#define KM_KEYMAP	0x8000
#define KM_HINTS	(KM_CTRL|KM_ALT)

typedef struct {
    int key;	/* KEY_XXX or 0 */
    int modifiers;	/* KM_ALT */
    ucs4_t ucs4;	/* character value */
} k_t;

struct keymap {
    struct keymap *next;
    struct keymap *successor;
    int ncodes;		/* number of key codes */
    k_t *codes;		/* key codes */
    int *hints;		/* hints (flags) */
    char *name;		/* keymap name */
    char *file;		/* file path or resource name */
    int line;		/* keymap line number */
    bool temp;		/* temporary keymap? */
    char *action;	/* actions to perform */
};

#define IS_INACTIVE(k)	((k)->hints[0] & KM_INACTIVE)

static struct keymap *master_keymap = NULL;

static bool last_3270 = false;
static bool last_nvt = false;

static int lookup_ccode(const char *s);
static void keymap_3270_mode(bool);

#define codecmp(k1, k2, len)	\
	kvcmp((k1)->codes, (k2)->codes, len)

static void read_one_keymap(const char *name, const char *fn, bool temp,
	const char *r0, int flags);
static void clear_keymap(void);
static void set_inactive(void);

/*
 * Compare two k_t's.
 * Returns 0 if equal, nonzero if not.
 */
static int
kcmp(k_t *a, k_t *b)
{
    if (a->key && b->key && (a->key == b->key)) {
	return 0;
    }
    if (a->ucs4 && b->ucs4 &&
	(a->ucs4 == b->ucs4) &&
	(a->modifiers == b->modifiers)) {
	return 0;
    }

    /* Special case for both a and b empty. */
    if (!a->key && !b->key && !a->ucs4 && !b->ucs4) {
	return 0;
    }

    return 1;
}

/*
 * Compare a vector of k_t's.
 */
static int
kvcmp(k_t *a, k_t *b, int len)
{
    	int i;

	for (i = 0; i < len; i++) {
	    	if (kcmp(&a[i], &b[i]))
			return 1;
	}
	return 0;
}

/*
 * Parse a key definition.
 * Returns <0 for error, 1 for key found and parsed, 0 for nothing found.
 * Returns the balance of the string and the character code.
 * Is destructive.
 */
static int
parse_keydef(char **str, k_t *ccode, int *hint)
{
    char *s = *str;
    char *t;
    char *ks;
    int flags = 0;
    ks_t Ks;
    bool matched = false;

    ccode->key = 0;
    ccode->ucs4 = 0;
    ccode->modifiers = 0;

    /* Check for nothing. */
    while (isspace((unsigned char)*s)) {
	s++;
    }
    if (!*s) {
	return 0;
    }
    *str = s;

    s = strstr(s, "<Key>");
    if (s == NULL) {
	return -1;
    }
    ks = s + 5;
    *s = '\0';
    s = *str;
    while (*s) {
	while (isspace((unsigned char)*s)) {
	    s++;
	}
	if (!*s) {
	    break;
	}
	if (!strncmp(s, "Alt", 3)) {
	    ccode->modifiers |= KM_ALT;
	    s += 3;
	} else if (!strncmp(s, "Ctrl", 4)) {
	    flags |= KM_CTRL;
	    s += 4;
	} else {
	    return -2;
	}
    }
    s = ks;
    while (isspace((unsigned char)*s)) {
	s++;
    }
    if (!*s) {
	return -3;
    }

    t = s;
    while (*t && !isspace((unsigned char)*t)) {
	t++;
    }
    if (*t) {
	*t++ = '\0';
    }

    if (!strncasecmp(s, "U+", 2) || !strncasecmp(s, "0x", 2)) {
	unsigned long u;
	char *ptr;

	/* Direct specification of Unicode. */
	u = strtoul(s + 2, &ptr, 16);
	if (u == 0 || *ptr != '\0') {
	    return -7;
	}
	ccode->ucs4 = (ucs4_t)u;
	matched = true;
    }
    if (!matched) {
	ucs4_t u;
	int consumed;
	enum me_fail error;

	/*
	 * Convert local multibyte to Unicode.  If the result is 1
	 * character in length, use that code.
	 */
	u = multibyte_to_unicode(s, strlen(s), &consumed, &error);
	if (u != 0 && (size_t)consumed == strlen(s)) {
	    ccode->ucs4 = u;
	    matched = true;
	}
    }
    if (!matched) {
	/* Try an HTML entity name or X11 keysym. */
	Ks = string_to_key(s);
	if (Ks != KS_NONE) {
	    ccode->ucs4 = Ks;
	    matched = true;
	}
    }
    if (!matched) {
	int cc;

	/* Try for a curses key name. */
	cc = lookup_ccode(s);
	if (cc == -1) {
	    return -4;
	}
	if (flags || ccode->modifiers) {
	    return -5; /* no Alt/Ctrl with KEY_XXX */
	}
	ccode->key = cc;
	matched = true;
    }

    /* Apply Ctrl. */
    if (ccode->ucs4) {
	if (flags & KM_CTRL) {
	    if (ccode->ucs4 > 0x20 && ccode->ucs4 < 0x80) {
		ccode->ucs4 &= 0x1f;
	    } else {
		return -6; /* Ctrl ASCII-7 only */
	    }
	}
    }

    /* Return the remaining string, and success. */
    *str = t;
    *hint = flags;
    return 1;
}

static char *pk_errmsg[] = {
    "Missing <Key>",
    "Unknown modifier",
    "Missing keysym",
    "Unknown keysym",
    "Can't use Ctrl or Alt modifier with curses symbol",
    "Ctrl modifier is restricted to ASCII-7 printable characters",
    "Invalid Unicode syntax"
};

/*
 * Locate a keymap resource or file.
 * Returns 0 for do-nothing, 1 for success, -1 for error.
 * On success, returns the full name of the resource or file (which must be
 *  freed) in '*fullname'.
 * On success, returns a resource string (which must be closed) or NULL
 *  (indicating a file name to open is in *fullname) in '*r'.
 */
static int
locate_keymap(const char *name, char **fullname, char **r)
{
    char *rs;			/* resource value */
    char *fnx;			/* expanded file name */
    int a;			/* access(fnx) */

    /* Return nothing, to begin with. */
    *fullname = NULL;
    *r = NULL;

    /* See if it's a resource. */
    rs = get_fresource(ResKeymap ".%s", name);

    /* If there's a plain version, return it. */
    if (rs != NULL) {
	*fullname = NewString(name);
	*r = NewString(rs);
	return 1;
    }

    /* See if it's a file. */
    fnx = do_subst(name, DS_VARS | DS_TILDE);
    a = access(fnx, R_OK);

    /* If there's a plain version, return it. */
    if (a == 0) {
	*fullname = fnx;
	return 1;
    }

    /* No dice. */
    Free(fnx);
    return -1;
}

/* Add a keymap entry. */
static void
add_keymap_entry(int ncodes, k_t *codes, int *hints, const char *name,
	const char *file, int line, bool temp, const char *action,
	struct keymap ***nextkp)
{
    struct keymap *k;

    /* Allocate a new node. */
    k = Malloc(sizeof(struct keymap));
    k->next = NULL;
    k->successor = NULL;
    k->ncodes = ncodes;
    k->codes = Malloc(ncodes * sizeof(k_t));
    memcpy(k->codes, codes, ncodes * sizeof(k_t));
    k->hints = Malloc(ncodes * sizeof(int));
    memcpy(k->hints, hints, ncodes * sizeof(int));
    k->name = NewString(name);
    k->file = NewString(file);
    k->line = line;
    k->temp = temp;
    k->action = NewString(action);

    /* Link it in. */
    **nextkp = k;
    *nextkp = &k->next;
}

/*
 * Read a keymap from a file.
 * Returns true for success, false for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static bool
read_keymap(const char *name, bool temp)
{
    char *name_3270 = Asprintf("%s.3270", name);
    char *name_nvt = Asprintf("%s.nvt", name);
    int rc, rc_3270, rc_nvt;
    char *fn, *fn_3270, *fn_nvt;
    char *r0, *r0_3270, *r0_nvt;

    if (master_keymap != NULL && !strcmp(name, master_keymap->name)) {
	popup_an_error("Duplicate keymap: %s", name);
	return false;
    }

    rc = locate_keymap(name, &fn, &r0);
    rc_3270 = locate_keymap(name_3270, &fn_3270, &r0_3270);
    rc_nvt = locate_keymap(name_nvt, &fn_nvt, &r0_nvt);
    if (rc < 0 && rc_3270 < 0 && rc_nvt < 0) {
	popup_an_error("No such keymap resource or file: %s", name);
	Free(name_3270);
	Free(name_nvt);
	return false;
    }

    if (rc >= 0) {
	read_one_keymap(name, fn, temp, r0, 0);
	Free(fn);
	Free(r0);
    }
    if (rc_3270 >= 0) {
	read_one_keymap(name_3270, fn_3270, temp, r0_3270, KM_3270_ONLY);
    Free(fn_3270);
	    Free(r0_3270);
    }
    if (rc_nvt >= 0) {
	read_one_keymap(name_nvt, fn_nvt, temp, r0_nvt, KM_NVT_ONLY);
	Free(fn_nvt);
	Free(r0_nvt);
    }
    Free(name_3270);
    Free(name_nvt);

    return true;
}

/*
 * Read a keymap from a file.
 * Accumulates the keymap onto the list pointed to by nextkp.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static void
read_one_keymap_internal(const char *name, const char *fn, bool temp,
	const char *r0, int flags, struct keymap ***nextkp)
{
    char *r = NULL;		/* resource value */
    char *r_copy = NULL;	/* initial value of r */
    FILE *f = NULL;		/* resource file */
    char buf[1024];		/* file read buffer */
    int line = 0;		/* line number */
    char *left, *right;		/* chunks of line */
    static int ncodes = 0;
    static int maxcodes = 0;
    static k_t *codes = NULL;
    static int *hints = NULL;
    int rc = 0;
    char *action_error;

    /* Find the resource or file. */
    if (r0 != NULL) {
	r = r_copy = NewString(r0);
    } else {
	f = fopen(fn, "r");
	if (f == NULL) {
	    xs_warning("Cannot open file: %s", fn);
	    return;
	}
    }

    while ((r != NULL)? (rc = split_dresource(&r, &left, &right)):
			fgets(buf, sizeof(buf), f) != NULL) {
	char *s;
	k_t ccode;
	int pkr;
	int hint;

	line++;

	/* Skip empty lines and comments. */
	if (r == NULL) {
	    s = buf;
	    while (isspace((unsigned char)*s)) {
		s++;
	    }
	    if (!*s || *s == '!' || *s == '#') {
		continue;
	    }
	}

	/* Split. */
	if (rc < 0 ||
	    (r == NULL && split_dresource(&s, &left, &right) < 0)) {
	    popup_an_error("Keymap %s, line %d: syntax error", fn, line);
	    goto done;
	}
	if (!validate_command(right, (int)(right - left), &action_error)) {
	    popup_an_error("Keymap %s, line %d: error:\n%s", fn, line,
		    action_error);
	    Free(action_error);
	    goto done;
	}

	pkr = parse_keydef(&left, &ccode, &hint);
	if (pkr == 0) {
	    popup_an_error("Keymap %s, line %d: Missing <Key>", fn, line);
	    goto done;
	}
	if (pkr < 0) {
	    popup_an_error("Keymap %s, line %d: %s", fn, line,
		    pk_errmsg[-1 - pkr]);
	    goto done;
	}

	/* Accumulate keycodes. */
	ncodes = 0;
	do {
	    if (++ncodes > maxcodes) {
		maxcodes = ncodes;
		codes = Realloc(codes, maxcodes * sizeof(k_t));
		hints = Realloc(hints, maxcodes * sizeof(int));
	    }
	    codes[ncodes - 1] = ccode; /* struct copy */
	    hints[ncodes - 1] = hint;
	    pkr = parse_keydef(&left, &ccode, &hint);
	    if (pkr < 0) {
		popup_an_error("Keymap %s, line %d: %s", fn, line,
			pk_errmsg[-1 - pkr]);
		goto done;
	    }
	} while (pkr != 0);

	/* Add it to the list. */
	hints[0] |= flags;
	add_keymap_entry(ncodes, codes, hints, name, fn, line, temp, right,
		nextkp);
    }

done:
    Free(r_copy);
    if (f != NULL) {
	fclose(f);
    }
}

/*
 * Read a keymap from a file.
 * Adds the keymap to the front of the 'master_keymap' list.
 * Returns 0 for success, -1 for an error.
 */
static void
read_one_keymap(const char *name, const char *fn, bool temp, const char *r0,
	int flags)
{
    struct keymap *one_master;
    struct keymap **one_nextk;

    /* Read in the keymap. */
    one_master = NULL;
    one_nextk = &one_master;
    read_one_keymap_internal(name, fn, temp, r0, flags, &one_nextk);

    if (one_master == NULL) {
	/* Nothing added. */
	return;
    }
    if (master_keymap == NULL) {
	/* Something added, nothing there before. */
	master_keymap = one_master;
	return;
    }

    /* Insert this keymap ahead of the previous ones. */
    *one_nextk = master_keymap;
    master_keymap = one_master;
}

/* Multi-key keymap support. */
static struct keymap *current_match = NULL;
static int consumed = 0;
static char *ignore = "[ignore]";

/* Find the shortest keymap with a longer match than k. */
static struct keymap *
longer_match(struct keymap *k, int nc)
{
    struct keymap *j;
    struct keymap *shortest = NULL;

    for (j = master_keymap; j != NULL; j = j->next) {
	if (IS_INACTIVE(j)) {
	    continue;
	}
	if (j != k && j->ncodes > nc && !codecmp(j, k, nc)) {
	    if (j->ncodes == nc+1) {
		return j;
	    }
	    if (shortest == NULL || j->ncodes < shortest->ncodes) {
		shortest = j;
	    }
	}
    }
    return shortest;
}

/*
 * Helper function that returns a keymap action, sets the status line, and
 * traces the result.  
 *
 * If s is NULL, then this is a failed initial lookup.
 * If s is 'ignore', then this is a lookup in progress (k non-NULL) or a
 *  failed multi-key lookup (k NULL).
 * Otherwise, this is a successful lookup.
 */
static char *
status_ret(char *s, struct keymap *k)
{
    /* Set the compose indicator based on the new value of current_match. */
    if (k != NULL) {
	vstatus_compose(true, ' ', KT_STD);
    } else {
	vstatus_compose(false, 0, KT_STD);
    }

    if (s != NULL && s != ignore) {
	vtrace(" %s:%d -> %s\n", current_match->file, current_match->line, s);
    }
    if ((current_match = k) == NULL) {
	consumed = 0;
    }
    return s;
}

/* Timeout for ambiguous keymaps. */
static struct keymap *timeout_match = NULL;
static ioid_t kto = NULL_IOID;

static void
key_timeout(ioid_t id _is_unused)
{
    vtrace("Timeout, using shortest keymap match\n");
    kto = NULL_IOID;
    current_match = timeout_match;
    push_keymap_action(status_ret(timeout_match->action, NULL));
    timeout_match = NULL;
}

static struct keymap *
ambiguous(struct keymap *k, int nc)
{
    struct keymap *j;

    if ((j = longer_match(k, nc)) != NULL) {
	vtrace(" ambiguous keymap match, shortest is %s:%d, setting timeout\n",
		j->file, j->line);
	timeout_match = k;
	kto = AddTimeOut(500L, key_timeout);
    }
    return j;
}

/*
 * Look up an key in the keymap, return the matching action if there is one.
 *
 * This code implements the mutli-key lookup, by returning dummy actions for
 * partial matches.
 *
 * It also handles keyboards that generate ESC for the Alt key.
 */
char *
lookup_key(int kcode, ucs4_t ucs4, int modifiers)
{
    struct keymap *j, *k;
    int n_shortest = 0;
    k_t code;

    code.key = kcode;
    code.ucs4 = ucs4;
    code.modifiers = modifiers;

    /* If there's a timeout pending, cancel it. */
    if (kto != NULL_IOID) {
	RemoveTimeOut(kto);
	kto = NULL_IOID;
	timeout_match = NULL;
    }

    /* If there's no match pending, find the shortest one. */
    if (current_match == NULL) {
	struct keymap *shortest = NULL;

	for (k = master_keymap; k != NULL; k = k->next) {
	    if (IS_INACTIVE(k))
		continue;
	    if (!kcmp(&code, &k->codes[0])) {
		if (k->ncodes == 1) {
		    shortest = k;
		    break;
		}
		if (shortest == NULL || k->ncodes < shortest->ncodes) {
		    shortest = k;
		    n_shortest++;
		}
	    }
	}
	if (shortest != NULL) {
	    current_match = shortest;
	    consumed = 0;
	} else {
	    return NULL;
	}
    }

    /* See if this character matches the next one we want. */
    if (!kcmp(&code, &current_match->codes[consumed])) {
	consumed++;
	if (consumed == current_match->ncodes) {
	    /* Final match. */
	    j = ambiguous(current_match, consumed);
	    if (j == NULL) {
		return status_ret(current_match->action, NULL);
	    } else {
		return status_ret(ignore, j);
	    }
	} else {
	    /* Keep looking. */
	    vtrace(" partial keymap match in %s:%d %s\n",
		    current_match->file, current_match->line,
		    (n_shortest > 1)? " and other(s)": "");
	    return status_ret(ignore, current_match);
	}
    }

    /* It doesn't.  Try for a better candidate. */
    for (k = master_keymap; k != NULL; k = k->next) {
	if (IS_INACTIVE(k)) {
	    continue;
	}
	if (k == current_match) {
	    continue;
	}
	if (k->ncodes > consumed && !codecmp(k, current_match, consumed) &&
		!kcmp(&k->codes[consumed], &code)) {
	    consumed++;
	    if (k->ncodes == consumed) {
		j = ambiguous(k, consumed);
		if (j == NULL) {
		    current_match = k;
		    return status_ret(k->action, NULL);
		} else {
		    return status_ret(ignore, j);
		}
	    } else {
		return status_ret(ignore, k);
	    }
	}
    }

    /* Complain. */
    beep();
    vtrace(" keymap lookup failure after partial match\n");
    return status_ret(ignore, NULL);
}

static struct {
    const char *name;
    int code;
} ncurses_key[] = {
    { "BREAK",		KEY_BREAK },
    { "DOWN",		KEY_DOWN },
    { "UP",		KEY_UP },
    { "LEFT",		KEY_LEFT },
    { "RIGHT",		KEY_RIGHT },
    { "HOME",		KEY_HOME },
    { "BACKSPACE",	KEY_BACKSPACE },
    { "F0",		KEY_F0 },
    { "DL",		KEY_DL },
    { "IL",		KEY_IL },
    { "DC",		KEY_DC },
    { "IC",		KEY_IC },
    { "EIC",		KEY_EIC },
    { "CLEAR",		KEY_CLEAR },
    { "EOS",		KEY_EOS },
    { "EOL",		KEY_EOL },
    { "SF",		KEY_SF },
    { "SR",		KEY_SR },
    { "NPAGE",		KEY_NPAGE },
    { "PPAGE",		KEY_PPAGE },
    { "STAB",		KEY_STAB },
    { "CTAB",		KEY_CTAB },
    { "CATAB",		KEY_CATAB },
    { "ENTER",		KEY_ENTER },
    { "SRESET",		KEY_SRESET },
    { "RESET",		KEY_RESET },
    { "PRINT",		KEY_PRINT },
    { "LL",		KEY_LL },
    { "A1",		KEY_A1 },
    { "A3",		KEY_A3 },
    { "B2",		KEY_B2 },
    { "C1",		KEY_C1 },
    { "C3",		KEY_C3 },
    { "BTAB",		KEY_BTAB },
    { "BEG",		KEY_BEG },
    { "CANCEL",		KEY_CANCEL },
    { "CLOSE",		KEY_CLOSE },
    { "COMMAND",	KEY_COMMAND },
    { "COPY",		KEY_COPY },
    { "CREATE",		KEY_CREATE },
    { "END",		KEY_END },
    { "EXIT",		KEY_EXIT },
    { "FIND",		KEY_FIND },
    { "HELP",		KEY_HELP },
    { "MARK",		KEY_MARK },
    { "MESSAGE",	KEY_MESSAGE },
    { "MOVE",		KEY_MOVE },
    { "NEXT",		KEY_NEXT },
    { "OPEN",		KEY_OPEN },
    { "OPTIONS",	KEY_OPTIONS },
    { "PREVIOUS",	KEY_PREVIOUS },
    { "REDO",		KEY_REDO },
    { "REFERENCE",	KEY_REFERENCE },
    { "REFRESH",	KEY_REFRESH },
    { "REPLACE",	KEY_REPLACE },
    { "RESTART",	KEY_RESTART },
    { "RESUME",		KEY_RESUME },
    { "SAVE",		KEY_SAVE },
    { "SBEG",		KEY_SBEG },
    { "SCANCEL",	KEY_SCANCEL },
    { "SCOMMAND",	KEY_SCOMMAND },
    { "SCOPY",		KEY_SCOPY },
    { "SCREATE",	KEY_SCREATE },
    { "SDC",		KEY_SDC },
    { "SDL",		KEY_SDL },
    { "SELECT",		KEY_SELECT },
    { "SEND",		KEY_SEND },
    { "SEOL",		KEY_SEOL },
    { "SEXIT",		KEY_SEXIT },
    { "SFIND",		KEY_SFIND },
    { "SHELP",		KEY_SHELP },
    { "SHOME",		KEY_SHOME },
    { "SIC",		KEY_SIC },
    { "SLEFT",		KEY_SLEFT },
    { "SMESSAGE",	KEY_SMESSAGE },
    { "SMOVE",		KEY_SMOVE },
    { "SNEXT",		KEY_SNEXT },
    { "SOPTIONS",	KEY_SOPTIONS },
    { "SPREVIOUS",	KEY_SPREVIOUS },
    { "SPRINT",		KEY_SPRINT },
    { "SREDO",		KEY_SREDO },
    { "SREPLACE",	KEY_SREPLACE },
    { "SRIGHT",		KEY_SRIGHT },
    { "SRSUME",		KEY_SRSUME },
    { "SSAVE",		KEY_SSAVE },
    { "SSUSPEND",	KEY_SSUSPEND },
    { "SUNDO",		KEY_SUNDO },
    { "SUSPEND",	KEY_SUSPEND },
    { "UNDO",		KEY_UNDO },
    { NULL, 0 }
};

/* Look up a curses symbolic key. */
static int
lookup_ccode(const char *s)
{
    int i;
    unsigned long f;
    char *ptr;

    for (i = 0; ncurses_key[i].name != NULL; i++) {
	if (!strcasecmp(s, ncurses_key[i].name)) {
	    return ncurses_key[i].code;
	}
    }
    if (s[0] == 'F' &&
	(f = strtoul(s + 1, &ptr, 10)) < 64 &&
	ptr != s + 1 &&
	*ptr == '\0') {

	return KEY_F(f);
    }
    return -1;
}

/* Look up a curses key code. */
static const char *
lookup_cname(int ccode)
{
    int i;

    for (i = 0; ncurses_key[i].name != NULL; i++) {
	if (ccode == ncurses_key[i].code) {
	    return ncurses_key[i].name;
	}
    }
    for (i = 0; i < 64; i++) {
	if (ccode == KEY_F(i)) {
	    static char buf[10];

	    sprintf(buf, "F%d", i);
	    return buf;
	}
    }

    return NULL;
}

/**
 * Free a temporary keymap entry.
 */
static void
free_keymap(struct keymap *k)
{
    Free(k->codes);
    Free(k->hints);
    Free(k->name);
    Free(k->file);
    Free(k->action);
    Free(k);
}

/**
 * Push or pop a temporary keymap.
 */
static bool
Keymap_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug(AnKeymap, ia, argc, argv);
    if (check_argc(AnKeymap, argc, 0, 1) < 0) {
	return false;
    }

    if (argc > 0) {
	/* Push this keymap. */
	if (!read_keymap(argv[0], true)) {
	    return false;
	}

	/* Set the inactive flags. */
	set_inactive();
    } else {
	struct keymap *k;
	char *km_name = NULL;

	if (master_keymap == NULL || !master_keymap->temp) {
	    return true;
	}
	km_name = NewString(master_keymap->name);

	/* Pop the top keymap. */
	while ((k = master_keymap) != NULL) {
	    if (!k->temp || strcmp(k->name, km_name)) {
		break;
	    }
	    master_keymap = k->next;
	    free_keymap(k);
	}
	Free(km_name);

	/* Set the inactive flags. */
	set_inactive();
    }

    return true;
}

/**
 * Keymap module registrations.
 */
void
keymap_register(void)
{
    static action_table_t keymap_actions[] = {
	{ AnKeymap,		Keymap_action, ACTION_KE },
	{ AnTemporaryKeymap,	Keymap_action, ACTION_KE }
    };

    /* Register for state changes. */
    register_schange(ST_3270_MODE, keymap_3270_mode);
    register_schange(ST_CONNECT, keymap_3270_mode);

    /* Register the actions. */
    register_actions(keymap_actions, array_count(keymap_actions));
}

/* Read each of the keymaps specified by the keymap resource. */
void
keymap_init(void)
{
    char *s0, *s;
    char *comma;

    /* In case this is a subsequent call, wipe out the current keymap. */
    clear_keymap();

    /* Read the base keymap. */
    read_keymap("base", false);

    /* Read the user-defined keymaps. */
    if (appres.interactive.key_map != NULL) {
	s = s0 = NewString(appres.interactive.key_map);
	while ((comma = strchr(s, ',')) != NULL) {
	    *comma = '\0';
	    if (*s) {
		read_keymap(s, false);
	    }
	    s = comma + 1;
	}
	if (*s) {
	    read_keymap(s, false);
	}
	Free(s0);
    }

    last_3270 = IN_3270;
    last_nvt = IN_NVT;
    set_inactive();
}

/* Erase the current keymap. */
static void
clear_keymap(void)
{
    struct keymap *k, *next;

    for (k = master_keymap; k != NULL; k = next) {
	next = k->next;
	free_keymap(k);
    }
    master_keymap = NULL;
}

/* Set the inactive flags for the current keymap. */
static void
set_inactive(void)
{
    struct keymap *k, *j;

    /* Clear the inactive flags and successors. */
    for (k = master_keymap; k != NULL; k = k->next) {
	k->hints[0] &= ~KM_INACTIVE;
	k->successor = NULL;
    }

    /* Turn off elements which have the wrong mode. */
    for (k = master_keymap; k != NULL; k = k->next) {
	/* If the mode is wrong, turn it off. */
	if ((!last_3270 && (k->hints[0] & KM_3270_ONLY)) ||
		(!last_nvt  && (k->hints[0] & KM_NVT_ONLY))) {
	    k->hints[0] |= KM_INACTIVE;
	    }
    }

    /* Compute superseded entries. */
    for (k = master_keymap; k != NULL; k = k->next) {
	if (k->hints[0] & KM_INACTIVE) {
	    continue;
	}
	for (j = k->next; j != NULL; j = j->next) {
	    if (j->hints[0] & KM_INACTIVE) {
		continue;
	    }
	    /* It may supercede other entries. */
	    if (j->ncodes == k->ncodes && !codecmp(j, k, k->ncodes)) {
		j->hints[0] |= KM_INACTIVE;
		j->successor = k;
	    }
	}
    }
}

/* 3270/NVT mode change. */
static void
keymap_3270_mode(bool ignored _is_unused)
{
    if (last_3270 != IN_3270 || last_nvt != IN_NVT) {
	last_3270 = IN_3270;
	last_nvt = IN_NVT;
	set_inactive();
    }
}

/*
 * Decode a key.
 * Accepts a hint as to which form was used to specify it, if it came from a
 * keymap definition.
 */
const char *
decode_key(int k, ucs4_t ucs4, int hint, char *buf)
{
    const char *n;
    int len;
    char mb[16];
    char *s = buf;

    if (k) {
	/* Curses key. */
	if ((n = lookup_cname(k)) != NULL) {
	    sprintf(buf, "<Key>%s", n);
	} else {
	    sprintf(buf, "[unknown curses key 0x%x]", k);
	}
	return buf;
    }

    if (hint & KM_ALT) {
	s += sprintf(s, "Alt");
    }

    if (ucs4 < ' ') {
	/* Control key. */
	char *latin1_name = key_to_string(ucs4);

	if (latin1_name != NULL) {
	    strcpy(buf, latin1_name);
	} else {
	    sprintf(s, "Ctrl<Key>%c", (int)(ucs4 + '@') & 0xff);
	}
	return buf;
    }

    /* Special-case ':' and ' ' because of the keymap syntax. */
    if (ucs4 == ':') {
	strcpy(s, "colon");
	return buf;
    }
    if (ucs4 == ' ') {
	strcpy(s, "space");
	return buf;
    }

    /* Convert from Unicode to local multi-byte. */
    len = unicode_to_multibyte(ucs4, mb, sizeof(mb));
    if (len > 0) {
	sprintf(s, "<Key>%s", mb);
    } else {
	sprintf(s, "<Key>U+%04x", k);
    }
    return buf;
}

/* Dump the current keymap. */
const char *
keymap_dump(void)
{
    varbuf_t r;
    struct keymap *k;
    char *s;
    size_t sl;

    vb_init(&r);

    for (k = master_keymap; k != NULL; k = k->next) {
	if (k->successor != NULL) {
	    vb_appendf(&r, "[%s:%d%s] -- superseded by %s:%d --\n",
		    k->file, k->line,
		    k->temp? " temp": "",
		    k->successor->file, k->successor->line);
	} else if (!IS_INACTIVE(k)) {
	    int i;
	    char buf[1024];
	    char *s = buf;
	    char dbuf[128];
	    char *t = safe_string(k->action);

	    for (i = 0; i < k->ncodes; i++) {
		s += sprintf(s, " %s", decode_key(k->codes[i].key,
			    k->codes[i].ucs4,
			    (k->hints[i] & KM_HINTS) |
				KM_KEYMAP | k->codes[i].modifiers,
			    dbuf));
	    }
	    vb_appendf(&r, "[%s:%d%s]%s: %s\n", k->file, k->line,
		    k->temp? " temp": "", buf, t);
	    Free(t);
	}
    }

    s = vb_consume(&r);
    sl = strlen(s);
    if (sl > 0 && s[sl - 1] == '\n') {
	s[sl - 1] = '\0';
    }

    return txdFree(s);
}
