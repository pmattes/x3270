/*
 * Copyright (c) 2006-2023 Paul Mattes.
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
 *	relink.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Utility functions to read a session file and create a
 *		compatible shortcut.
 */

#include "globals.h"

#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "ctlrc.h"
#include "host.h"
#include "screen.h"
#include "task.h"
#include "trace.h"
#include "utils.h"

#include <wincon.h>

#include "winvers.h"
#include "shortcutc.h"
#include "windirs.h"

#include "relinkc.h"

codepages_t codepages[] = {
    { "belgian",	"500",	0, L"1252"	},
    { "belgian-euro",	"1148",	0, L"1252"	},
    { "bracket",	"37*",	0, L"1252"	},
    { "brazilian",	"275",	0, L"1252"	},
    { "cp1047",		"1047",	0, L"1252"	},
    { "cp870",		"870",	0, L"1250"	},
    { "chinese-gb18030","1388",	1, L"936"	},
    { "finnish",	"278",	0, L"1252"	},
    { "finnish-euro",	"1143",	0, L"1252"	},
    { "french",		"297",	0, L"1252"	},
    { "french-euro",	"1147",	0, L"1252"	},
    { "german",		"273",	0, L"1252"	},
    { "german-euro",	"1141",	0, L"1252"	},
    { "greek",		"875",	0, L"1253"	},
    { "hebrew",		"424",	0, L"1255"	},
    { "icelandic",	"871",	0, L"1252"	},
    { "icelandic-euro",	"1149",	0, L"1252"	},
    { "italian",	"280",	0, L"1252"	},
    { "italian-euro",	"1144",	0, L"1252"	},
    { "japanese-kana",	"930",  1, L"932"	},
    { "japanese-latin",	"939",  1, L"932"	},
    { "norwegian",	"277",	0, L"1252"	},
    { "norwegian-euro",	"1142",	0, L"1252"	},
    { "russian",	"880",	0, L"1251"	},
    { "simplified-chinese","935",1,L"936"	},
    { "spanish",	"284",	0, L"1252"	},
    { "spanish-euro",	"1145",	0, L"1252"	},
    { "thai",		"1160",	0, L"874"	},
    { "traditional-chinese","937",1,L"950"	},
    { "turkish",	"1026",	0, L"1254"	},
    { "uk",		"285",	0, L"1252"	},
    { "uk-euro",	"1146",	0, L"1252"	},
    { "us-euro",	"1140",	0, L"1252"	},
    { "us-intl",	"037",	0, L"1252"	},
    { NULL,		NULL,	0, NULL	}
};

size_t num_codepages = (sizeof(codepages) / sizeof(codepages[0])) - 1;

/*  2             3             4             5                 */
int wrows[6] = { 0, 0,
    MODEL_2_ROWS, MODEL_3_ROWS, MODEL_4_ROWS, MODEL_5_ROWS };
int wcols[6] = { 0, 0,
    MODEL_2_COLS, MODEL_3_COLS, MODEL_4_COLS, MODEL_5_COLS };

const wchar_t *
reg_font_from_host_codepage(const char *font_name, const char *codepage_name, int *codepage, int (*err)(const char *string, ...))
{
    const wchar_t *default_font;
    unsigned i, j;
    wchar_t *cpname = NULL;
    wchar_t data[1024];
    DWORD dlen;
    HKEY key;
    static wchar_t dfont[1024];
    static wchar_t font[1024];
    DWORD type;

    *codepage = 0;

    /* Figure out the default font to return. */
    if (font_name[0]) {
	MultiByteToWideChar(CP_ACP, 0, font_name, -1, dfont, 1024);
	default_font = dfont;
    } else {
	default_font = L"Lucida Console";
    }

    /* Search the table for a match. */
    for (i = 0; codepages[i].name != NULL; i++) {
	if (!strcmp(codepage_name, codepages[i].name)) {
	    cpname = codepages[i].codepage;
	    break;
	}
    }

    /* If no match, use Lucida Console. */
    if (cpname == NULL) {
	return default_font;
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
	err("RegOpenKey failed -- cannot find font\n");
	return default_font;
    }
    dlen = sizeof(data);
    if (RegQueryValueExW(key, cpname, NULL, &type, (LPVOID)data,
		&dlen) != ERROR_SUCCESS) {
	/* No codepage-specific match, try the default. */
	dlen = sizeof(data);
	if (RegQueryValueExW(key, L"0", NULL, &type, (LPVOID)data,
		    &dlen) != ERROR_SUCCESS) {
	    RegCloseKey(key);
	    err("RegQueryValueEx failed -- cannot find font\n");
	    return default_font;
	}
    }
    RegCloseKey(key);
    if (type == REG_MULTI_SZ) {
	for (i = 0; i < dlen/sizeof(wchar_t); i++) {
	    if (data[i] == 0x0000) {
		break;
	    }
	}
	if (i + 1 >= dlen / sizeof(wchar_t) || data[i + 1] == 0x0000) {
	    err("Bad registry value -- cannot find font\n");
	    return default_font;
	}
	i++;
    } else {
	i = 0;
    }
    for (j = 0; i < dlen; i++, j++) {
	if (j == 0 && data[i] == L'*') {
	    i++;
	}
	if ((font[j] = data[i]) == 0x0000) {
	    break;
	}
    }
    *codepage = _wtoi(cpname);
    return font_name[0]? dfont: font;
}

