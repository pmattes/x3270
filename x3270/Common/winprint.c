/*
 * Copyright (c) 1994-2013, Paul Mattes.
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
 *		Windows printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"
#include "tablesc.h"

#include <errno.h>

#include "objects.h"
#include "popupsc.h"
#include "resources.h"
#include "printc.h"
#include "trace_dsc.h"
#include "utilc.h"

#include "winprintc.h"

#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "w3miscc.h"
#if defined(WC3270) /*[*/
# include <screenc.h>
#endif /*]*/

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

/* Typedefs */
typedef struct {		/* Windows screen print context */
	char *filename;		/* Name of file to print (and unlink) */
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
	static unsigned gen = 0;

	s = getenv("TEMP");
	if (s == NULL)
		s = getenv("TMP");
	if (s == NULL)
		s = "C:";
	*path = xs_buffer("%s\\x3h%u-%u.%s", s, getpid(), gen,
			    (ptype == P_RTF)? "rtf": "txt");
	gen = (gen + 1) % 1000;
	fd = open(*path, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
	if (fd < 0) {
	    Free(*path);
	    *path = NULL;
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

	if (wp != NULL)
	    return wp;

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
	SHELLEXECUTEINFO info;

	/* Run the command and wait for it to complete. */
	memset(&info, '\0', sizeof(info));
	(void) ShellExecuteEx(&info);
	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpFile = w->wp;
	info.lpParameters = w->args;
	info.nShow = SW_MINIMIZE;
	(void) ShellExecuteEx(&info);
	if (info.hProcess) {
		WaitForSingleObject(info.hProcess, INFINITE);
		CloseHandle(info.hProcess);
	}

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
close_wsh(unsigned long fd, ioid_t id)
{
	CloseHandle((HANDLE)fd);
	RemoveInput(id);
#if defined(C3270) /*[*/
	if (appres.do_confirms)
		popup_an_info("Screen image printed.\n");
#endif /*]*/
}

/* Start WordPad to print something, synchronously. */
void
start_wordpad_sync(char *action_name, char *filename, char *printer)
{
	char *wp;
	char *cmd;

	/* Find WordPad. */
	wp = find_wordpad();
	if (wp == NULL) {
		popup_an_error("%s: Can't find WORDPAD.EXE", action_name);
		return;
	}

	/* Construct the command line. */
	if (printer != NULL && printer[0])
		cmd = xs_buffer("start \"\" /wait /min \"%s\" /pt \"%s\" "
			"\"%s\"",
			wp, filename, printer);
	else
		cmd = xs_buffer("start \"\" /wait /min \"%s\" /p \"%s\"",
			wp, filename);
	trace_event("%s command: %s\n", action_name, cmd);

	/* Run the command. */
	system(cmd);
	Free(cmd);
}

/* Start WordPad to print something, asynchonously. */
void
start_wordpad_async(char *action_name, char *filename, char *printer)
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
	if (printer != NULL && printer[0])
		args = xs_buffer("/pt \"%s\" \"%s\"", filename, printer);
	else
		args = xs_buffer("/p \"%s\"", filename);
	trace_event("%s() command: \"%s\" %s\n", action_name, wp, args);

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
	(void) AddInput((unsigned long)print_thread, close_wsh);
}
