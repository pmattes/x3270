/*
 * Copyright (c) 2025-2026 Paul Mattes.
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
#include "main_window.h"
#include "popups.h"
#include "trace.h"
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
static HHOOK message_box_hook;
static bool message_box_activated;

/* Compute the proper location for the dialog. */
static bool
compute_location(int w, int h, int *x, int *y)
{
    int parent_x, parent_y, parent_w, parent_h;
    HWND main_window = get_main_window();

    if (main_window == NULL) {
	/* We don't know what the main window is, use the primary display. */
	parent_x = 0;
	parent_y = 0;
	parent_w = GetSystemMetrics(SM_CXFULLSCREEN);
	parent_h = GetSystemMetrics(SM_CYFULLSCREEN);
    } else {
	RECT rect;

	/* Get the rectangle for the primary window. */
	if (!GetWindowRect(main_window, &rect)) {
	    vctrace(TC_UI, "Can't get rectangle for main window 0x%08lx\n", (u_long)(size_t)main_window);
	    return false;
	}
	parent_x = rect.left;
	parent_y = rect.top;
	parent_w = rect.right - rect.left;
	parent_h = rect.bottom - rect.top;
    }

    if (parent_w < w || parent_h < h) {
	/* Strange, but possible. */
	*x = 0;
	*y = 0;
    } else {
	*x = parent_x + (parent_w - w) / 2;
	*y = parent_y + (parent_h - h) / 2;
    }
    return true;
}

/* Move the dialog box. */
static void
move_dialog(HWND dialog)
{
    RECT rect;
    int x = 0, y = 0;

    /* Figure out where to move it. */
    if (!GetWindowRect(dialog, &rect)) {
	vctrace(TC_UI, "move_dialog: Can't get rectangle for dialog\n");
	return;
    }
    if (!compute_location(rect.right - rect.left, rect.bottom - rect.top, &x, &y)) {
	vctrace(TC_PRINT, "move_dialog: Can't get rectangle for parent window\n");
	return;
    }

    /* Move it. */
    SetWindowPos(dialog, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
}

/* CBT hook procedure. */
static LRESULT CALLBACK
cbt_proc(int code, WPARAM w_param, LPARAM l_param)
{
    HWND hwnd;

    switch(code) {
    case HCBT_ACTIVATE:
	if (!message_box_activated) {
	    message_box_activated = true;
	    hwnd = (HWND)w_param;
	    vctrace(TC_UI, "cbt_proc: Got HCBT_ACTIVATE for 0x%lx, resizing\n", (u_long)(size_t)hwnd);
	    move_dialog(hwnd);
	}
	return 0;
    }
    return CallNextHookEx(message_box_hook, code, w_param, l_param);
}

/* Warning thread. */
static DWORD WINAPI
post_warning(LPVOID lpParameter _is_unused)
{
    while (true) {
	if (WaitForSingleObject(warning_semaphore, INFINITE) == WAIT_OBJECT_0 &&
		WaitForSingleObject(warning_mutex, INFINITE) == WAIT_OBJECT_0) {
	    warning_t *w = warnings;
	    char *expanded_message;
	    char *s, *t;

	    assert(w != NULL);
	    warnings = w->next;
	    if (warnings == NULL) {
		last_warning = &warnings;
	    }
	    ReleaseMutex(warning_mutex);

	    t = expanded_message = Malloc((strlen(w->message) * 2) + 1);
	    for (s = w->message; *s; s++) {
		if (*s == '\n') {
		    *t++ = '\\';
		    *t++ = 'n';
		} else {
		    *t++ = *s;
		}
	    }
	    *t = '\0';
	    vctrace(TC_UI, "Warning: %s\n", expanded_message);
	    Free(expanded_message);

	    message_box_hook = SetWindowsHookEx(WH_CBT, cbt_proc, NULL, GetCurrentThreadId());
	    MessageBox(get_main_window(), w->message, "wc3270 Warning", MB_ICONWARNING);
	    UnhookWindowsHookEx(message_box_hook);
	    message_box_activated = false;

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
