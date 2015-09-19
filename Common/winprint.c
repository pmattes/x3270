/*
 * Copyright (c) 1994-2015 Paul Mattes.
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
 *	winprint.c
 *		Windows screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"

#include <errno.h>

#include "popups.h"
#include "resources.h"
#include "print_screen.h"
#include "trace.h"
#include "utils.h"

#include "winprint.h"

#include <fcntl.h>
#include <sys/stat.h>
#include "w3misc.h"

/* Typedefs */
typedef struct {	/* Windows screen print context */
    char *filename;	/* Name of file to print (and unlink) */
    char *wp;		/* Path of WORDPAD.EXE */
    char *args;		/* Parameters for Wordpad */
} wsp_t;

/* Globals */

/* Statics */

/*
 * A Windows version of something like mkstemp().  Creates a temporary
 * file in $TEMP, returning its path and an open file descriptor.
 */
int
win_mkstemp(char **path, ptype_t ptype)
{
    char *s;
    int fd;
    unsigned gen = 0;

    while (gen < 1000) {
	s = getenv("TEMP");
	if (s == NULL) {
	    s = getenv("TMP");
	}
	if (s == NULL) {
	    s = "C:";
	}
	if (gen) {
	    *path = xs_buffer("%s\\x3h-%u-%u.%s", s, getpid(), gen,
		    (ptype == P_RTF)? "rtf": "txt");
	} else {
	    *path = xs_buffer("%s\\x3h-%u.%s", s, getpid(),
		    (ptype == P_RTF)? "rtf": "txt");
	}
	fd = open(*path, O_CREAT | O_RDWR, S_IREAD | S_IWRITE | O_EXCL);
	if (fd >= 0) {
	    break;
	}

	/* Try again. */
	Free(*path);
	*path = NULL;
	if (errno != EEXIST)
	    break;
	gen++;
    }
    return fd;
}

/*
 * Find WORDPAD.EXE.
 */
#define PROGRAMFILES "%ProgramFiles%"
static char *
find_wordpad(void)
{
    char data[1024];
    DWORD dlen;
    char *slash;
    static char *wp = NULL;
    HKEY key;

    if (wp != NULL) {
	return wp;
    }

    /* Get the shell print command for RTF files. */
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\Classes\\rtffile\\shell\\print\\command",
		0,
		KEY_READ,
		&key) != ERROR_SUCCESS) {
	return NULL;
    }
    dlen = sizeof(data);
    if (RegQueryValueEx(key,
		NULL,
		NULL,
		NULL,
		(LPVOID)data,
		&dlen) != ERROR_SUCCESS) {
	RegCloseKey(key);
	return NULL;
    }
    RegCloseKey(key);

    if (data[0] == '"') {
	char data2[1024];
	char *q2;

	/* The first token is quoted; that's the path. */
	strcpy(data2, data + 1);
	q2 = strchr(data2, '"');
	if (q2 == NULL) {
	    return NULL;
	}
	*q2 = '\0';
	strcpy(data, data2);
    } else if ((slash = strchr(data, '/')) != NULL) {
	/* Find the "/p". */
	*slash = '\0';
	if (*(slash - 1) == ' ')
	    *(slash - 1) = '\0';
    }

    if (!strncasecmp(data, PROGRAMFILES, strlen(PROGRAMFILES))) {
	char *pf = getenv("PROGRAMFILES");

	/* Substitute %ProgramFiles%. */
	if (pf == NULL) {
	    return NULL;
	}
	wp = xs_buffer("%s\\%s", pf, data + strlen(PROGRAMFILES));
    } else {
	wp = NewString(data);
    }
    return wp;
}

/* Asynchronous thread to print a screen snapshot with Wordpad. */
static DWORD WINAPI
run_wordpad(LPVOID lpParameter)
{
    wsp_t *w = (wsp_t *)lpParameter;
    char *cmdline;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    /* Run the command and wait for it to complete. */
    cmdline = xs_buffer("\"%s\" %s", w->wp, w->args);
    memset(&si, '\0', sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, '\0', sizeof(pi));
    if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, DETACHED_PROCESS,
		NULL, NULL, &si, &pi)) {
	    WaitForSingleObject(pi.hProcess, INFINITE);
	    CloseHandle(pi.hProcess);
	    CloseHandle(pi.hThread);
    }
    Free(cmdline);

    /* Unlink the temporary file. */
    (void) unlink(w->filename);

    /*
     * Free the memory.
     * This is a bit scary, but I believe it's thread-safe.
     * If not, I'll just leak the memory.
     */
    Free(w->args);
    Free(w);

    /* No more need for the thread. */
    ExitThread(0);
    return 0;
}

/*
 * Close a completed thread handle.
 */
static void
close_wsh(iosrc_t fd, ioid_t id)
{
    CloseHandle((HANDLE)fd);
    RemoveInput(id);
    if (appres.interactive.do_confirms) {
	popup_an_info("Screen image printed.\n");
    }
}

/* Start WordPad to print something, synchronously. */
void
start_wordpad_sync(const char *action_name, const char *filename,
	const char *printer)
{
    char *wp;
    char *cmd;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    /* Find WordPad. */
    wp = find_wordpad();
    if (wp == NULL) {
	popup_an_error("%s: Can't find WORDPAD.EXE", action_name);
	return;
    }

    /* Construct the command line. */
    if (printer != NULL && printer[0]) {
	cmd = xs_buffer("\"%s\" /pt \"%s\" \"%s\"", wp, filename, printer);
    } else {
	cmd = xs_buffer("\"%s\" /p \"%s\"", wp, filename);
    }
    vtrace("%s command: %s\n", action_name, cmd);

    /* Run the command and wait for it to complete. */
    memset(&si, '\0', sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, '\0', sizeof(pi));
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, DETACHED_PROCESS, NULL,
		NULL, &si, &pi)) {
	popup_an_error("%s: WORDPAD start failure: %s",
		action_name, win32_strerror(GetLastError()));
    } else {
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
    }
    Free(cmd);
}

/* Start WordPad to print something, asynchonously. */
void
start_wordpad_async(const char *action_name, const char *filename,
	const char *printer)
{
    char *wp;
    wsp_t *w;
    char *args;
    HANDLE print_thread;

    /* Find WordPad. */
    wp = find_wordpad();
    if (wp == NULL) {
	popup_an_error("%s: Can't find WORDPAD.EXE", action_name);
	return;
    }

    /* Construct the command line. */
    if (printer != NULL && printer[0]) {
	args = xs_buffer("/pt \"%s\" \"%s\"", filename, printer);
    } else {
	args = xs_buffer("/p \"%s\"", filename);
    }
    vtrace("%s command: \"%s\" %s\n", action_name, wp, args);

    /*
     * Create a thread to start WordPad, wait for it to terminate, and
     * delete the temporary file.
     */
    w = Malloc(sizeof(wsp_t) + strlen(filename) + 1);
    w->filename = (char *)(w + 1);
    strcpy(w->filename, filename);
    w->wp = wp;
    w->args = args;
    print_thread = CreateThread(NULL, 0, run_wordpad, w, 0, NULL);
    if (print_thread == NULL) {
	popup_an_error("%s: Cannot create printing thread: %s\n",
		action_name, win32_strerror(GetLastError()));
	Free(w);
	Free(args);
	return;
    }

    /*
     * Make sure the thread handle is closed when the screen printing is
     * done.
     */
    (void) AddInput(print_thread, close_wsh);
}
