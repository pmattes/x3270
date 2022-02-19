/*
 * Copyright (c) 2000-2022 Paul Mattes.
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
 *	snap.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Screen snapshot
 */

#include "globals.h"

#include "actions.h"
#include "cscreen.h"
#include "names.h"
#include "popups.h"
#include "snap.h"
#include "w3misc.h"

/*
 * Create a bitmap info struct.
 * This and the function below were adapted from:
 *  https://docs.microsoft.com/en-us/windows/win32/gdi/storing-an-image
 */
static PBITMAPINFO
create_bmp_info_struct(HBITMAP b)
{
    BITMAP bmp;
    PBITMAPINFO pbmi;
    WORD color_bits;

    /* Retrieve the bitmap color format, width, and height. */
    if (!GetObject(b, sizeof(BITMAP), (LPSTR)&bmp)) {
	popup_an_error("GetObject failed: %s", win32_strerror(GetLastError()));
	return NULL;
    }

    /* Convert the color format to a count of bits. */
    color_bits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
    if (color_bits == 1) {
	color_bits = 1;
    } else if (color_bits <= 4) {
	color_bits = 4;
    } else if (color_bits <= 8) {
	color_bits = 8;
    } else if (color_bits <= 16) {
	color_bits = 16;
    } else if (color_bits <= 24) {
	color_bits = 24;
    } else {
	color_bits = 32;
    }

    /* Allocate memory for the BITMAPINFO structure. */
    if (color_bits < 24) {
	pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
		sizeof(BITMAPINFOHEADER) +
		sizeof(RGBQUAD) * ((size_t)1 << color_bits));
    } else {
	pbmi = (PBITMAPINFO)LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER));
    }
    if (pbmi == NULL) {
	popup_an_error("LocalAlloc failed: %s",
		win32_strerror(GetLastError()));
	return NULL;
    }

    /* Initialize the fields in the BITMAPINFO structure. */
    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = bmp.bmWidth;
    pbmi->bmiHeader.biHeight = bmp.bmHeight;
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
    if (color_bits < 24) {
	pbmi->bmiHeader.biClrUsed = 1 << color_bits;
    }
    pbmi->bmiHeader.biCompression = BI_RGB;

    /*
     * Compute the number of bytes in the array of color
     * indices and store the result in biSizeImage.
     * The width must be DWORD aligned unless the bitmap is RLE
     * compressed.
     */
    pbmi->bmiHeader.biSizeImage =
	((pbmi->bmiHeader.biWidth * color_bits + 31) & ~31) / 8 *
	pbmi->bmiHeader.biHeight;
    /*
     * Set biClrImportant to 0, indicating that all of the device colors are
     * important.
     */
    pbmi->bmiHeader.biClrImportant = 0;
    return pbmi;
}

