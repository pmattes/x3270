/*
 * Copyright 2000-2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	keymap.c
 *		A Windows Console-based 3270 Terminal Emulator
 *		Keyboard mapping
 */

#include "globals.h"
#include <errno.h>
#include "appres.h"
#include "resources.h"

#include "hostc.h"
#include "keymapc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "statusc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include <windows.h>

#define ISREALLYSPACE(c) ((((c) & 0xff) <= ' ') && isspace(c))

#define WC3270KM_SUFFIX	"wc3270km"
#define SUFFIX_LEN	sizeof(WC3270KM_SUFFIX)

#define KM_3270_ONLY	0x0100	/* used in 3270 mode only */
#define KM_NVT_ONLY	0x0200	/* used in NVT mode only */
#define KM_INACTIVE	0x0400	/* wrong NVT/3270 mode, or overridden */

#define KM_KEYMAP	0x8000
#define KM_HINTS	(KM_SHIFT | KM_CTRL | KM_ALT)

struct keymap {
	struct keymap *next;		/* next element in the keymap */
	struct keymap *successor;	/* mapping that overrules this one */
	int ncodes;			/* number of key codes */
	int *codes;			/* key codes (ASCII or vkey symbols) */
	int *hints;			/* hints (modifiers and restrictions) */
	char *file;			/* file or resource name */
	int line;			/* keymap line number */
	char *action;			/* action(s) to perform */
};

#define IS_INACTIVE(k)	((k)->hints[0] & KM_INACTIVE)

static struct keymap *master_keymap = NULL;
static struct keymap **nextk = &master_keymap;

static Boolean last_3270 = False;
static Boolean last_nvt = False;

static int lookup_ccode(const char *s);
static void keymap_3270_mode(Boolean);

static void read_one_keymap(const char *name, const char *fn, const char *r0,
    int flags);
static void clear_keymap(void);
static void set_inactive(void);

/*
 * Parse a key definition.
 * Returns <0 for error, 1 for key found and parsed, 0 for nothing found.
 * Returns the balance of the string and the character code.
 * Is destructive.
 */

enum {
	PKE_MKEY = -1,
	PKE_UMOD = -2,
	PKE_MSYM = -3,
	PKE_USYM = -4
} pk_error;

static char *pk_errmsg[] = {
	"Missing <Key>",
	"Unknown modifier",
	"Missing keysym",
	"Unknown keysym"
};

