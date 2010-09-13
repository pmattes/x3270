/*
 * Copyright (c) 2006-2009, Paul Mattes.
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

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <direct.h>

#include "windirsc.h"


/*
 * If Win2K or later, use SHGetFoldersA from shell32.dll.
 * Otherwise, use the function below.
 */

/* Locate the desktop and appdata directories from the Windows registry. */
static int
old_get_dirs(char **desktop, char **appdata)
{
	HRESULT hres;
	HKEY hkey;
	DWORD index;

	/* Get some paths from Windows. */
	hres = RegOpenKeyEx(HKEY_CURRENT_USER,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
    		0, KEY_QUERY_VALUE, &hkey);
	if (hres != ERROR_SUCCESS) {
	    	printf("Sorry, I can't figure out where your Desktop or "
			"Application Data directories are, Windows error "
			"%ld.\n", hres);
		return -1;
	}

	if (desktop != NULL) {
	    	*desktop = malloc(MAX_PATH);
		if (*desktop == NULL)
		    	return -1;
		(*desktop)[0] = '\0';
	}
	if (appdata != NULL) {
	    	*appdata = malloc(MAX_PATH);
		if (*appdata == NULL)
		    	return -1;
		(*appdata)[0] = '\0';
	}

	/*
	 * Iterate to find Desktop and AppData.
	 * We can't just go for them individually, because we can't use
	 * ReqQueryValueEx on Win98, and ReqQueryValue doesn't work.
	 */
	for (index = 0; ; index++) {
		char name[MAX_PATH];
		DWORD nlen = MAX_PATH;
		char value[MAX_PATH];
		DWORD vlen = MAX_PATH;
		DWORD type;

		hres = RegEnumValue(hkey, index, name, &nlen, 0, &type,
			(unsigned char *)value, &vlen);
		if (hres != ERROR_SUCCESS)
			break;

		if (desktop != NULL && !strcmp(name, "Desktop"))
		    	strcpy(*desktop, value);
		else if (appdata != NULL && !strcmp(name, "AppData"))
		    	strcpy(*appdata, value);

		if ((desktop == NULL || (*desktop)[0]) &&
		    (appdata == NULL || (*appdata)[0]))
		    	break;

	}
	RegCloseKey(hkey);

	if ((desktop != NULL && !(*desktop)[0]) ||
	    (appdata != NULL && !(*appdata)[0])) {

	    	printf("Sorry, I can't figure out where your Desktop or "
			"Application Data directories are.\n");
		return -1;
	}

	return 0;
}

/*
 * dll_SHGetFolderPath explicitly pulls SHGetFolderPathA out of shell32.dll,
 * so we won't get link errors on Win98.
 */

static HRESULT
dll_SHGetFolderPath(HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags,
	LPTSTR pszPath)
{
    	static HMODULE handle = NULL;
	static FARPROC p = NULL;
	typedef HRESULT (__stdcall *sgfp_fn)(HWND, int, HANDLE, DWORD, LPSTR);

	if (handle == NULL) {
	    	handle = LoadLibrary("shell32.dll");
		if (handle == NULL) {
		    	fprintf(stderr, "Cannot find shell32.dll\n");
			return E_FAIL;
		}
		p = GetProcAddress(handle, "SHGetFolderPathA");
		if (p == NULL) {
		    	fprintf(stderr, "Cannot find SHGetFolderPathA in "
				"shell32.dll\n");
			return E_FAIL;
		}
	}
	return ((sgfp_fn)p)(hwndOwner, nFolder, hToken, dwFlags, pszPath);
}

/* Locate the desktop and appdata directories via the SHGetFolderPath API. */
static int
new_get_dirs(char **desktop, char **appdata)
{
    	HRESULT r;

	if (desktop != NULL) {
	    	*desktop = malloc(MAX_PATH);
		if (*desktop == NULL)
		    	return -1;
		r = dll_SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
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
		r = dll_SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
			SHGFP_TYPE_CURRENT, *appdata);
		if (r != S_OK) {
			printf("SHGetFolderPath(APPDATA) failed: 0x%x\n",
				(int)r);
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
 *  param[in]  argv0	 program's argv[0]
 *  param[in]  appname	 application name (for app-data)
 *  param[out] instdir	 installation directory (or NULL)
 *  param[out] desktop	 desktop directory (or NULL)
 *  param[out] appdata	 app-data directory (or NULL)
 *  param[out] installed is the program installed?
 *
 *  Returns 0 for success, -1 for an unrecoverable error.
 *  All returned directories end in '\'. 
 *
 * Uses the presence of CATF.EXE to decide if the program is installed or
 * not.  If not, appdata is returned as the cwd.
 */
int
get_dirs(char *argv0, char *appname, char **instdir, char **desktop,
	char **appdata, int *installed)
{
    	char **xappdata = appdata;
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

	/* If not installed, app-data is cwd. */
	if (appdata != NULL && !is_installed) {
		*appdata = getcwd_bsl();
		if (*appdata == NULL)
			return -1;

		/* Keep xxx_get_dirs() from resolving it below. */
		xappdata = NULL;
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

		if ((info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ||
			    (info.dwMajorVersion < 5)) {
			/* Use the registry. */
			if (old_get_dirs(desktop, xappdata) < 0)
				return -1;
		} else {
			/* Use the API. */
			if (new_get_dirs(desktop, xappdata) < 0)
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
			 * Create the AppData directory, in case the program
			 * was installed by a different user.
			 */
			_mkdir(*xappdata);
		}
	}

#if defined(DEBUG) /*[*/
	printf("get_dirs: instdir '%s', desktop '%s', appdata '%s'\n",
		instdir? *instdir: "(none)",
		desktop? *desktop: "(none)",
		appdata? *appdata: "(none)");
	printf("Enter...");
	fflush(stdout);
	(void) getchar();
#endif /*]*/

	return 0;
}
