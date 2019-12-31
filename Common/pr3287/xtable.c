/*
 * Copyright (c) 2013, 2014, 2019  Paul Mattes.
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
 * pr3287 custom translation table support (-xtable).
 */

#include "globals.h"
#include <errno.h>

#include "xtablec.h"

/* Symbolically-named ASCII control characters. */
static struct {
    const char *name;
    int value;
} cc[] = {
    { "bs", '\b' },
    { "cr", '\r' },
    { "bel", '\a' },
    { "esc", 27 },
    { "escape", 27 },
    { "ff", '\f' },
    { "ht", '\t' },
    { "lf", 10 },
    { "nl", 10 },
    { "nul", 0 },
    { "space", 32 },
    { "tab", '\t' },
    { "vt", '\v' },
    { NULL, 0 }
};

/* Translation table. */
#define MAX_EX	64
static struct {
    int len;	/* -1 for no translation, 0 for empty translation */
    unsigned char expansion[MAX_EX];
} xls[256];
static int xtable_initted = 0;

/*
 * Expand 1-3 octal characters.
 * (*s) points to the first.
 * Point (*s) at the last.
 */
static char
loct(char **s)
{
    char *t = *s;
    char r = *t - '0';

    if (*(t + 1) >= '0' && *(t + 1) <= '7') {
	r *= 8;
	r += *++t - '0';
	if (*(t + 1) >= 0 && *(t + 1) < '7') {
	    r *= 8;
	    r += *++t - '0';
	}
    }
    *s = t;
    return r;
}

/*
 * Translate a hex digit to 0..16.
 * Return -1 for an invalid digit.
 */
static int
xdigit(char c)
{
    if (c >= '0' && c <= '9') {
	return c - '0';
    } else if (c >= 'a' && c <= 'f') {
	return 10 + (c - 'a');
    } else if (c >= 'A' && c <= 'F') {
	return 10 + (c - 'A');
    } else {
	return -1;
    }
}

/*
 * Expand 1-2 hex characters.
 * (*s) points to the character before the first.
 * Point (*s) at the last.
 */
static int
lhex(char **s)
{
    char *t = *s;
    char r = 0;
    int d;

    d = xdigit(*(t + 1));
    if (d >= 0) {
	r = d;
	t++;
	d = xdigit(*(t + 1));
	if (d >= 0) {
	    r = (r * 16) + d;
	    t++;
	}
    } else {
	return -1;
    }
    *s = t;
    return r;
}

#define is_white(c)	((c) == ' ' || (c) == '\t' || (c) == 'r' || (c) == '\n')
#define is_delim(c)	(is_white(c) || (c) == '\0')
#define is_comment(s)	(*(s) == '!' || *(s) == '#' || !strncmp(s, "//", 2))