static int
parse_keydef(char **str, int *ccode, int *hint)
{
	char *s = *str;
	char *t;
	char *ks;
	int flags = 0;
	KeySym Ks;
	int xccode;

	/* Check for nothing. */
	while (ISREALLYSPACE(*s))
		s++;
	if (!*s)
		return 0;
	*str = s;

	s = strstr(s, "<Key>");
	if (s == CN)
		return PKE_MKEY;
	ks = s + 5;
	*s = '\0';
	s = *str;
	while (*s) {
		while (ISREALLYSPACE(*s))
			s++;
		if (!*s)
			break;
		if (!strncasecmp(s, "Shift", 5)) {
			flags |= KM_SHIFT;
			s += 5;
		} else if (!strncasecmp(s, "Ctrl", 4)) {
			flags |= KM_CTRL;
			s += 4;
		} else if (!strncasecmp(s, "LeftCtrl", 8)) {
			flags |= KM_LCTRL;
			s += 8;
		} else if (!strncasecmp(s, "RightCtrl", 9)) {
			flags |= KM_RCTRL;
			s += 9;
		} else if (!strncasecmp(s, "Alt", 3)) {
			flags |= KM_ALT;
			s += 3;
		} else if (!strncasecmp(s, "LeftAlt", 7)) {
			flags |= KM_LALT;
			s += 7;
		} else if (!strncasecmp(s, "RightAlt", 8)) {
			flags |= KM_ALT;
			s += 8;
		} else
			return PKE_UMOD;
	}
	s = ks;
	while (ISREALLYSPACE(*s))
		s++;
	if (!*s)
		return PKE_MSYM;

	t = s;
	while (*t && !ISREALLYSPACE(*t))
		t++;
	if (*t)
		*t++ = '\0';
	xccode = lookup_ccode(s);
	if (xccode != -1) {
	    	*ccode = xccode;
	} else {
	    	if (!strncasecmp(s, "U+", 2) || !strncasecmp(s, "0x", 2)) {
		    	unsigned long l;
			char *ptr;

		    	/*
			 * Explicit Unicode.
			 * We limit ourselves to UCS-2 for now, becuase of how
			 * we represent keymaps and keys (VK_xxx in upper 16
			 * bits, Unicode in lower 16 bits).
			 */
			l = strtoul(s, &ptr, 16);
			if (!((l == 0) || (l & ~0xffff) || *ptr != '\0'))
			    	*ccode = (int)l;
			else
			    	return PKE_USYM;
		} else if (strlen(s) == 1) {
		    	int nc;
			WCHAR w;

		    	/* Single (ANSI CP) character. */
			nc = MultiByteToWideChar(CP_ACP, 0, s, 1, &w, 1);
			if (nc == 1)
			    	*ccode = (int)w;
			else
			    	return PKE_USYM;
		} else {
		    	/* Try for a Latin-1 name. */
		    	Ks = StringToKeysym(s);
			if (Ks != NoSymbol)
			    	*ccode = (int)Ks;
			else
			    	return PKE_USYM;
		}
	}

	/* Canonicalize Ctrl. */
	if ((flags & KM_CTRL) && *ccode >= '@' && *ccode <= '~') {
		*ccode &= 0x1f;
		flags &= ~KM_CTRL;
	}

	/* Return the remaining string, and success. */
	*str = t;
	*hint = flags;
	return 1;
}

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
	char *fny;
	char *fnp;
	int a;

	/* Return nothing, to begin with. */
	*fullname = CN;
	*r = CN;

	/* See if it's a resource. */
	rs = get_fresource(ResKeymap ".%s", name);

	/* If there's a plain version, return it. */
	if (rs != CN) {
		*fullname = NewString(name);
		*r = NewString(rs);
		return 1;
	}

	/* See if it's a file. */
	fnx = do_subst(name, True, True);
	fny = xs_buffer("%s.%s", fnx, WC3270KM_SUFFIX);
	Free(fnx);
	fnx = CN;

	/*
	 * Try the application data directory first, then (for compatiblity
	 * with older releases) the install directory.
	 */
	fnp = xs_buffer("%s%s", myappdata, fny);
	a = access(fnp, R_OK);
	Free(fnp);
	if (a == 0) {
	    	*fullname = fny;
		return 1;
	}

	fnp = xs_buffer("%s%s", instdir, fny);
	a = access(fny, R_OK);
	Free(fnp);
	if (a == 0) {
	    	*fullname = fny;
		return 1;
	}

	/* No dice. */
	Free(fny);
	return -1;
}

/*
 * Compare a pair of keymaps for compatablity (could k2 match k1).
 * N.B.: This functon may need to be further parameterized for equality versus
 * (ambiguous) matching.
 */
static int
codecmp(struct keymap *k1, struct keymap *k2, int len)
{
	int r;
	int i;

	/* Compare the raw codes first. */
	r = memcmp(k1->codes, k2->codes, len * sizeof(int));
	if (r)
		return r;

	/* The codes agree, now try the modifiers. */
	for (i = 0; i < len; i++) {
		if (k1->hints[i] & KM_HINTS) {
			if ((k1->hints[i] & KM_HINTS) !=
			    (k2->hints[i] & KM_HINTS))
				return -1;
		}
	}

	/* Same same. */
	return 0;

}

