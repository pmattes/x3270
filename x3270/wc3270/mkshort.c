/*
 * Copyright (c) 2006-2010, 2013 Paul Mattes.
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

#include <wincon.h>

#include "shortcutc.h"
#include "winversc.h"

int
main(int argc, char *argv[])
{
	char exe_path[MAX_PATH];
	HRESULT hres;
	char *install_dir;
	char *exe_name;
	char *link_path;

	(void) get_version_info();

	/* Pull in the parameter. */
	if (argc != 4) {
	    	fprintf(stderr, "usage: %s install-dir exe-name link-path\n",
			argv[0]);
		return 1;
	}
	install_dir = argv[1];
	exe_name = argv[2];
	link_path = argv[3];
	sprintf(exe_path, "%s\\%s", install_dir, exe_name);

	/* Create the link. */
	if (is_nt)
		hres = CreateLink(
			exe_path,
			link_path,
			NULL,
			NULL,
			install_dir,
			46,
			80,
			L"Lucida Console",
			0,
			0);
	else {
		char link_name[MAX_PATH];
		char *bsl;
		size_t sl;

		/*
		 * Extract the link name (the PIF title) from the link path.
		 */
		bsl = strrchr(link_path, '\\');
		if (bsl == NULL) {
			fprintf(stderr, "cannot figure out link name from "
				"'%s'\n", link_path);
			return 1;
		}
		bsl++;
		sl = strlen(bsl);
		if (sl < 5 || strcasecmp(bsl + sl - 4, ".pif")) {
			fprintf(stderr, "cannot figure out link name from "
				"'%s'\n", link_path);
			return 1;
		}
		strncpy(link_name, bsl, sl - 4);
		link_name[sl - 4] = '\0';

	    	hres = Piffle(
			link_name,
			exe_path,
			link_path,
			"",
			"",
			install_dir,
			46,
			80,
			"Lucida Console");
	}

	if (hres) {
		fprintf(stderr, "link creation \"%s\" failed\n", link_path);
	}

	return hres;
}
