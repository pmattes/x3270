/*
 * Copyright (c) 2007-2009, 2014 Paul Mattes.
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
 *	winvers.c
 *		A Windows console-based 3270 Terminal Emulator
 *		OS version query
 */

#include <windows.h>
#include <stdio.h>

#include "winversc.h"

int has_ipv6 = 1;
int windows_major_version;
int windows_minor_version;

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

	/*
	 * Enforce our version requirements explicitly, though chances are
	 * missing DLL entry points will cause us to fall over long before we
	 * get to here.
	 */
	if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ||
		info.dwMajorVersion < 5) {
		fprintf(stderr, "Minimum supported Windows version is Windows "
			"2000 (NT 5.0)\n");
		return -1;
	}

	/* Save the version for applications that need fine-grained info. */
	windows_major_version = info.dwMajorVersion;
	windows_minor_version = info.dwMinorVersion;

	/*
	 * Win2K (5.0) and earlier is IPv4-only.  WinXP (5.1) and later can
	 * have IPv6.
	 */
	if (info.dwMajorVersion == 5 && info.dwMinorVersion < 1) {
	    has_ipv6 = 0;
	}

	return 0;
}
