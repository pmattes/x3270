/*
 * Copyright (c) 2006-2009, 2014-2016 Paul Mattes.
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

#include "windirs.h"

/* Locate the desktop and appdata directories via the SHGetFolderPath API. */
static int
get_dirs_shfp(char **desktop, char **appdata, char **common_desktop,
	char **common_appdata)
{
    HRESULT r;

    if (desktop != NULL) {
	*desktop = malloc(MAX_PATH);
	if (*desktop == NULL) {
	    return -1;
	}
	r = SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
		SHGFP_TYPE_CURRENT, *desktop);
	if (r != S_OK) {
	    fprintf(stderr, "SHGetFolderPath(DESKTOPDIRECTORY) failed: 0x%x\n",
		    (int)r);
	    return -1;
	}
    }

    if (appdata != NULL) {
	*appdata = malloc(MAX_PATH);
	if (*appdata == NULL) {
	    return -1;
	}
	r = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
		*appdata);
	if (r != S_OK) {
	    fprintf(stderr, "SHGetFolderPath(APPDATA) failed: 0x%x\n", (int)r);
	    return -1;
	}
    }

    if (common_desktop != NULL) {
	*common_desktop = malloc(MAX_PATH);
	if (*common_desktop == NULL) {
	    return -1;
	}
	r = SHGetFolderPath(NULL, CSIDL_COMMON_DESKTOPDIRECTORY,
	    NULL, SHGFP_TYPE_CURRENT, *common_desktop);
	if (r != S_OK) {
	    fprintf(stderr, "SHGetFolderPath(COMMON_DESKTOPDIRECTORY) failed: "
		    "0x%x\n", (int)r);
	    return -1;
	}
    }

    if (common_appdata != NULL) {
	*common_appdata = malloc(MAX_PATH);
	if (*common_appdata == NULL) {
	    return -1;
	}
	r = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
		SHGFP_TYPE_CURRENT, *common_appdata);
	if (r != S_OK) {
	    fprintf(stderr, "SHGetFolderPath(COMMON_APPDATA) failed: 0x%x\n",
		    (int)r);
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
	if (xwd == NULL) {
	    return NULL;
	}

	strcpy(xwd, wd);
	strcat(xwd, "\\");
	free(wd);
	wd = xwd;
    }
    return wd;
}

/**
 * Locate the installation, desktop and app-data directories.
 * Return them in malloc'd buffers, all with trailing backslashes.
 * Also return a flag indicating that the program was installed.
 * If returning AppData and the program is installed, make sure that the
 * directory exists.
 *
 * @param[in]  appname	 	application name (for app-data)
 * @param[out] instdir	 	installation directory (or NULL)
 * @param[out] desktop	 	desktop directory (or NULL)
 * @param[out] appdata	 	app-data directory (or NULL)
 * @param[out] common_desktop	common desktop directory (or NULL)
 * @param[out] common_appdata	common app-data directory (or NULL)
 * @param[out] documents	My Documents directory (or NULL)
 * @param[out] common_docunents	common Documents directory (or NULL)
 * @param[out] docs3270		My Documents\{appname} directory (or NULL)
 * @param[out] common_docs3270	common Documents\{appname} directory (or NULL)
 * @param[out] flags 		Is the program installed? Does catf,exe exist?
 *
 * @returns true for success, false for an unrecoverable error.
 *
 * All returned directories end in '\'. 
 *
 * Uses the presence of CATF.EXE to decide if the program is installed or
 * not.  If not, appdata is returned as the cwd.
 */