/* Add a keymap entry. */
static void
add_keymap_entry(int ncodes, int *codes, int *hints, const char *file,
    int line, const char *action)
{
	struct keymap *k;
	struct keymap *j;

	/* Allocate a new node. */
	k = Malloc(sizeof(struct keymap));
	k->next = NULL;
	k->successor = NULL;
	k->ncodes = ncodes;
	k->codes = Malloc(ncodes * sizeof(int));
	(void) memcpy(k->codes, codes, ncodes * sizeof(int));
	k->hints = Malloc(ncodes * sizeof(int));
	(void) memcpy(k->hints, hints, ncodes * sizeof(int));
	k->file = NewString(file);
	k->line = line;
	k->action = NewString(action);

	/* See if it's inactive, or supercedes other entries. */
	if ((!last_3270 && (k->hints[0] & KM_3270_ONLY)) ||
	    (!last_nvt  && (k->hints[0] & KM_NVT_ONLY))) {
		k->hints[0] |= KM_INACTIVE;
	} else for (j = master_keymap; j != NULL; j = j->next) {
		/* It may supercede other entries. */
		if (j->ncodes == k->ncodes &&
		    !codecmp(j, k, k->ncodes)) {
			j->hints[0] |= KM_INACTIVE;
			j->successor = k;
		}
	}

	/* Link it in. */
	*nextk = k;
	nextk = &k->next;
}

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static void
read_keymap(const char *name)
{
	char *name_3270 = xs_buffer("%s.3270", name);
	char *name_nvt = xs_buffer("%s.nvt", name);
	int rc, rc_3270, rc_nvt;
	char *fn, *fn_3270, *fn_nvt;
	char *r0, *r0_3270, *r0_nvt;

	rc = locate_keymap(name, &fn, &r0);
	rc_3270 = locate_keymap(name_3270, &fn_3270, &r0_3270);
	rc_nvt = locate_keymap(name_nvt, &fn_nvt, &r0_nvt);
	if (rc < 0 && rc_3270 < 0 && rc_nvt < 0) {
		xs_warning("No such keymap resource or file: %s",
		    name);
		Free(name_3270);
		Free(name_nvt);
		return;
	}

	if (rc >= 0) {
		read_one_keymap(name, fn, r0, 0);
		Free(fn);
		Free(r0);
	}
	if (rc_3270 >= 0) {
		read_one_keymap(name_3270, fn_3270, r0_3270, KM_3270_ONLY);
		Free(fn_3270);
		Free(r0_3270);
	}
	if (rc_nvt >= 0) {
		read_one_keymap(name_nvt, fn_nvt, r0_nvt, KM_NVT_ONLY);
		Free(fn_nvt);
		Free(r0_nvt);
	}
	Free(name_3270);
	Free(name_nvt);
}

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static void
read_one_keymap(const char *name, const char *fn, const char *r0, int flags)
{
	char *r = CN;			/* resource value */
	char *r_copy = CN;		/* initial value of r */
	FILE *f = NULL;			/* resource file */
	char buf[1024];			/* file read buffer */
	int line = 0;			/* line number */
	char *left, *right;		/* chunks of line */
	static int ncodes = 0;
	static int maxcodes = 0;
	static int *codes = NULL, *hints = NULL;
	int rc = 0;
	char *xfn = NULL;

	/* Find the resource or file. */
	if (r0 != CN) {
		r = r_copy = NewString(r0);
		xfn = (char *)fn;
	} else {
		char *path;
	    	int sl;

		/*
		 * Try the application data directory first, then (for
		 * compatiblity with older releases) the install directory.
		 */
		path = xs_buffer("%s%s", myappdata, fn);
		f = fopen(path, "r");
		if (f == NULL) {
		    	Free(path);
		    	path = xs_buffer("%s%s", instdir, fn);
			f = fopen(path, "r");
			if (f == NULL) {
				Free(path);
				xs_warning("Cannot open file: %s", fn);
				return;
			}
		}
		Free(path);
		sl = strlen(fn);
		if (sl > SUFFIX_LEN &&
		    !strcmp(fn + sl - SUFFIX_LEN, "." WC3270KM_SUFFIX)) {
		    	xfn = NewString(fn);
			xfn[sl - SUFFIX_LEN] = '\0';
		} else {
		    	xfn = (char *)fn;
		}
	}

	while ((r != CN)? (rc = split_dresource(&r, &left, &right)):
		          fgets(buf, sizeof(buf), f) != CN) {
		char *s;
		int ccode;
		int pkr;
		int hint;

		line++;

		/* Skip empty lines and comments. */
		if (r == CN) {
			s = buf;
			while (ISREALLYSPACE(*s))
				s++;
			if (!*s || *s == '!' || *s == '#')
				continue;
		}

		/* Split. */
		if (rc < 0 ||
		    (r == CN && split_dresource(&s, &left, &right) < 0)) {
			popup_an_error("%s, line %d: syntax error",
			    fn, line);
			goto done;
		}

		pkr = parse_keydef(&left, &ccode, &hint);
		if (pkr == 0) {
			popup_an_error("%s, line %d: Missing <Key>",
			    fn, line);
			goto done;
		}
		if (pkr < 0) {
			popup_an_error("%s, line %d: %s",
			    fn, line, pk_errmsg[-1 - pkr]);
			goto done;
		}

		/* Accumulate keycodes. */
		ncodes = 0;
		do {
			if (++ncodes > maxcodes) {
				maxcodes = ncodes;
				codes = Realloc(codes, maxcodes * sizeof(int));
				hints = Realloc(hints, maxcodes * sizeof(int));
			}
			codes[ncodes - 1] = ccode;
			hints[ncodes - 1] = hint;
			pkr = parse_keydef(&left, &ccode, &hint);
			if (pkr < 0) {
				popup_an_error("%s, line %d: %s",
				    fn, line, pk_errmsg[-1 - pkr]);
				goto done;
			}
		} while (pkr != 0);

		/* Add it to the list. */
		hints[0] |= flags;
		add_keymap_entry(ncodes, codes, hints, xfn, line, right);
	}

    done:
	Free(r_copy);
	if (f != NULL)
		fclose(f);
	if (xfn != fn)
	    Free(xfn);
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
		if (IS_INACTIVE(j))
			continue;
		if (j != k && j->ncodes > nc && !codecmp(j, k, nc)) {
			if (j->ncodes == nc+1)
				return j;
			if (shortest == NULL || j->ncodes < shortest->ncodes)
				shortest = j;
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
	if (k != NULL)
		status_compose(True, ' ', KT_STD);
	else
		status_compose(False, 0, KT_STD);

	if (s != NULL && s != ignore)
		trace_event(" %s:%d -> %s\n", current_match->file,
		    current_match->line, s);
	if ((current_match = k) == NULL)
		consumed = 0;
	return s;
}

/* Timeout for ambiguous keymaps. */
static struct keymap *timeout_match = NULL;
static unsigned long kto = 0L;

static void
key_timeout(void)
{
	trace_event("Timeout, using shortest keymap match\n");
	kto = 0L;
	current_match = timeout_match;
	push_keymap_action(status_ret(timeout_match->action, NULL));
	timeout_match = NULL;
}

static struct keymap *
ambiguous(struct keymap *k, int nc)
{
	struct keymap *j;

	if ((j = longer_match(k, nc)) != NULL) {
		trace_event(" ambiguous keymap match, shortest is %s:%d, "
		    "setting timeout\n", j->file, j->line);
		timeout_match = k;
		kto = AddTimeOut(500L, key_timeout);
	}
	return j;
}

/*
 * Check for compatability between a keymap and a key's modifier state.
 * Returns 1 for success, 0 for failure.
 */
static int
compatible_hint(int hint, int state)
{
	int h = hint & KM_HINTS;
	int s = state & KM_HINTS;

	if (!h)
		return 1;

	/*
	 * This used to be fairly straightforward, but it got murky when
	 * we split the left and right ctrl and alt keys.
	 *
	 * Basically, what we want is if both left and right Alt or Ctrl
	 * are set in 'hint', then either left or right Alt or Ctrl set in
	 * 'state' would be a match.  If only left or right is set in 'hint',
	 * then the match in 'state' has to be exact.
	 *
	 * We do this by checking for both being set in 'hint' and either
	 * being set in 'state'.  If this is the case, we set both in 'state'
	 * and try for an exact match.
	 */
	if ((h & KM_CTRL) == KM_CTRL) {
		if (s & KM_CTRL)
			s |= KM_CTRL;
	}
	if ((h & KM_ALT) == KM_ALT) {
		if (s & KM_ALT)
			s |= KM_ALT;
	}

	return (h & s) == h;
}

/*
 * Look up an key in the keymap, return the matching action if there is one.
 *
 * This code implements the mutli-key lookup, by returning dummy actions for
 * partial matches.
 */
char *
lookup_key(unsigned long code, unsigned long state)
{
	struct keymap *j, *k;
	int n_shortest = 0;
	int state_match = 0;

	/* trace_event("lookup_key(0x%08lx, 0x%lx)\n", code, state); */

	/* If there's a timeout pending, cancel it. */
	if (kto) {
		RemoveTimeOut(kto);
		kto = 0L;
		timeout_match = NULL;
	}

	/* Translate the Windows state to KM flags. */
	if (state & SHIFT_PRESSED)
		state_match |= KM_SHIFT;
	if (state & LEFT_ALT_PRESSED)
		state_match |= KM_LALT;
	if (state & RIGHT_ALT_PRESSED)
		state_match |= KM_RALT;
	if (state & LEFT_CTRL_PRESSED)
		state_match |= KM_LCTRL;
	if (state & RIGHT_CTRL_PRESSED)
		state_match |= KM_RCTRL;

	/* If there's no match pending, find the shortest one. */
	if (current_match == NULL) {
		struct keymap *shortest = NULL;

		for (k = master_keymap; k != NULL; k = k->next) {
			if (IS_INACTIVE(k))
				continue;
			if (code == k->codes[0] &&
			    compatible_hint(k->hints[0], state_match)) {
				if (k->ncodes == 1) {
					shortest = k;
					break;
				}
				if (shortest == NULL ||
				    k->ncodes < shortest->ncodes) {
					shortest = k;
					n_shortest++;
				}
			}
		}
		if (shortest != NULL) {
			current_match = shortest;
			consumed = 0;
		} else
			return NULL;
	}

	/* See if this character matches the next one we want. */
	if (code == current_match->codes[consumed] &&
            compatible_hint(current_match->hints[consumed], state_match)) {
		consumed++;
		if (consumed == current_match->ncodes) {
			/* Final match. */
			j = ambiguous(current_match, consumed);
			if (j == NULL)
				return status_ret(current_match->action, NULL);
			else
				return status_ret(ignore, j);
		} else {
			/* Keep looking. */
			trace_event(" partial keymap match in %s:%d %s\n",
			    current_match->file, current_match->line,
			    (n_shortest > 1)? " and other(s)": "");
			return status_ret(ignore, current_match);
		}
	}

	/* It doesn't.  Try for a better candidate. */
	for (k = master_keymap; k != NULL; k = k->next) {
		if (IS_INACTIVE(k))
			continue;
		if (k == current_match)
			continue;
		if (k->ncodes > consumed &&
		    !codecmp(k, current_match, consumed) &&
		    k->codes[consumed] == code &&
		    compatible_hint(k->hints[consumed], state_match)) {
			consumed++;
			if (k->ncodes == consumed) {
				j = ambiguous(k, consumed);
				if (j == NULL) {
					current_match = k;
					return status_ret(k->action,
					    NULL);
				} else
					return status_ret(ignore, j);
			} else
				return status_ret(ignore, k);
		}
	}

	/* Complain. */
	Beep(750, 150);
	trace_event(" keymap lookup failure after partial match\n");
	return status_ret(ignore, NULL);
}

static struct {
	const char *name;
	unsigned long code;
} vk_key[] = {
	{ "SHIFT",	VK_SHIFT << 16 },
	{ "CTRL",	VK_CONTROL << 16 },
	{ "ALT",	0x12 << 16 },
	{ "CAPSLOCK",	0x14 << 16 },
	{ "BACK",	VK_BACK << 16 },
	{ "RETURN",	VK_RETURN << 16 },
	{ "TAB",	VK_TAB << 16 },
	{ "ESCAPE",	VK_ESCAPE << 16 },
	{ "CLEAR",	VK_CLEAR << 16 },
	{ "PAUSE",	VK_PAUSE << 16 },
	{ "PRIOR",	VK_PRIOR << 16 },
	{ "NEXT",	VK_NEXT << 16 },
	{ "END",	VK_END << 16 },
	{ "HOME",	VK_HOME << 16 },
	{ "LEFT",	VK_LEFT << 16 },
	{ "UP",		VK_UP << 16 },
	{ "RIGHT",	VK_RIGHT << 16 },
	{ "DOWN",	VK_DOWN << 16 },
	{ "SELECT",	VK_SELECT << 16 },
	{ "PRINT",	VK_PRINT << 16 },
	{ "EXECUTE",	VK_EXECUTE << 16 },
	{ "SNAPSHOT",	VK_SNAPSHOT << 16 },
	{ "INSERT",	VK_INSERT << 16 },
	{ "DELETE",	VK_DELETE << 16 },
	{ "HELP",	VK_HELP << 16 },
	{ "LWIN",	VK_LWIN << 16 },
	{ "RWIN",	VK_RWIN << 16 },
	{ "APPS",	VK_APPS << 16 },
	{ "SLEEP",	VK_SLEEP << 16 },
	{ "NUMPAD0",	VK_NUMPAD0 << 16 },
	{ "NUMPAD1",	VK_NUMPAD1 << 16 },
	{ "NUMPAD2",	VK_NUMPAD2 << 16 },
	{ "NUMPAD3",	VK_NUMPAD3 << 16 },
	{ "NUMPAD4",	VK_NUMPAD4 << 16 },
	{ "NUMPAD5",	VK_NUMPAD5 << 16 },
	{ "NUMPAD6",	VK_NUMPAD6 << 16 },
	{ "NUMPAD7",	VK_NUMPAD7 << 16 },
	{ "NUMPAD8",	VK_NUMPAD8 << 16 },
	{ "NUMPAD9",	VK_NUMPAD9 << 16 },
	{ "MULTIPLY",	VK_MULTIPLY << 16 },
	{ "ADD",	VK_ADD << 16 },
	{ "SEPARATOR",	VK_SEPARATOR << 16 },
	{ "SUBTRACT",	VK_SUBTRACT << 16 },
	{ "DECIMAL",	VK_DECIMAL << 16 },
	{ "DIVIDE",	VK_DIVIDE << 16 },
	{ "F1",		VK_F1 << 16 },
	{ "F2",		VK_F2 << 16 },
	{ "F3",		VK_F3 << 16 },
	{ "F4",		VK_F4 << 16 },
	{ "F5",		VK_F5 << 16 },
	{ "F6",		VK_F6 << 16 },
	{ "F7",		VK_F7 << 16 },
	{ "F8",		VK_F8 << 16 },
	{ "F9",		VK_F9 << 16 },
	{ "F10",	VK_F10 << 16 },
	{ "F11",	VK_F11 << 16 },
	{ "F12",	VK_F12 << 16 },
	{ "F13",	VK_F13 << 16 },
	{ "F14",	VK_F14 << 16 },
	{ "F15",	VK_F15 << 16 },
	{ "F16",	VK_F16 << 16 },
	{ "F17",	VK_F17 << 16 },
	{ "F18",	VK_F18 << 16 },
	{ "F19",	VK_F19 << 16 },
	{ "F20",	VK_F20 << 16 },
	{ "F21",	VK_F21 << 16 },
	{ "F22",	VK_F22 << 16 },
	{ "F23",	VK_F23 << 16 },
	{ "F24",	VK_F24 << 16 },
	{ "NUMLOCK",	VK_NUMLOCK << 16 },
	{ "SCROLL",	VK_SCROLL << 16 },
	{ "LMENU",	VK_LMENU << 16 },
	{ "RMENU",	VK_RMENU << 16 },

	{ CN, 0 }
};

/* Look up a symbolic vkey name and return its code. */
static int
lookup_ccode(const char *s)
{
	int i;

	for (i = 0; vk_key[i].name != CN; i++) {
		if (!strcasecmp(s, vk_key[i].name))
			return vk_key[i].code;
	}
	return -1;
}

/* Look up a vkey code and return its name. */
const char *
lookup_cname(unsigned long ccode, Boolean special_only)
{
	int i;

	for (i = 0; vk_key[i].name != CN; i++) {
		if (ccode == vk_key[i].code)
			return vk_key[i].name;
	}
	if (!special_only && (ccode >= (' ' << 16) && ccode <= ('~' << 16))) {
	    	static char cbuf[2];

		cbuf[0] = (char)(ccode >> 16);
		cbuf[1] = '\0';
		return cbuf;
	}
	return CN;
}

/* Read each of the keymaps specified by the keymap resource. */
void
keymap_init(void)
{
	char *s0, *s;
	char *comma;
	static Boolean initted = False;

	/* In case this is a subsequent call, wipe out the current keymap. */
	clear_keymap();

	read_keymap("base");
	if (appres.key_map != CN) {
		s = s0 = NewString(appres.key_map);
		while ((comma = strchr(s, ',')) != CN) {
			*comma = '\0';
			if (*s)
				read_keymap(s);
			s = comma + 1;
		}
		if (*s)
			read_keymap(s);
		Free(s0);
	}

	last_3270 = IN_3270;
	last_nvt = IN_ANSI;
	set_inactive();

	if (!initted) {
		register_schange(ST_3270_MODE, keymap_3270_mode);
		register_schange(ST_CONNECT, keymap_3270_mode);
		initted = True;
	}
}

/* Erase the current keymap. */
static void
clear_keymap(void)
{
	struct keymap *k, *next;

	for (k = master_keymap; k != NULL; k = next) {
		next = k->next;
		Free(k->codes);
		Free(k->hints);
		Free(k->file);
		Free(k->action);
		Free(k);
	}
	master_keymap = NULL;
	nextk = &master_keymap;
}

/* Set the inactive flags for the current keymap. */
static void
set_inactive(void)
{
	struct keymap *k;

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

	/* Turn off elements with successors. */
	for (k = master_keymap; k != NULL; k = k->next) {
		struct keymap *j;
		struct keymap *last_j = NULL;

		if (IS_INACTIVE(k))
			continue;

		/* If it now has a successor, turn it off. */
		for (j = k->next; j != NULL; j = j->next) {
			if (!IS_INACTIVE(j) &&
			    k->ncodes == j->ncodes &&
			    !codecmp(k, j, k->ncodes)) {
				last_j = j;
			}
		}
		if (last_j != NULL) {
			k->successor = last_j;
			k->hints[0] |= KM_INACTIVE;
		}
	}
}

/* 3270/NVT mode change. */
static void
keymap_3270_mode(Boolean ignored unused)
{
	if (last_3270 != IN_3270 || last_nvt != IN_ANSI) {
		last_3270 = IN_3270;
		last_nvt = IN_ANSI;
		set_inactive();
	}
}

/* Decode hints (modifiers). */
static const char *
decode_hint(int hint)
{
	static char buf[128];
	char *s = buf;

	*s = '\0';

	if (hint & KM_SHIFT)
		s += sprintf(s, "Shift ");
	if ((hint & KM_CTRL) == KM_CTRL)
		s += sprintf(s, "Ctrl ");
	else if (hint & KM_LCTRL)
		s += sprintf(s, "LeftCtrl");
	else if (hint & KM_RCTRL)
		s += sprintf(s, "RightCtrl");
	if ((hint & KM_ALT) == KM_ALT)
		s += sprintf(s, "Alt ");
	else if (hint & KM_LALT)
		s += sprintf(s, "LeftAlt");
	else if (hint & KM_RALT)
		s += sprintf(s, "RightAlt");

	return buf;
}

/*
 * Decode a key.
 * Accepts a hint as to which form was used to specify it, if it came from a
 * keymap definition.
 */
const char *
decode_key(int k, int hint, char *buf)
{
	char *s = buf;

	if (k & 0xffff0000) {
	    	const char *n;

	    	/* VK_xxx */
		n = lookup_cname(k, False);
		(void) sprintf(buf, "%s<Key>%s", decode_hint(hint),
			       n? n: "???");
	} else if (k < ' ') {
		(void) sprintf(s, "%sCtrl <Key>%c",
			decode_hint(hint & ~KM_CTRL), k + '@');
	} else if (k == ':') {
		(void) sprintf(s, "%s<Key>colon", decode_hint(hint));
	} else if (k == ' ') {
		(void) sprintf(s, "%s<Key>space", decode_hint(hint));
	} else {
	    	wchar_t w = k;
		char c;
		BOOL udc = FALSE;

		/* Try translating to OEM for display on the console. */
		(void)WideCharToMultiByte(CP_OEMCP, 0, &w, 1, &c, 1, "?",
					  &udc);
		if (!udc)
		    	(void) sprintf(s, "%s<Key>%c", decode_hint(hint), c);
		else
			(void) sprintf(s, "%s<Key>U+%04x", decode_hint(hint),
				       k);
	}
	return buf;
}

/* Dump the current keymap. */
void
keymap_dump(void)
{
	struct keymap *k;

	for (k = master_keymap; k != NULL; k = k->next) {
		if (k->successor != NULL)
			action_output("[%s:%d]  (replaced by %s:%d)", k->file,
			    k->line, k->successor->file, k->successor->line);
		else if (!IS_INACTIVE(k)) {
			int i;
			char buf[1024];
			char *s = buf;
			char dbuf[128];

			for (i = 0; i < k->ncodes; i++) {
				s += sprintf(s, " %s",
				    decode_key(k->codes[i],
					(k->hints[i] & KM_HINTS) | KM_KEYMAP,
					    dbuf));
			}
			action_output("[%s:%d]%s: %s", k->file, k->line,
			    buf, k->action);
		}
	}
}
