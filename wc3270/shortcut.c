/*
 * Copyright (c) 1996-2009, 2015-2017 Paul Mattes.
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
 *	shortcut.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Shell link creation
 */

#if !defined(_WIN32) /*[*/
# error For Windows only.
#endif /*]*/

#include "globals.h"

#include "shortcutc.h"
#include "winvers.h"

#if !defined EXP_PROPERTYSTORAGE_SIG /*[*/
# define EXP_PROPERTYSTORAGE_SIG 0xa0000009
#endif /*]*/

#define MASK16	0xffff

/*
 * An opaque EXP_PROPERTYSTORAGE block that has "Ctrl key shortcuts" disabled.
 */
static unsigned short new9block[] = {
    0x0150, 0x0000, 0x0009, 0xa000, 0x0089, 0x0000, 0x5331, 0x5350,
    0x8ae2, 0x4658, 0x4cbc, 0x4338, 0xfcbb, 0x9313, 0x9826, 0xce6d,
    0x006d, 0x0000, 0x0004, 0x0000, 0x1f00, 0x0000, 0x2d00, 0x0000,
    0x5300, 0x2d00, 0x3100, 0x2d00, 0x3500, 0x2d00, 0x3200, 0x3100,
    0x2d00, 0x3800, 0x3300, 0x3400, 0x3400, 0x3900, 0x3900, 0x3600,
    0x3400, 0x2d00, 0x3100, 0x3800, 0x3200, 0x3100, 0x3900, 0x3100,
    0x3900, 0x3700, 0x3600, 0x3300, 0x2d00, 0x3200, 0x3100, 0x3000,
    0x3600, 0x3900, 0x3500, 0x3300, 0x3200, 0x3700, 0x3700, 0x2d00,
    0x3100, 0x3000, 0x3000, 0x3100, 0x0000, 0x0000, 0x0000, 0x0000,
    0x8200, 0x0000, 0x3100, 0x5053, 0x0753, 0x5706, 0x960c, 0xde03,
    0x9d43, 0xe361, 0xd721, 0x50df, 0x1126, 0x0000, 0x0300, 0x0000,
    0x0000, 0x000b, 0x0000, 0xffff, 0x0000, 0x0011, 0x0000, 0x0001,
    0x0000, 0x0b00, 0x0000, 0xff00, 0x00ff, 0x1100, 0x0000, 0x0200,
    0x0000, 0x0000, 0x000b, 0x0000, 0xffff, 0x0000, 0x0011, 0x0000,
    0x0004, 0x0000, 0x0b00, 0x0000, 0xff00, 0x00ff, 0x1100, 0x0000,
    0x0600, 0x0000, 0x0000, 0x0002, 0x0000, 0x00ff, 0x0000, 0x0011,
    0x0000, 0x0005, 0x0000, 0x0b00, 0x0000, 0xff00, 0x00ff, 0x0000,
    0x0000, 0x3900, 0x0000, 0x3100, 0x5053, 0xb153, 0x6d16, 0xad44,
    0x708d, 0xa748, 0x4048, 0xa42e, 0x783d, 0x1d8c, 0x0000, 0x6800,
    0x0000, 0x0000, 0x0048, 0x0000, 0xf10d, 0x4e87, 0x0000, 0x0000,
    0x0000, 0x0660, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void substitute9block(const char *path_link);

/*
 * create_link - uses the shell's IShellLink and IPersistFile interfaces to
 * create and store a shortcut to the specified object.
 * Returns the result of calling the member functions of the interfaces.
 *  path_obj - address of a buffer containing the path of the object
 *  path_link - address of a buffer containing the path where the shell link
 *   is to be stored
 *  desc - address of a buffer containing the description of the shell link
 */
HRESULT
create_link(LPCSTR path_obj, LPSTR path_link, LPSTR desc, LPSTR args,
	LPSTR dir, int rows, int cols, wchar_t *font, int pointsize,
	int codepage)
{
    HRESULT		hres;
    int	 		initialized;
    IShellLink*		psl = NULL; 
    IShellLinkDataList*	psldl = NULL; 
    IPersistFile*	ppf = NULL;
    NT_CONSOLE_PROPS	p;
    WORD		wsz[MAX_PATH];

    hres = CoInitialize(NULL);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: CoInitialize failed: %ld\n", hres);
	goto out;
    }
    initialized = 1;

    /* Get a pointer to the IShellLink interface. */
    hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
	    &IID_IShellLink, (LPVOID *)&psl);

    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: CoCreateInstance failed: %ld\n", hres);
	goto out;
    }

    /* Set the path to the shortcut target, and add the description. */
    psl->lpVtbl->SetPath(psl, path_obj);
    if (desc) {
	psl->lpVtbl->SetDescription(psl, desc);
    }
    if (args) {
	psl->lpVtbl->SetArguments(psl, args);
    }
    if (dir) {
	psl->lpVtbl->SetWorkingDirectory(psl, dir);
    }

    /* Add the icon. */
    psl->lpVtbl->SetIconLocation(psl, path_obj, 0);

    hres = psl->lpVtbl->QueryInterface(psl, &IID_IShellLinkDataList,
	    (void **)&psldl);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: QueryInterface(DataList) failed: %ld\n",
		hres);
	goto out;
    }

    memset(&p, '\0', sizeof(NT_CONSOLE_PROPS));
    p.cbSize = sizeof(p);
    p.dwSignature = NT_CONSOLE_PROPS_SIG;
    p.wFillAttribute = 7;		/* ? */
    p.wPopupFillAttribute = 245;	/* ? */
    p.dwScreenBufferSize.X = cols;
    p.dwScreenBufferSize.Y = 0x012c;
    p.dwWindowSize.X = cols;
    p.dwWindowSize.Y = rows;
    p.dwWindowOrigin.X = 0;
    p.dwWindowOrigin.Y = 0;
    p.nFont = 0;
    p.nInputBufferSize = 0;
    p.dwFontSize.X = 0;
    p.dwFontSize.Y = pointsize? pointsize: 12;
    p.uFontFamily = 0;			/* FF_DONTCARE */
    p.uFontWeight = 400;		/* FW_NORMAL */
    wcsncpy(p.FaceName, font, LF_FACESIZE - 1);
    p.FaceName[LF_FACESIZE - 1] = 0;
    p.uCursorSize = 100;
    p.bFullScreen = 0;
    p.bQuickEdit = 0;
    p.bInsertMode = 1;
    p.bAutoPosition = 1;
    p.uHistoryBufferSize = 0x32;
    p.uNumberOfHistoryBuffers = 4;
    p.bHistoryNoDup = 0;
    p.ColorTable[0] = 0;
    p.ColorTable[1] =  0x00800000;
    p.ColorTable[2] =  0x00008000;
    p.ColorTable[3] =  0x00808000;
    p.ColorTable[4] =  0x00000080;
    p.ColorTable[5] =  0x00800080;
    p.ColorTable[6] =  0x00008080;
    p.ColorTable[7] =  0x00c0c0c0;
    p.ColorTable[8] =  0x00808080;
    p.ColorTable[9] =  0x00ff8000;
    p.ColorTable[10] = 0x0000ff00;
    p.ColorTable[11] = 0x00ffff00;
    p.ColorTable[12] = 0x000a0adc;
    p.ColorTable[13] = 0x00ff00ff;
    p.ColorTable[14] = 0x0000ffff;
    p.ColorTable[15] = 0x00ffffff;

    hres = psldl->lpVtbl->AddDataBlock(psldl, &p);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: AddDataBlock(NT_CONSOLE_PROPS) failed: "
		"%ld\n", hres);
	goto out;
    }

    if (codepage) {
	NT_FE_CONSOLE_PROPS pfe;

	memset(&pfe, '\0', sizeof(pfe));
	pfe.cbSize = sizeof(pfe);
	pfe.dwSignature = NT_FE_CONSOLE_PROPS_SIG;
	pfe.uCodePage = codepage;

	hres = psldl->lpVtbl->AddDataBlock(psldl, &pfe);
	if (!SUCCEEDED(hres)) {
	    fprintf(stderr, "create_link: AddDataBlock(NT_FE_CONSOLE_PROPS) "
		    "failed: %ld\n", hres);
	    goto out;
	}
    }

    /*
     * Query IShellLink for the IPersistFile interface for saving the
     * shortcut in persistent storage.
     */
    hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf);

    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: QueryInterface(Persist) failed: %ld\n",
		hres);
	goto out;
    }

    /* Ensure that the string is ANSI. */
    MultiByteToWideChar(CP_ACP, 0, path_link, -1, wsz, MAX_PATH);

    /* Save the link by calling IPersistFile::Save. */
    hres = ppf->lpVtbl->Save(ppf, wsz, TRUE);
    if (!SUCCEEDED(hres)) {
	fprintf(stderr, "create_link: Save failed: %ld\n", hres);
	goto out;
    }