bool
get_dirs(char *appname, char **instdir, char **desktop, char **appdata,
	char **common_desktop, char **common_appdata, char **documents,
	char **common_documents, char **docs3270, char **common_docs3270,
	unsigned *flags)
{
    char **xappdata = appdata;
    char **common_xappdata = common_appdata;
    bool is_installed = false;
    HRESULT r;
    char *d, *cd;
    HKEY key;
    HMODULE h;

    if (flags != NULL) {
	*flags = 0;
    }

    /* Check for the registry key to see if we are installed. */
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, ".wc3270", 0, KEY_READ, &key)
	    == ERROR_SUCCESS) {
	RegCloseKey(key);
	if (flags != NULL) {
	    *flags |= GD_INSTALLED;
	}
	is_installed = true;
    }

    /* Check for CATF.EXE. */
    h = LoadLibrary("CATF.EXE");
    if (h != NULL) {
	FreeLibrary(h);
	if (flags != NULL) {
	    *flags |= GD_CATF;
	}
    }

    /*
     * Use arg0 and GetFullPathName() to figure out the installation
     * directory.
     *
     * When the Session Wizard is run from the normal install directory, this
     * will produce the desired result. In no-install mode, this will also do
     * the right thing, assuming that they put wc3270.exe and wc3270wiz.exe
     * in the same place.
     *
     * The danger is if someone copies the Wizard somewhere different. We will
     * end up pointing at that directory.
     *
     * I can't use CSIDL_PROGRAMFILES, because the user can override it in the
     * installer.
     */
    if (instdir != NULL) {

	/* Get the pathname of this program. */
	HMODULE hModule = GetModuleHandle(NULL);
	char path[MAX_PATH];
	char *bsl;

	GetModuleFileName(hModule, path, MAX_PATH);
	CloseHandle(hModule);

	/* Chop it off after the last backslash. */
	bsl = strrchr(path, '\\');
	if (bsl == NULL) {
	    /* Should not happen. */
	    *instdir = getcwd_bsl();
	    if (*instdir == NULL) {
		return false;
	    }
	} else {
	    *(bsl + 1) = '\0';
	    *instdir = malloc(strlen(path) + 1);
	    if (*instdir == NULL) {
		return false;
	    }
	    strcpy(*instdir, path);
	}
    }

    /* If not installed, app-data and common app-data are cwd. */
    if (!is_installed) {
	if (appdata != NULL) {
	    *appdata = getcwd_bsl();
	    if (*appdata == NULL) {
		return false;
	    }
	}
	if (common_appdata != NULL) {
	    *common_appdata = getcwd_bsl();
	    if (*common_appdata == NULL) {
		return false;
	    }
	}

	/* Keep get_dirs_shfp() from resolving it below. */
	xappdata = NULL;
	common_xappdata = NULL;
    }

    if (desktop != NULL || xappdata != NULL) {
	char *wsl;

	/* Ask Windows where the directories are. */
	if (get_dirs_shfp(desktop, xappdata, common_desktop,
		    common_xappdata) < 0) {
	    return false;
	}

	/* Append a trailing "\" to Desktop. */
	if (desktop != NULL && (*desktop)[strlen(*desktop) - 1] != '\\') {
	    wsl = malloc(strlen(*desktop) + 2);
	    if (wsl == NULL) {
		return false;
	    }
	    sprintf(wsl, "%s\\", *desktop);
	    free(*desktop);
	    *desktop = wsl;
	}

	/* Append the application name and trailing "\" to AppData. */
	if (xappdata != NULL) {
	    size_t sl = strlen(*xappdata);

	    wsl = malloc(sl + 1 + strlen(appname) + 2);
	    if (wsl == NULL) {
		return false;
	    }

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
	if (common_desktop != NULL && *common_desktop != NULL &&
		(*common_desktop)[strlen(*common_desktop) - 1] != '\\') {

	    wsl = malloc(strlen(*common_desktop) + 2);
	    if (wsl == NULL) {
		return false;
	    }
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
		return false;
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

    /* Get the Documents directories. */

    if (documents != NULL || docs3270 != NULL) {
	d = malloc(MAX_PATH + 1);
	if (d == NULL) {
	    return false;
	}
	r = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, d);
	if (r != S_OK) {
	    free(d);
	    d = NULL;
	} else {
	    strcat(d, "\\");
	}
	if (documents != NULL) {
	    *documents = d;
	}
    }
    if (common_documents != NULL || common_docs3270 != NULL) {
	cd = malloc(MAX_PATH);
	if (cd == NULL) {
	    return false;
	}
	r = SHGetFolderPath(NULL, CSIDL_COMMON_DOCUMENTS, NULL,
		SHGFP_TYPE_CURRENT, cd);
	if (r != S_OK) {
	    free(cd);
	    cd = NULL;
	} else {
	    strcat(cd, "\\");
	}
	if (common_documents != NULL) {
	    *common_documents = cd;
	}
    }
    if (d != NULL && docs3270 != NULL) {
	size_t sl = strlen(d) + strlen(appname) + 2;

	*docs3270 = malloc(sl);
	if (*docs3270 == NULL) {
	    return false;
	}
	snprintf(*docs3270, sl, "%s%s\\", d, appname);
    }
    if (cd != NULL && common_docs3270 != NULL) {
	size_t sl = strlen(cd) + strlen(appname) + 2;
	*common_docs3270 = malloc(sl);
	if (*common_docs3270 == NULL) {
	    return false;
	}
	snprintf(*common_docs3270, sl, "%s%s\\", cd, appname);
    }

#if defined(DEBUG) /*[*/
    printf("get_dirs: instdir '%s', desktop '%s', appdata '%s', "
	    "common_desktop '%s', common_appdata '%s' "
	    "documents '%s', common_documents '%s' "
	    "docs3270 '%s', common_docs3270 '%s'\n",
	    instdir? *instdir: "(none)",
	    desktop? *desktop: "(none)",
	    appdata? *appdata: "(none)",
	    common_desktop? *common_desktop: "(none)",
	    common_appdata? *common_appdata: "(none)",
	    documents? *documents: "(none)",
	    common_documents? *common_documents: "(none)",
	    docs3270? *docs3270: "(none)",
	    common_docs3270? *common_docs3270: "(none)");
    printf("Enter...");
    fflush(stdout);
    (void) getchar();
#endif /*]*/

    return true;
}
