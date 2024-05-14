/*
 * Copyright (c) 2019-2024 Paul Mattes.
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

#if defined(_WIN32) /*[*/
# error Not for Windows
#endif /*]*/

#include "globals.h"

#include "appres.h"
#include "find_console.h"
#include "txa.h"
#include "utils.h"

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

/* Find an executable in $PATH. */
bool
find_in_path(const char *program)
{
    char *path;
    char *colon;

    if (program[0] == '/') {
	return access(program, X_OK) == 0;
    }
    path = getenv("PATH");
    while ((colon = strchr(path, ':')) != NULL) {
	if (colon != path) {
	    char *xpath = txAsprintf("%.*s/%s", (int)(colon - path), path, program);

	    if (access(xpath, X_OK) == 0) {
		return true;
	    }
	}
	path = colon + 1;
    }
    if (*path) {
	char *xpath = txAsprintf("%s/%s", path, program);

	if (access(xpath, X_OK) == 0) {
	    return true;
	}
    }
    return false;
}

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
	    if (find_in_path(consoles[i].program)) {
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
		    find_in_path(override)) {
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
    dup = txdFree(NewString(override));
    dup[space - override] = '\0';
    if (find_in_path(dup)) {
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
    char *str = txdFree(NewString(t->command_string));
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
    return ix;
}
