/*
 * Copyright 2006, 2007 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	windirs.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Find common directory paths.
 */

#include <windows.h>
#include <stdio.h>

#include "windirsc.h"

/* Locate the desktop and session directories from the Windows registry. */
int
get_dirs(char *desktop, char *appdata)
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

		hres = RegEnumValue(hkey, index, name, &nlen, 0, &type, value,
			&vlen);
		if (hres != ERROR_SUCCESS)
			break;

		if (desktop != NULL && !strcmp(name, "Desktop"))
		    	strcpy(desktop, value);
		else if (appdata != NULL && !strcmp(name, "AppData")) {
		    	strcpy(appdata, value);
			strcat(appdata, "\\wc3270\\");
		}

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
