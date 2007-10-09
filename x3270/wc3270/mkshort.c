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
			80);
	else
	    	hres = Piffle(
			linkname,
			exepath,
			linkpath,
			"",
			"",
			install_dir,
			44,
			80);

	if (hres) {
		fprintf(stderr, "Link creation failed.\n");
	}

	return hres;
}
