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
 *     * Neither the name of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
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
 *	warning.c
 *		A Windows console 3270 Terminal Emulator
 *		Warning pop-ups
 */

#include "globals.h"

#include <assert.h>
#include "popups.h"
#include "w3misc.h"

#include "warning.h"

/* Warning queue element. */
typedef struct warning {
    struct warning *next;
    char *message;
} warning_t;

/* Warning queue. */
warning_t *warnings = NULL;
warning_t **last_warning = &warnings;

static HANDLE warning_semaphore = INVALID_HANDLE_VALUE;
static HANDLE warning_mutex = INVALID_HANDLE_VALUE;
static HANDLE warning_thread = INVALID_HANDLE_VALUE;

/* Warning thread. */
static DWORD WINAPI
post_warning(LPVOID lpParameter _is_unused)
{
    while (true) {
	if (WaitForSingleObject(warning_semaphore, INFINITE) == WAIT_OBJECT_0 &&
		WaitForSingleObject(warning_mutex, INFINITE) == WAIT_OBJECT_0) {
	    warning_t *w = warnings;

	    assert(w != NULL);
	    warnings = w->next;
	    if (warnings == NULL) {
		last_warning = &warnings;
	    }
	    ReleaseMutex(warning_mutex);
	    MessageBox(NULL, w->message, "wc3270 Warning", MB_ICONWARNING);
	    Free(w);
	}
    }
}

/*
 * Initialization.
 */
static void
warning_init(void)
{
    static bool initted = false;

    if (initted) {
	return;
    }
    initted = true;

    warning_semaphore = CreateSemaphore(NULL, 0, 1000, NULL);
    warning_mutex = CreateMutex(NULL, FALSE, NULL);
    warning_thread = CreateThread(NULL, 0, post_warning, NULL, 0, NULL);
    if (warning_thread == NULL) {
	popup_an_error("Cannot create warning thread: %s", win32_strerror(GetLastError()));
    }
}

/*
 * Pop up a warning message.
 */
void
popup_warning(const char *s)
{
    warning_t *w;

    warning_init();
    w = (warning_t *)Malloc(sizeof(warning_t) + strlen(s) + 1);
    w->next = NULL;
    w->message = (char *)(w + 1);
    strcpy(w->message, s);
    if (WaitForSingleObject(warning_mutex, INFINITE) == WAIT_OBJECT_0) {
	*last_warning = w;
	last_warning = &w->next;
	ReleaseMutex(warning_mutex);
	ReleaseSemaphore(warning_semaphore, 1, NULL);
    }
}
