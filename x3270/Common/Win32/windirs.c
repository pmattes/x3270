/*
 * Copyright (c) 2006-2009, 2014 Paul Mattes.
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
 *	windirs.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Find common directory paths.
 */

#include "globals.h"

#include "windirsc.h"

/* Locate the desktop and appdata directories via the SHGetFolderPath API. */
static int
new_get_dirs(char **desktop, char **appdata, char **common_desktop,
	char **common_appdata)
{
    	HRESULT r;

	if (desktop != NULL) {
	    	*desktop = malloc(MAX_PATH);
		if (*desktop == NULL)
		    	return -1;
		r = SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
			SHGFP_TYPE_CURRENT, *desktop);
		if (r != S_OK) {
			printf("SHGetFolderPath(DESKTOPDIRECTORY) failed: "
				"0x%x\n", (int)r);
			fflush(stdout);
			return -1;
		}
	}

	if (appdata != NULL) {
	    	*appdata = malloc(MAX_PATH);
		if (*appdata == NULL)
		    	return -1;
		r = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
			SHGFP_TYPE_CURRENT, *appdata);
		if (r != S_OK) {
			printf("SHGetFolderPath(APPDATA) failed: 0x%x\n",
				(int)r);
			fflush(stdout);
			return -1;
		}
	}

	if (common_desktop != NULL) {
	    	*common_desktop = malloc(MAX_PATH);
		if (*common_desktop == NULL)
		    	return -1;
		r = SHGetFolderPath(NULL, CSIDL_COMMON_DESKTOPDIRECTORY,
			NULL, SHGFP_TYPE_CURRENT, *common_desktop);
		if (r != S_OK) {
			printf("SHGetFolderPath(COMMON_DESKTOPDIRECTORY) "
				"failed: 0x%x\n", (int)r);
			fflush(stdout);
			return -1;
		}
	}

	if (common_appdata != NULL) {
	    	*common_appdata = malloc(MAX_PATH);
		if (*common_appdata == NULL)
		    	return -1;
		r = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
			SHGFP_TYPE_CURRENT, *common_appdata);
		if (r != S_OK) {
			printf("SHGetFolderPath(COMMON_APPDATA) failed: "
				"0x%x\n", (int)r);
			fflush(stdout);
			return -1;
		}
	}

	return 0;
}

/* Return the current working directory, always ending with a '\'. */
static char *
getcwd_bsl(void)
{
    	char *wd;
	size_t sl;

	wd = _getcwd(NULL, 0);
	sl = strlen(wd);
	if (sl > 0 && wd[sl - 1] != '\\') {
		char *xwd;

		xwd = malloc(sl + 2);
		if (xwd == NULL)
			return NULL;

		strcpy(xwd, wd);
		strcat(xwd, "\\");
		free(wd);
		wd = xwd;
	}
	return wd;
}

/*
 * Locate the installation, desktop and app-data directories.
 * Return them in malloc'd buffers, all with trailing backslashes.
 * Also return a flag indicating that the program was installed.
 * If returning AppData and the program is installed, make sure that the
 * directory exists.
 *
 *  param[in]  argv0	 	program's argv[0]
 *  param[in]  appname	 	application name (for app-data)
 *  param[out] instdir	 	installation directory (or NULL)
 *  param[out] desktop	 	desktop directory (or NULL)
 *  param[out] appdata	 	app-data directory (or NULL)
 *  param[out] common_desktop	common desktop directory (or NULL)
 *  param[out] common_appdata	common app-data directory (or NULL)
 *  param[out] installed 	is the program installed?
 *
 *  Returns 0 for success, -1 for an unrecoverable error.
 *  All returned directories end in '\'. 
 *  On Windows 98, common_desktop and common_appdata don't exist, so these are
 *  returned as empty strings.
 *
 * Uses the presence of CATF.EXE to decide if the program is installed or
 * not.  If not, appdata is returned as the cwd.
 */
