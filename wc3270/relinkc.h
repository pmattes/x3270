/*
 * Copyright (c) 2006-2023 Paul Mattes.
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
 *	relinkc.h
 *		A Windows console-based 3270 Terminal Emulator
 *		Utility functions to read a session file and create a
 *		compatible shortcut.
 */

#define STR_SIZE	256

#define WIZARD_VER	5

typedef struct {
    	/* Fields for wc3270 3.3.9 (Wizard version 1) */
	char  session[STR_SIZE];	/* session name */
	char  host[STR_SIZE];		/* host name */
	DWORD port;			/* TCP port */
	char  luname[STR_SIZE];		/* LU name */
	DWORD tls;			/* TKS tunnel flag */
	char  proxy_type[STR_SIZE];	/* proxy type */
	char  proxy_host[STR_SIZE];	/*  proxy host */
	char  proxy_port[STR_SIZE];	/*  proxy port */
	DWORD model;			/* model number */
	char  codepage[STR_SIZE];	/* code page name */
	DWORD is_dbcs;
	DWORD wpr3287;			/* pr3287 flag */
	char  printerlu[STR_SIZE];	/*  printer LU */
	char  printer[STR_SIZE];	/*  Windows printer name */
	char  printercp[STR_SIZE];	/*  pr3287 code page */
	char  keymaps[STR_SIZE];	/* keymap names */

	/* Fields added for wc3270 3.3.10 (Wizard version 2) */
	unsigned char flags;		/* miscellaneous flags */
	unsigned char ov_rows;		/* oversize rows */
	unsigned char ov_cols;		/* oversize columns */
	unsigned char point_size;	/* font point size */
	unsigned char flags2;		/* more flags */

	/* Fields added for wc3270 4.0ga10 (Wizard version 3) */
	char  proxy_user[STR_SIZE];	/* proxy username */
	char  proxy_password[STR_SIZE];	/* proxy password */

	/* Fields added for wc3270 4.1alpha7 (Wizard version 4) */
	char  accept_hostname[STR_SIZE]; /* accept hostname */

	/* Fields added for wc3270 4.3alpha1 (Wizard version 5) */
	char  font_name[STR_SIZE];	/* font name */
	DWORD font_weight;		/* font weight */
} session_t;

#define WF_EMBED_KEYMAPS	0x01	/* embed keymaps in session */
#define WF_AUTO_SHORTCUT	0x02	/* 'auto-shortcut' mode */
#define WF_WHITE_BG		0x04	/* white background */
#define WF_NO_MENUBAR		0x08	/* don't leave room for menu bar */
#define WF_VERIFY_HOST_CERTS	0x10	/* verify host certificate
					   (deprecated) */
#define WF_TRACE		0x20	/* trace at start-up */
#define WF_CROSSHAIR		0x40	/* crosshair cursor */
#define WF_ALTCURSOR		0x80	/* underscore cursor */

#define WF2_NEW_VHC_DEFAULT	0x01	/* has new verify-host-cert default */
#define WF2_NO_VERIFY_HOST_CERT	0x02	/* do not verify host certificate */
#define WF2_ALWAYS_INSERT	0x04	/* always use insert mode */
#define WF2_CURSOR_BLINK	0x08	/* cursor blink */

typedef struct {
	char *name;
	char *hostcp;
	int is_dbcs;
	wchar_t *codepage;
} codepages_t;
extern codepages_t codepages[];
extern size_t num_codepages;
extern int wrows[6];
extern int wcols[6];

extern int read_user_settings(FILE *f, char **usp);
extern int read_session(FILE *f, session_t *s, char **usp);
extern HRESULT create_shortcut(session_t *session, char *exepath,
	char *linkpath, char *args, char *workingdir);
extern const wchar_t *reg_font_from_host_codepage(const char *font_name, const char *codepage_name, int *codepage,
	int (*err)(const char *string, ...));
