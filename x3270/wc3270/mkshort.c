/*
 * Copyright (c) 2006-2009, Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	mkshort.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Quick standalone utility to Create a shortcut to
 *		wc3270.exe on the desktop with the right properties.
 */

#include "globals.h"

#include <windows.h>
#include <wincon.h>
#include <shlobj.h>
#include "shlobj_missing.h" /* missing IShellLinkDataist stuff from MinGW */

#include "shortcutc.h"
#include "winversc.h"

int
main(int argc, char *argv[])
{
	char exepath[MAX_PATH];
	char linkpath[MAX_PATH];
	HRESULT hres;
	char *install_dir;
	char *exe;
	char *linkname;

	(void) get_version_info();

	/* Pull in the parameter. */
	if (argc != 4) {
	    	fprintf(stderr, "usage: %s install-dir exe linkname\n",
			argv[0]);
		return 1;
	}
	install_dir = argv[1];
	exe = argv[2];
	linkname = argv[3];
	sprintf(exepath, "%s\\%s", install_dir, exe);

	/* Figure out the link path. */
	if (is_nt) {
	    	char *userprof;

		userprof = getenv("USERPROFILE");
		if (userprof == NULL) {
			fprintf(stderr, "Sorry, I can't figure out where your user "
				"profile is.\n");
			return 1;
		}
		sprintf(linkpath, "%s\\Desktop\\%s.lnk", userprof, linkname);
	} else {
		char *windir;

		windir = getenv("WinDir");
		if (windir == NULL) {
			printf("Sorry, I can't figure out where %%WinDir%% "
				"is.\n");
			return -1;
		}
		sprintf(linkpath, "%s\\Desktop\\%s.pif", windir, linkname);
	}

	/* Create the link. */
	if (is_nt)
		hres = CreateLink(
			exepath,
			linkpath,
			NULL,
			NULL,
			install_dir,
			44,
			80,
			L"Lucida Console",
			0,
			0);
	else
	    	hres = Piffle(
			linkname,
			exepath,
			linkpath,
			"",
			"",
			install_dir,
			44,
			80,
			"Lucida Console");

	if (hres) {
		fprintf(stderr, "Link creation failed.\n");
	}

	return hres;
}
