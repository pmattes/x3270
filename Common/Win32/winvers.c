/*
 * Copyright 2007 by Paul Mattes.
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
 *	winvers.c
 *		A Windows console-based 3270 Terminal Emulator
 *		OS version query
 */

#include <windows.h>
#include <stdio.h>

#include "winversc.h"

int is_nt = 1;
int has_ipv6 = 1;

int
get_version_info(void)
{
        OSVERSIONINFO info;

	/* Figure out what version of Windows this is. */
	memset(&info, '\0', sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	if (GetVersionEx(&info) == 0) {
	    	fprintf(stderr, "Can't get Windows version\n");
	    	return -1;
	}

	/* Yes, people still run Win98. */
	if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
	    	is_nt = 0;

	/* Win2K and earlier is IPv4-only.  WinXP and later can have IPv6. */
	if (!is_nt ||
		info.dwMajorVersion < 5 ||
		(info.dwMajorVersion == 5 && info.dwMinorVersion < 1)) {
	    has_ipv6 = 0;
	}

	return 0;
}
