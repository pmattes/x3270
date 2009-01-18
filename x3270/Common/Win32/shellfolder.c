/*
 * Copyright (c) 2008-2009, Paul Mattes.
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

	if (desktop != NULL) {
		r = SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
			SHGFP_TYPE_CURRENT, desktop);
		if (r != S_OK) {
			printf("SHGetFolderPath(DESKTOPDIRECTORY) failed: "
				"0x%x\n", (int)r);
			fflush(stdout);
			return -1;
		}
	}

	if (appdata != NULL) {
		r = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
			SHGFP_TYPE_CURRENT, appdata);
		if (r != S_OK) {
			printf("SHGetFolderPath(APPDATA) failed: 0x%x\n",
				(int)r);
			fflush(stdout);
			return -1;
		}
	    }

	return 0;
}
