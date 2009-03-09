/*
 * Copyright (c) 2006-2009, Paul Mattes.
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
 *	windirs.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Find common directory paths.
 */

#include <windows.h>
#include <stdio.h>

#include "windirsc.h"


/* XXX: If Win2K or later, use get_shell_folders from shf.dll.
 * Otherwise, use the function below.
 */

/* Locate the desktop and appdata directories from the Windows registry. */
static int
old_get_dirs(char *desktop, char *appdata)
{
	HRESULT hres;
	HKEY hkey;
	DWORD index;

	/* Get some paths from Windows. */
	hres = RegOpenKeyEx(HKEY_CURRENT_USER,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
    		0, KEY_QUERY_VALUE, &hkey);
	if (hres != ERROR_SUCCESS) {
	    	printf("Sorry, I can't figure out where your Desktop or "
			"Application Data directories are, Windows error "
			"%ld.\n", hres);
		return -1;
	}

	if (desktop != NULL)
		desktop[0] = '\0';
	if (appdata != NULL)
		appdata[0] = '\0';

	/*
	 * Iterate to find Desktop and AppData.
	 * We can't just go for them individually, because we can't use
	 * ReqQueryValueEx on Win98, and ReqQueryValue doesn't work.
	 */
	for (index = 0; ; index++) {
		char name[MAX_PATH];
		DWORD nlen = MAX_PATH;
		char value[MAX_PATH];
		DWORD vlen = MAX_PATH;
		DWORD type;

		hres = RegEnumValue(hkey, index, name, &nlen, 0, &type,
			(unsigned char *)value, &vlen);
		if (hres != ERROR_SUCCESS)
			break;

		if (desktop != NULL && !strcmp(name, "Desktop"))
		    	strcpy(desktop, value);
		else if (appdata != NULL && !strcmp(name, "AppData"))
		    	strcpy(appdata, value);

		if ((desktop == NULL || desktop[0]) &&
		    (appdata == NULL || appdata[0]))
		    	break;

	}
	RegCloseKey(hkey);

	if ((desktop != NULL && !desktop[0]) ||
	    (appdata != NULL && !appdata[0])) {

	    	printf("Sorry, I can't figure out where your Desktop or "
			"Application Data directories are.\n");
		return -1;
	}

	return 0;
}

/* Locate the desktop and appdata directories via the ShGetFolderPath API. */
static int
new_get_dirs(char *desktop, char *appdata)
{
    	HMODULE handle;
	static int loaded = FALSE;
	static FARPROC p;
	typedef int shfproc(char *, char *);

	if (!loaded) {
		handle = LoadLibrary("shf.dll");
		if (handle == NULL) {
			printf("Cannot find shf.dll to resolve the Desktop "
				"and Application Data directories.\n");
			return -1;
		}
		p = GetProcAddress(handle, "get_shell_folders");
		if (p == NULL) {
			printf("Cannot resolve get_shell_folders() in "
				"shf.dll\n");
			return -1;
		}
	}
	return ((shfproc *)p)(desktop, appdata);
}

/* Locate the desktop and appdata directories. */
int
get_dirs(char *desktop, char *appdata, char *appname)
{
        OSVERSIONINFO info;

	/* Figure out what version of Windows this is. */
	memset(&info, '\0', sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	if (GetVersionEx(&info) == 0) {
	    	fprintf(stderr, "Can't get Windows version\n");
	    	return -1;
	}

	if ((info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ||
		    (info.dwMajorVersion < 5)) {
	    	/* Use the registry. */
		if (old_get_dirs(desktop, appdata) < 0)
		    	return -1;
	} else {
	    	/* Use the API. */
		if (new_get_dirs(desktop, appdata) < 0)
		    	return -1;
	}

	/* Append the application name and trailing "\" to AppData. */
	strcat(appdata, "\\");
	strcat(appdata, appname);
	strcat(appdata, "\\");

	return 0;
}
