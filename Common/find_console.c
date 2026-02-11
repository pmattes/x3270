/*
 * Copyright (c) 2019-2026 Paul Mattes.
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
 *      find_console.c
 *              Console window support.
 */

#include "globals.h"

#include "appres.h"
#include "find_console.h"
#include "txa.h"
#include "utils.h"

#if !defined(_WIN32) /*[*/
/* Well-known consoles, in order of preference. */
static console_desc_t consoles[] = {
    { "gnome-terminal",
	"gnome-terminal --title " TITLE_SUBST " -- " COMMAND_SUBST },
    { "konsole",
	"konsole --caption " TITLE_SUBST " -e " COMMAND_SUBST },
    { "xfce4-terminal",
	"xfce4-terminal -T " TITLE_SUBST " -x " COMMAND_SUBST },
    { "xterm",
	"xterm -sb -tn xterm-256color -rv -title " TITLE_SUBST " -e "
	    COMMAND_SUBST },
    { NULL, NULL }
};
#endif /*]*/

/* Find an executable in $PATH. */
const char *
find_in_path(const char *program)
{
#   if !defined(_WIN32) /*[*/
#    define DIR_CHAR	'/'
#    define ACCESS_OK	X_OK
#    define SEP_CHAR	':'
#   else /*][*/
#    define DIR_CHAR '\\'
#    define ACCESS_OK	R_OK
#    define SEP_CHAR	';'
#   endif /*]*/
    char *path;
    char *sep;
    const char *xpath;

    /* Check for absolute path. */
#if !defined(_WIN32) /*[*/
    if (program[0] == DIR_CHAR) {
	return (access(program, ACCESS_OK) == 0)? program: NULL;
    }
#else /*][*/
    if (program[0] == DIR_CHAR || (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", toupper((int)program[0])) != NULL && program[1] == ':')) {
	return access(program, ACCESS_OK)? NULL: program;
    }
#endif /*]*/

#if defined(_WIN32) /*[*/
    /* Try the install directory. */
    xpath = txAsprintf("%s%s", instdir, program);
    if (access(xpath, ACCESS_OK) == 0) {
	return xpath;
    }
#endif /*]*/

    /* Walk $PATH. */
    path = getenv("PATH");
    while ((sep = strchr(path, SEP_CHAR)) != NULL) {
	if (sep != path) {
	    xpath = txAsprintf("%.*s%c%s", (int)(sep - path), path, DIR_CHAR, program);
	    if (access(xpath, ACCESS_OK) == 0) {
		return xpath;
	    }
	}
	path = sep + 1;
    }
    if (*path) {
	xpath = txAsprintf("%s%c%s", path, DIR_CHAR, program);
	if (access(xpath, ACCESS_OK) == 0) {
	    return xpath;
	}
    }

    return NULL;
}

#if !defined(_WIN32) /*[*/
/* Find the preferred console emulator for the prompt. */
console_desc_t *
find_console(const char **errmsg)
{
    int i;
    char *override = appres.interactive.console;
    char *pctc, *space, *dup;

    if (override == NULL) {
	/* No override. Find the best one. */
	for (i = 0; consoles[i].program != NULL; i++) {
	    if (find_in_path(consoles[i].program) != NULL) {
		return &consoles[i];
	    }
	}
	*errmsg = "None found";
	return NULL;
    }

    if (strchr(override, ' ') == NULL) {
	/* They just specified the name. */
	for (i = 0; consoles[i].program != NULL; i++) {
	    if (!strcmp(override, consoles[i].program) &&
		    find_in_path(override) != NULL) {
		return &consoles[i];
	    }
	}
	*errmsg = "Specified name not found";
	return NULL;
    }

    /* The specified a full override. */
    pctc = strstr(override, " " COMMAND_SUBST);
    if (pctc == NULL || pctc[strlen(" " COMMAND_SUBST)] != '\0')  {
	*errmsg = "Specified command does not end with " COMMAND_SUBST;
	return NULL;
    }

    space = strchr(override, ' ');
    dup = NewString(override);
    dup[space - override] = '\0';
    txdFree(dup);
    if (find_in_path(dup) != NULL) {
	static console_desc_t t_ret;

	t_ret.program = dup;
	t_ret.command_string = override;
	return &t_ret;
    }

    *errmsg = "Specified command not found";
    return NULL;
}

/* Copy console arguments to an argv array. */
int
console_args(console_desc_t *t, const char *title, const char ***s, int ix)
{
    char *str = NewString(t->command_string);
    char *saveptr;
    char *token;

    /* Split the command string into tokens. */
    while ((token = strtok_r(str, " ", &saveptr)) != NULL) {
	str = NULL;
	if (!strcmp(token, TITLE_SUBST)) {
	    array_add(s, ix++, title);
	} else if (strcmp(token, COMMAND_SUBST)) {
	    array_add(s, ix++, token);
	}
    }
    txdFree(str);
    return ix;
}
#endif /*]*/