/* Save a bitmap into a file. */
static bool
create_bmp_file(HWND hwnd, LPTSTR file_name, PBITMAPINFO pbi, HBITMAP b,
	HDC dc)
{
    HANDLE f = INVALID_HANDLE_VALUE; /* file handle */
    BITMAPFILEHEADER hdr;     /* bitmap file header */
    PBITMAPINFOHEADER pbih;   /* bitmap info header */
    LPBYTE bits = NULL;       /* memory pointer */
    DWORD cb;                 /* incremental count of bytes */
    BYTE *hp;                 /* byte pointer */
    DWORD written;            /* bytes written */
    int quad_size;	      /* size of RGBQUAD array */
    bool rv = false;          /* return value */

    pbih = (PBITMAPINFOHEADER)pbi;
    bits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
    if (bits == NULL) {
	popup_an_error("GlobalAlloc failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }

    /*
     * Retrieve the color table (RGBQUAD array) and the bits
     * (array of palette indices) from the DIB.
     */
    if (!GetDIBits(dc, b, 0, (WORD)pbih->biHeight, bits, pbi,
		DIB_RGB_COLORS)) {
	popup_an_error("GetDIBits failed: %s", win32_strerror(GetLastError()));
	goto done;
    }

    /* Set up the header. */
    quad_size = (int)(pbih->biClrUsed * sizeof(RGBQUAD));
    hdr.bfType = 0x4d42; /* 0x42 = "B" 0x4d = "M" */
    hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + pbih->biSize + quad_size +
	    pbih->biSizeImage);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + pbih->biSize + quad_size);

    /* Create the file. */
    f = CreateFile(file_name,
	    GENERIC_READ | GENERIC_WRITE,
	    (DWORD)0,
	    NULL,
	    CREATE_ALWAYS,
	    FILE_ATTRIBUTE_NORMAL,
	    (HANDLE)NULL);
    if (f == INVALID_HANDLE_VALUE) {
	popup_an_error("CreateFile failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }

    /* Write it. */
    cb = pbih->biSizeImage;
    hp = bits;
    if (!WriteFile(f, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER),
		(LPDWORD)&written, NULL) ||
	!WriteFile(f, (LPVOID)pbih, sizeof(BITMAPINFOHEADER) + quad_size,
		(LPDWORD)&written, NULL) ||
	!WriteFile(f, (LPSTR)hp, (int)cb,
		(LPDWORD)&written, NULL)) {
	popup_an_error("WriteFile failed: %s", win32_strerror(GetLastError()));
	goto done;
    }

    /* Close it. */
    if (!CloseHandle(f)) {
	popup_an_error("CloseHandle failed: %s",
		win32_strerror(GetLastError()));
	f = INVALID_HANDLE_VALUE;
	goto done;
    }
    f = INVALID_HANDLE_VALUE;

    /* Success. */
    rv = true;

done:
    /* Clean up. */
    if (f != INVALID_HANDLE_VALUE) {
	CloseHandle(f);
    }
    if (bits != NULL) {
	GlobalFree((HGLOBAL)bits);
    }
    return rv;
}

/* Snap the screen into a .bmp file. */
bool
SnapScreen_action(ia_t ia, unsigned argc, const char** argv)
{
    size_t sl;
    bool ret = false;
    HDC dc = NULL;
    HDC target_dc = NULL;
    RECT rect = { 0 };
    HBITMAP bitmap = NULL;
    PBITMAPINFO pbmi = NULL;

    /* Check arguments. */
    action_debug(AnSnapScreen, ia, argc, argv);
    if (check_argc(AnSnapScreen, argc, 1, 1) < 0) {
	return false;
    }
    sl = strlen(argv[0]);
    if (sl < 5 || strcasecmp(argv[0] + sl - 4, ".bmp")) {
	popup_an_error(AnSnapScreen "(): argument must end with .bmp");
	return false;
    }

    /* Grab a bitmap from the window. */
    dc = GetDC(console_window);
    if (dc == NULL) {
	popup_an_error(AnSnapScreen "(): GetDC failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }
    target_dc = CreateCompatibleDC(dc);
    if (target_dc == NULL) {
	popup_an_error(AnSnapScreen "(): CreateCompatibleDC failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }
    if (!GetWindowRect(console_window, &rect)) {
	popup_an_error(AnSnapScreen "(): GetWindowRect failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }
    bitmap = CreateCompatibleBitmap(dc, rect.right - rect.left,
	rect.bottom - rect.top);
    if (bitmap == NULL) {
	popup_an_error(AnSnapScreen "(): CreateCompatibleBitmap failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }
    SelectObject(target_dc, bitmap);
    if (!PrintWindow(console_window, target_dc, 0 /*PW_CLIENTONLY*/))
    {
	popup_an_error(AnSnapScreen "(): PrintWindow failed: %s",
		win32_strerror(GetLastError()));
	goto done;
    }

    /* Save it to a file. */
    pbmi = create_bmp_info_struct(bitmap);
    if (pbmi == NULL) {
	goto done;
    }
    if (!create_bmp_file(console_window, (char *)argv[0], pbmi, bitmap,
		target_dc)) {
	goto done;
    }

    /* Success. */
    ret = true;

done:
    if (bitmap != NULL) {
	DeleteObject(bitmap);
    }
    if (dc != NULL) {
	ReleaseDC(console_window, dc);
    }
    if (target_dc != NULL) {
	DeleteDC(target_dc);
    }
    if (pbmi != NULL) {
	LocalFree(pbmi);
    }
    return ret;
}
