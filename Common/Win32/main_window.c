/*
 * Copyright (c) 2025 Paul Mattes.
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
 *	main_window.c
 *		Support for a settable/displayable main window ID.
 */

#include "globals.h"

#if !defined(_WIN32) /*[*/
#error This module is only for Win32.
#endif /*]*/

#include <errno.h>

#include "appres.h"
#include "main_window.h"
#include "popups.h"
#include "resources.h"
#include "toggles.h"
#include "txa.h"
#include "utils.h"

static const char *canonicalize_window_id(const char *value);

#if defined(_WIN64) /*[*/
# define PTRCONV strtoull
# define MAXVAL ULLONG_MAX
#else /*][*/
# define PTRCONV strtoul
# define MAXVAL ULONG_MAX
#endif /*]*/

/* Get the handle for the main window. */
HWND
get_main_window(void)
{
    if (appres.window_id != NULL) {
        return (HWND)PTRCONV(get_main_window_str(), NULL, 0);
    } else {
        return NULL;
    }
}

const char *
get_main_window_str(void)
{
    return canonicalize_window_id(appres.window_id);
}

/* Set the handle for the main window. */
void
set_main_window(HWND hwnd)
{
    Replace(appres.window_id, Asprintf("0x%p", hwnd));
}

/* Toggle the window ID. */
static toggle_upcall_ret_t
toggle_window_id(const char *name _is_unused, const char *value, unsigned flags, ia_t ia)
{
    size_t l;
    char *nextp;

    if (!*value) {
	Replace(appres.window_id, NULL);
	return TU_SUCCESS;
    }

    errno = 0;
    l = PTRCONV(value, &nextp, 0);
    if (*nextp != '\0' || (l == MAXVAL && errno == ERANGE)) {
	popup_an_error("Invalid " ResWindowId " value");
	return TU_FAILURE;
    }

    Replace(appres.window_id, Asprintf("0x%p", (void *)(size_t)l));
    return TU_SUCCESS;
}

/* Canonnicalize the window ID. */
static const char *
canonicalize_window_id(const char *value)
{
    size_t l = (size_t)INVALID_HANDLE_VALUE;
    char *nextp;

    if (value != NULL && *value) {
	size_t ld;

	errno = 0;
	ld = PTRCONV(value, &nextp, 0);
	if (*nextp == '\0' && (ld != MAXVAL || errno != ERANGE)) {
	    l = ld;
	}
    }

    return txAsprintf("0x%p", (void *)l);
}

/* Register toggles. */
void
main_window_register(void)
{
    /* Register the toggle. */
    register_extended_toggle(ResWindowId, toggle_window_id, NULL, canonicalize_window_id,
	    (void **)&appres.window_id, XRM_STRING);
}