int
get_dirs(char *argv0, char *appname, char **instdir, char **desktop,
	char **appdata, char **common_desktop, char **common_appdata,
	int *installed)
{
    	char **xappdata = appdata;
    	char **common_xappdata = common_appdata;
	int is_installed = FALSE;

	if (appdata != NULL || installed != NULL) {
	    	HMODULE h;

		h = LoadLibrary("CATF.EXE");
		if (h != NULL) {
		    	FreeLibrary(h);
			is_installed = TRUE;
		} else {
		    	is_installed = FALSE;
		}
		if (installed != NULL)
		    	*installed = is_installed;
	}

	/*
	 * Use arg0 and GetFullPathName() to figure out the installation
	 * directory.
	 */
	if (instdir != NULL) {
	    	char *bsl;
		char *tmp_instdir;
		DWORD rv;

		bsl = strrchr(argv0, '\\');
		if (bsl != NULL) {
		    	/* argv0 contains a path. */
		    	tmp_instdir = malloc(strlen(argv0) + 1);
			if (tmp_instdir == NULL)
			    	return -1;
			strcpy(tmp_instdir, argv0);
			if (bsl - argv0 > 0 &&
				    tmp_instdir[bsl - argv0 - 1] == ':')
			    	/* X:\foo */
			    	tmp_instdir[bsl - argv0 + 1] = '\0';
			else
			    	/* X:\foo\bar */
			    	tmp_instdir[bsl - argv0] = '\0';

			rv = GetFullPathName(tmp_instdir, 0, NULL, NULL);
			*instdir = malloc(rv + 2);
			if (*instdir == NULL)
				return -1;
			if (GetFullPathName(tmp_instdir, rv + 1, *instdir,
				    NULL) == 0)
				return -1;
			free(tmp_instdir);

			/* Make sure instdir ends in '\\'. */
			if ((*instdir)[strlen(*instdir) - 1] != '\\')
				strcat(*instdir, "\\");
		    } else {
		    	*instdir = getcwd_bsl();
			if (*instdir == NULL)
			    	return -1;
		}
	}

	/* If not installed, app-data and common app-data are cwd. */
	if (appdata != NULL && !is_installed) {
		*appdata = getcwd_bsl();
		if (*appdata == NULL)
			return -1;
		if (common_appdata != NULL) {
			*common_appdata = strdup(*appdata);
			if (*common_appdata == NULL) {
				return -1;
			}
		}

		/* Keep xxx_get_dirs() from resolving it below. */
		xappdata = NULL;
		common_xappdata = NULL;
	}

	if (desktop != NULL || xappdata != NULL) {
		OSVERSIONINFO info;
		char *wsl;

		/* Figure out what version of Windows this is. */
		memset(&info, '\0', sizeof(info));
		info.dwOSVersionInfoSize = sizeof(info);
		if (GetVersionEx(&info) == 0) {
			fprintf(stderr, "Can't get Windows version\n");
			return -1;
		}

		/* Ask Windows where the directories are. */
		if (new_get_dirs(desktop, xappdata, common_desktop,
			    common_xappdata) < 0) {
			return -1;
		}

		/* Append a trailing "\" to Desktop. */
		if (desktop != NULL &&
			(*desktop)[strlen(*desktop) - 1] != '\\') {

			wsl = malloc(strlen(*desktop) + 2);
			if (wsl == NULL)
			    	return -1;
			sprintf(wsl, "%s\\", *desktop);
			free(*desktop);
			*desktop = wsl;
		}

		/* Append the application name and trailing "\" to AppData. */
		if (xappdata != NULL) {
		    	size_t sl = strlen(*xappdata);

			wsl = malloc(sl + 1 + strlen(appname) + 2);
			if (wsl == NULL)
			    	return -1;

			sprintf(wsl, "%s\\%s\\", *xappdata, appname);
			free(*xappdata);
			*xappdata = wsl;

			/*
			 * Create the per-user AppData directory, in case the
			 * program was installed by a different user.
			 */
			_mkdir(*xappdata);
		}

		/* Append a trailing "\" to CommonDesktop. */
		if (common_desktop != NULL &&
			*common_desktop != NULL &&
			(*common_desktop)[strlen(*common_desktop) - 1]
			    != '\\') {

			wsl = malloc(strlen(*common_desktop) + 2);
			if (wsl == NULL)
			    	return -1;
			sprintf(wsl, "%s\\", *common_desktop);
			free(*common_desktop);
			*common_desktop = wsl;
		}

		/* Append the product name to CommonAppData. */
		if (common_xappdata != NULL && *common_xappdata != NULL) {
			size_t sl = strlen(*common_xappdata);
			int add_bsl = 0;

			if ((*common_xappdata)[sl - 1] != '\\') {
				add_bsl = 1;
			}

			wsl = malloc(sl + add_bsl + strlen(appname) + 2);
			if (wsl == NULL) {
				return -1;
			}
			sprintf(wsl, "%s%s%s\\",
				*common_xappdata,
				add_bsl? "\\": "",
				appname);
			_mkdir(wsl);

			free(*common_xappdata);
			*common_xappdata = wsl;
		}

	}

#if defined(DEBUG) /*[*/
	printf("get_dirs: instdir '%s', desktop '%s', appdata '%s', "
		"common_desktop '%s', common_appdata '%s'\n",
		instdir? *instdir: "(none)",
		desktop? *desktop: "(none)",
		appdata? *appdata: "(none)",
		common_desktop? *common_desktop: "(none)",
		common_appdata? *common_appdata: "(none)");
	printf("Enter...");
	fflush(stdout);
	(void) getchar();
#endif /*]*/

	return 0;
}