/* Convert a hexadecimal digit to a nybble. */
static unsigned
hex(char c)
{
    static char *digits = "0123456789abcdef";
    char *pos;

    pos = strchr(digits, c);
    if (pos == NULL) {
	return 0; /* XXX */
    }
    return (unsigned)(pos - digits);
}

//#define DEBUG_EDIT 1

int
read_user_settings(FILE *f, char **usp)
{
    int saw_star;
    char buf[1024];

    if (usp == NULL) {
	return 1; /* success */
    }
    *usp = NULL;

    /*
     * Read the balance of the file into a temporary buffer, ignoring
     * the '!*' line.
     */
    saw_star = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
	if (!saw_star) {
	    if (buf[0] == '!' && buf[1] == '*') {
		saw_star = 1;
	    }
	    continue;
	}
	if (*usp == NULL) {
	    *usp = malloc(strlen(buf) + 1);
	    (*usp)[0] = '\0';
	} else {
	    *usp = realloc(*usp, strlen(*usp) + strlen(buf) + 1);
	}
	if (*usp == NULL) {
#if defined(DEBUG_EDIT) /*[*/
	    printf("out of memory]\n");
#endif /*]*/
	    return 0;
	}
	strcat(*usp, buf);
    }
    return 1;
}

/*
 * Read an existing session file.
 * Returns 1 for success (file read and editable), 0 for failure.
 */
int
read_session(FILE *f, session_t *s, char **usp)
{
    char buf[1024];
    int saw_hex = 0;
    int saw_star = 0;
    unsigned long csum;
    unsigned long fcsum = 0;
    int ver;
    int s_off = 0;

    /*
     * Look for the checksum and version.  Verify the version.
     *
     * XXX: It might be a good idea to validate each '!x' line and
     * make sure that the length is right.
     */
    while (fgets(buf, sizeof(buf), f) != NULL) {
	if (buf[0] == '!' && buf[1] == 'x')
	    saw_hex = 1;
	else if (buf[0] == '!' && buf[1] == '*')
	    saw_star = 1;
	else if (buf[0] == '!' && buf[1] == 'c') {
	    if (sscanf(buf + 2, "%lx %d", &csum, &ver) != 2) {
#if defined(DEBUG_EDIT) /*[*/
		printf("[bad !c line '%s']\n", buf);
#endif /*]*/
		return 0;
	    }
	    if (ver > WIZARD_VER) {
#if defined(DEBUG_EDIT) /*[*/
		printf("[bad ver %d > %d]\n", ver, WIZARD_VER);
#endif /*]*/
		return 0;
	    }
	}
    }
    if (!saw_hex || !saw_star) {
#if defined(DEBUG_EDIT) /*[*/
	printf("[missing%s%s]\n", saw_hex? "": "hex", saw_star? "": "star");
#endif /*]*/
	return 0;
    }

    /* Checksum from the top up to the '!c' line. */
    fflush(f);
    fseek(f, 0, SEEK_SET);
    fcsum = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
	char *t;

	if (buf[0] == '!' && buf[1] == 'c') {
	    break;
	}

	for (t = buf; *t; t++) {
	    fcsum += *t & 0xff;
	}
    }
    if (fcsum != csum) {
#if defined(DEBUG_EDIT) /*[*/
	printf("[checksum mismatch, want 0x%08lx got 0x%08lx]\n", csum, fcsum);
#endif /*]*/
	return 0;
    }

    /* Once more, with feeling.  Scribble onto the session structure. */
    fflush(f);
    fseek(f, 0, SEEK_SET);
    s_off = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {

	if (buf[0] == '!' && buf[1] == 'x') {
	    char *t;

	    for (t = buf + 2; *t; t += 2) {
		if (*t == '\n') {
		    break;
		}
		if (s_off > sizeof(*s)) {
#if defined(DEBUG_EDIT) /*[*/
		    printf("[s overflow: %d > %d]\n", s_off, (int)sizeof(*s));
#endif /*]*/
		    return 0;
		}
		((char *)s)[s_off++] = (hex(*t) << 4) | hex(*(t + 1));
	    }
	} else if (buf[0] == '!' && buf[1] == 'c') {
	    break;
	}
    }

    /*
     * Read the balance of the file into a temporary buffer, ignoring
     * the '!*' line.
     */
    if (usp != NULL && read_user_settings(f, usp) == 0) {
	return 0;
    }

    /* Success */
    return 1;
}

HRESULT
create_shortcut(session_t *session, char *exepath, char *linkpath, char *args,
	char *workingdir)
{
    const wchar_t *font;
    int codepage = 0;
    int extra_height = 1;

    font = reg_font_from_host_codepage(session->font_name, session->codepage, &codepage, printf);

    if (!(session->flags & WF_NO_MENUBAR)) {
	extra_height += 2;
    }

    return create_link(
	    exepath,		/* path to executable */
	    linkpath,		/* where to put the link */
	    "wc3270 session",	/* description */
	    args,		/* arguments */
	    workingdir,		/* working directory */
	    (session->ov_rows? session->ov_rows: wrows[session->model])
		+ extra_height,	/* console rows */
	    session->ov_cols? session->ov_cols: wcols[session->model],
				/* console columns */
	    (wchar_t *)font,	/* font */
	    session->point_size,/* point size */
	    session->font_weight,/* font weight */
	    codepage);		/* code page */
}
