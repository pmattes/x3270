/*
 * Copyright (c) 2007-2009, 2014, 2016-2017 Paul Mattes.
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

#include "globals.h"

#include "winvers.h"

#if defined(__GNUC__) /*[*/
/* MinGW doesn't have IsWindowsVersionOrGreater(). */
BOOL IsWindowsVersionOrGreater(WORD major_version, WORD minor_version,
	WORD service_pack_major)
{
    OSVERSIONINFOEX version_info;
    DWORDLONG condition_mask = 0;

    memset(&version_info, 0, sizeof(OSVERSIONINFOEX));
    version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    version_info.dwMajorVersion = major_version;
    version_info.dwMinorVersion = minor_version;
    version_info.wServicePackMajor = service_pack_major;
    VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(condition_mask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    return VerifyVersionInfo(&version_info,
	    VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
	    condition_mask);
}
#endif /*]*/

int
get_version_info(void)
{
    /*
     * Enforce our version requirements explicitly, though chances are
     * missing DLL entry points will cause us to fall over long before we
     * get to here.
     */
    if (!IsWindowsVersionOrGreater(5, 1, 0)) {
	fprintf(stderr, "Minimum supported Windows version is Windows XP "
		"(NT 5.1)\n");
	return -1;
    }

    return 0;
}

/* Returns true if running under Wine. */
bool
is_wine(void)
{
    static const char *(CDECL *pwine_get_version)(void);
    HMODULE hntdll = GetModuleHandle("ntdll.dll");
    if (!hntdll) {
	return false;
    }

    pwine_get_version = (void *)GetProcAddress(hntdll, "wine_get_version");
    return pwine_get_version != NULL;
}