out:
    if (ppf != NULL) {
	ppf->lpVtbl->Release(ppf);
    }
    if (psldl != NULL) {
	psldl->lpVtbl->Release(psldl);
    }
    if (psl != NULL) {
	psl->lpVtbl->Release(psl);
    }

    if (initialized) {
	CoUninitialize();
    }

    if (SUCCEEDED(hres) && IsWindowsVersionOrGreater(10, 0, 0)) {
	substitute9block(path_link);
    }

    return hres;
} 

/*
 * Substitute the opaque EXP_PROPERTYSTORAGE block in a completed link with
 * one that disables Windows 10 "Ctrl key shortcuts".
 *
 * Obviously this is a hack, owing to the lack of documentation of the opaque
 * block, and the apparent lack of any other way to accomplish this.
 */
static void
substitute9block(const char *path_link)
{
#   define WINDOW	4
    char temp_path[MAX_PATH];
    FILE *f, *g;
    unsigned short window[WINDOW];
    int head = 0;
    int tail = 0;
    int count = 0;
    bool matched = false;

    /* Create the temporary name. */
    strcpy(temp_path, path_link);
    strcat(temp_path, ".tmp");

    /* Open the existing link. */
    f = fopen(path_link, "rb+");
    if (f == NULL) {
	fprintf(stderr, "substitute9block: Re-open of link '%s' failed: %s\n",
		path_link, strerror(errno));
	return;
    }

    /* Open the temporary link. */
    g = fopen(temp_path, "wb");
    if (g == NULL) {
	fclose(f);
	fprintf(stderr, "substitute9block: Open of temporary link '%s' "
		"failed: %s\n", temp_path, strerror(errno));
	return;
    }

    /*
     * Read until we get to the two-DWORD sequence:
     *  xxxxyyyy a0000009
     * xxxxyyyy is the length.
     * a0000009 identifies the EXP_PROPERTYSTORAGE block.
     *
     * The file is 16-bit (but not 32-bit) aligned, so we need to read two
     * bytes at a time.
     *
     * Store the chunks in a 4-element array. As we read new value into the
     * tail, write out the head.
     */
    for (;;) {
	unsigned skip_short;
	unsigned short skip_buf;

	/* Write out the head. */
	if (count == WINDOW) {
	    if (fwrite(&window[head], sizeof(unsigned short), 1, g) != 1) {
		fprintf(stderr, "substitute9block: Write/copy to temp link "
			"failed: %s\n", strerror(errno));
	    }
	    head = (head + 1) % WINDOW;
	}

	/* Read into the tail. */
	if (fread(&window[tail], sizeof(unsigned short), 1, f) != 1) {
	    /* All done. */
	    break;
	}
	if (++count > WINDOW) {
	    count = WINDOW;
	}
	tail = (tail + 1) % WINDOW;

	/* Check for a match. */
	if ((count != WINDOW) ||
		(window[(head + 2) % WINDOW] !=
		    (EXP_PROPERTYSTORAGE_SIG & MASK16)) ||
		(window[(head + 3) % WINDOW] !=
		    EXP_PROPERTYSTORAGE_SIG >> 16)) {
	    /* No match. */
	    continue;
	}

	/*
	 * A match. The two elements at the head are the length in bytes to
	 * skip, including what has already been read into the window.
	 */
	matched = true;
	skip_short = (((window[(head + 1) % WINDOW] << 16) + window[head])
		/ sizeof(unsigned short)) - WINDOW;
	while (skip_short--) {
	    fread(&skip_buf, sizeof(unsigned short), 1, f);
	}

	/* Substitute a different block. */
	if (fwrite(new9block, sizeof(new9block), 1, g) != 1) {
	    fprintf(stderr, "substitute9block: Write/subst to temp link "
		    "failed: %s\n", strerror(errno));
	}

	/* Keep on reading. */
	count = 0;
	head = 0;
	tail = 0;
    }
    fclose(f);

    /* Write out the remainder of the window. */
    while (count--) {
	if (fwrite(&window[head], sizeof(unsigned short), 1, g) != 1) {
	    fprintf(stderr, "substitute9block: Write/tail to temp link "
		    "failed: %s\n", strerror(errno));
	}
	head = (head + 1) % WINDOW;
    }
    fclose(g);

    /* Delete the old file and rename the temporary to that name. */
    if (unlink(path_link) < 0) {
	fprintf(stderr, "substitute9block: Unlink of original link failed: "
		"%s\n", strerror(errno));
    }
    if (rename(temp_path, path_link) < 0) {
	fprintf(stderr, "substitute9block: Rename of temp link failed: %s\n",
		strerror(errno));
    }

    if (!matched) {
	fprintf(stderr, "substitute9block: No match for EXP_PROPERTYSTORAGE "
		"block!\n");
    }
}
