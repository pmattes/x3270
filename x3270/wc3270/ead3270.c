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
 *	ead3270.c
 *		A Windows console 3270 Terminal Emulator
 *		Application Data directory explorer.
 */

#include <windows.h>
#include <stdio.h>
#include <limits.h>
#include "windirsc.h"

int
main(int argc, char *argv[])
{
    char appdata[MAX_PATH];
    int sl;
    char short_ad[MAX_PATH];
    char cmd[7 + MAX_PATH];

    /* Get the application data directory. */
    if (get_dirs(NULL, appdata, "wc3270") < 0) {
	fprintf(stderr, "get_dirs failed\n");
	return 1;
    }
    sl = strlen(appdata);
    if (sl > 1 && appdata[sl - 1] == '\\')
	appdata[sl - 1] = '\0';

    /* Convert it to a short name. */
    if (GetShortPathName(appdata, short_ad, sizeof(short_ad)) == 0) {
	fprintf(stderr, "GetShortPathName(\"%s\") failed, win32 error %ld\n",
		appdata, GetLastError());
	return 1;
    }

    /* Run it. */
    sprintf(cmd, "start %s", short_ad);
    system(cmd);
    return 0;
}