/* Initialize the translation table. */
int
xtable_init(const char *filename)
{
    FILE *f;
    char buf[1024];
    int lno = 0;
    int i;
    int rc = 0;

    /* Initialize the translation table. */
    for (i = 0; i < 256; i++) {
	xls[i].len = -1;
    }

    /* We're initted well enough for xtable_lookup() to be called. */
    xtable_initted = 1;

    /* Open the file. */
    f = fopen(filename, "r");
    if (f == NULL) {
	errmsg("%s: %s", filename, strerror(errno));
	return -1;
    }

    /* Read it. */
    while (fgets(buf, sizeof(buf), f) != NULL) {
	char *s;
	unsigned long ebc, asc;
	char *p;
	char xl[64];
	int sx;

	lno++;
	s = buf;

	while (is_white(*s)) {
	    s++;
	}
	/* Skip empty lines. */
	if (!*s) {
	    continue;
	}
	/* Skip comment lines. */
	if (is_comment(s)) {
	    continue;
	}

	/*
	 * The format of a line is:
	 *  ebcdic EBCDIC-code ascii [ASCII-code]...
	 * An EBCDIC code can be specified as:
	 *  X'nn'    Hexadecimal
	 *  0xnn     Hexadecimal
	 *  0nn      Octal
	 *  nn       Decimal
	 * An ASCII code can be specified as:
	 *  0xn      Hexadecimal
	 *  0n       Octal
	 *  n        Decimal
	 *  ^X       Control code
	 *  CR NL LF FF NUL TAB SPACE ESC ESCAPE
	 *           More control codes
	 *  "text"   Literal text
	 * Named and literal characters are not supported on the EBCDIC
	 *  side because their definition depends on the host codepage.
	 * Literal characters are supported on the ASCII side, though
	 *  their interpretation of single characters depends on the
	 *  local character set.
	 */

	/* Parse 'ebcdic'. */
	if (strncasecmp(s, "ebcdic", strlen("ebcdic")) ||
	    !is_white(*(s + strlen("ebcdic")))) {
	    errmsg("%s:%d: missing 'ebcdic' keyword", filename, lno);
	    rc = -1;
	    goto done;
	}

	s += strlen("ebcdic");
	while (is_white(*s)) {
		s++;
	}
	/* Skip empty lines. */
	if (!*s) {
	    continue;
	}
	/* Skip comment lines. */
	if (is_comment(s)) {
	    continue;
	}

	/* Parse the EBCDIC code. */
	if (!strncasecmp(s, "X'", 2)) {
	    ebc = strtoul(s + 2, &p, 16);
	    if (*p != '\'' || !is_delim(*(p + 1))) {
		errmsg("%s:%d: EBCDIC code X'nn' syntax error", filename, lno);
		rc = -1;
		goto done;
	    }
	    p++;
	} else {
	    ebc = strtoul(s, &p, 0);
	    if (!is_delim(*p)) {
		errmsg("%s:%d: EBCDIC code number syntax error", filename, lno);
		rc = -1;
		goto done;
	    }
	}
	if (ebc < 64) {
	    errmsg("%s:%d: EBCDIC code < 64", filename, lno);
	    rc = -1;
	    goto done;
	}
	if (ebc > 255) {
	    errmsg("%s:%d: EBCDIC code > 255", filename, lno);
	    rc = -1;
	    goto done;
	}
	s = p;
	while (is_white(*s)) {
	    s++;
	}

	/* Parse 'ascii'. */
	if (strncasecmp(s, "ascii", strlen("ascii")) ||
	    !is_white(*(s + strlen("ascii")))) {
	    errmsg("%s:%d: missing 'ascii' keyword", filename, lno);
	    rc = -1;
	    goto done;
	}

	s += strlen("ascii");
	/* Skip empty lines. */
	if (!*s) {
	    continue;
	}
	/* Skip comment lines. */
	if (is_comment(s)) {
	    continue;
	}

	/* Parse the ASCII codes. */
	sx = 0;
	while (*s) {
	    while (is_white(*s)) {
		s++;
	    }
	    if (!*s || is_comment(s)) {
		break;
	    }
	    if (*s >= '0' && *s <= '9') {
		/* Looks like a number. */
		asc = strtoul(s, &p, 0);
		if (!is_delim(*p)) {
		    errmsg("%s:%d:%zd: number syntax error", filename, lno,
			    s - buf + 1);
		    rc = -1;
		    goto done;
		}
		s = p;
	    } else if (*s == '^') {
		/* Looks like a control character. */
		if (*(s + 1) >= '@' &&
		    *(s + 1) <= '_' &&
		    is_delim(*(s + 2))) {
		    asc = *(s + 1) - '@';
		} else {
		    errmsg("%s:%d:%zd: control character syntax error",
			    filename, lno, s - buf + 1);
		    rc = -1;
		    goto done;
		}
		s += 2;
	    } else if (*s == '"') {
		char *t;

		/* Quoted text. */
		t = ++s;
		for (;;) {
		    t = strchr(t, '"');
		    if (t != s && *(t - 1) == '\\') {
			t++;
			continue;
		    }
		    if (t == NULL || !is_delim(*(t + 1))) {
			errmsg("%s:%d:%zd: quoted text syntax error ",
				filename, lno, s - buf + 1);
			rc = -1;
			goto done;
		    }
		    break;
		}
		while (s < t) {
		    int c = *s++;

		    if (c == '\\') {
			switch (*s) {
			case '0':
			    c = loct(&s);
			    break;
			case 'a':
			    c = '\a';
			    break;
			case 'b':
			    c = '\b';
			    break;
			case 'f':
			    c = '\f';
			    break;
			case 'n':
			    c = '\n';
			    break;
			case 'r':
			    c = '\r';
			    break;
			case 't':
			    c = '\t';
			    break;
			case 'v':
			    c = '\v';
			    break;
			case 'x':
			    c = lhex(&s);
			    if (c < 0) {
				errmsg("%s:%d:%zd: \\x syntax error ",
					filename, lno, s - buf + 1);
				rc = -1;
				goto done;
			    }
			    break;
			default:
			    c = *s;
			    break;
			}
			s++;
		    }
		    if ((size_t)sx > sizeof(xl)) {
			errmsg("%s:%d: too many (%d) ASCII characters",
				filename, lno, sx);
			rc = -1;
			goto done;
		    }
		    xl[sx++] = c;
		}
		/* Skip the trailing double quote. */
		s++;

		/*
		 * Don't fall through to the logic that adds
		 * one character to the translation.
		 */
		continue;
	    } else {
		int j;

		/* Might be a symbolic character. */
		for (j = 0; cc[j].name != NULL; j++) {
		    size_t sl = strlen(cc[j].name);

		    if (!strncasecmp(cc[j].name, s, sl) &&
			is_delim(s[sl])) {
			asc = cc[j].value;
			s += sl;
			break;
		    }
		}
		if (cc[j].name == NULL) {
		    errmsg("%s:%d:%zd: unknown token", filename, lno,
			    s - buf + 1);
		    rc = -1;
		    goto done;
		}
	    }
	    if (asc > 255) {
		errmsg("%s:%d: ASCII code > 255", filename, lno);
		rc = -1;
		goto done;
	    }
	    if ((size_t)sx > sizeof(xl)) {
		errmsg("%s:%d: too many (%d) ASCII characters", filename,
			lno, sx);
		rc = -1;
		goto done;
	    }
	    xl[sx++] = (char)asc;
	}

	/* Save the translation. */
	xls[ebc].len = sx;
	memcpy(xls[ebc].expansion, xl, sx);
    }

#if defined(DUMP_TABLE) /*[*/
    {
	int ebc;

	for (ebc = 0; ebc < 256; ebc++) {
	    if (xls[ebc].len >= 0) {
		int k;

		printf("X'%02X' ->", ebc);
		for (k = 0; k < xls[ebc].len; k++) {
		    printf(" 0x%02x", (unsigned char)xls[ebc].expansion[k]);
		}
		printf("\n");
	    }
	}
	fflush(stdout); /* for Windows */
    }
#endif /*]*/

done:
    fclose(f);
    return rc;
}

/*
 * Translate an EBCDIC code to ASCII, using the custom table.
 * Returns:
 *   -1 no translation defined (use default table)
 *    0 expand to nothing
 *    n expand to <n> returned characters
 */
int
xtable_lookup(unsigned char ebc, unsigned char **r)
{
    if (!xtable_initted || ebc < 0x40) {
	*r = NULL;
	return -1;
    }

    if (xls[ebc].len > 0) {
	*r = xls[ebc].expansion;
    } else if (xls[ebc].len == 0) {
	*r = (unsigned char *)"";
    } else {
	*r = NULL;
    }
    return xls[ebc].len;
}
