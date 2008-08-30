/*
 * Copyright 2008 by Paul Mattes.
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
 *	shellfolder.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Shell folder resolution.
 */

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

#include "shellfolderc.h"

/*
 * Use the ShGetFolderPath API to find the Desktop and AppData directories.
 * This function is linked into a DLL that is used only on Windows 2000
 * and later.  (ShGetFolderPath exists on W9x and NT4, but is in a different
 * DLL than we link wc3270 against.)
 */
int
get_shell_folders(char *desktop, char *appdata)
{
    	HRESULT r;

    	r = SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, 
		desktop);
	if (r != S_OK)
	    	return -1;
    	r = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, 
		appdata);
	if (r != S_OK)
	    	return -1;
	return 0;
}
