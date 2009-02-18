/*
 * Copyright (c) 2006-2009, Paul Mattes.
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
 *	wizard.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Session creation wizard
 */

#include "globals.h"

#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#include <windows.h>
#include <direct.h>
#include <wincon.h>
#include <shlobj.h>
#include "shlobj_missing.h"

#include "winversc.h"
#include "shortcutc.h"
#include "windirsc.h"

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

#define STR_SIZE	256
#define LEGAL_CNAME	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcedfghijklmnopqrstuvwxyz" \
			"0123456789_- "

#define KEYMAP_SUFFIX	".wc3270km"
#define KS_LEN		strlen(KEYMAP_SUFFIX)

#define KM_3270		".3270"
#define LEN_3270	strlen(KM_3270)

#define KM_NVT		".nvt"
#define LEN_NVT		strlen(KM_NVT)

#define KM_DESC		"!description: "
#define LEN_DESC	strlen(KM_DESC)

#define WIZARD_VER	1

extern char *wversion;

struct {
    	char    *name;
	char    *hostcp;
	int      is_dbcs;
	wchar_t *codepage;
} charsets[] = {
	{ "belgian",		"500",	0, L"1252"	},
	{ "belgian-euro",	"1148",	0, L"1252"	},
	{ "bracket",		"37*",	0, L"1252"	},
	{ "brazilian",		"275",	0, L"1252"	},
	{ "cp1047",		"1047",	0, L"1252"	},
	{ "cp870",		"870",	0, L"1250"	},
	{ "chinese-gb18030",	"1388",	1, L"936"	},
	{ "finnish",		"278",	0, L"1252"	},
	{ "finnish-euro",	"1143",	0, L"1252"	},
	{ "french",		"297",	0, L"1252"	},
	{ "french-euro",	"1147",	0, L"1252"	},
	{ "german",		"273",	0, L"1252"	},
	{ "german-euro",	"1141",	0, L"1252"	},
	{ "greek",		"875",	0, L"1253"	},
	{ "hebrew",		"424",	0, L"1255"	},
	{ "icelandic",		"871",	0, L"1252"	},
	{ "icelandic-euro",	"1149",	0, L"1252"	},
	{ "italian",		"280",	0, L"1252"	},
	{ "italian-euro",	"1144",	0, L"1252"	},
	{ "japanese-kana",	"930",  1, L"932"	},
	{ "japanese-latin",	"939",  1, L"932"	},
	{ "norwegian",		"277",	0, L"1252"	},
	{ "norwegian-euro",	"1142",	0, L"1252"	},
	{ "russian",		"880",	0, L"1251"	},
	{ "simplified-chinese",	"935",  1, L"936"	},
	{ "spanish",		"284",	0, L"1252"	},
	{ "spanish-euro",	"1145",	0, L"1252"	},
	{ "thai",		"838",	0, L"1252"	},
	{ "traditional-chinese","937",	1, L"950"	},
	{ "turkish",		"1026",	0, L"1254"	},
	{ "uk",			"285",	0, L"1252"	},
	{ "uk-euro",		"1146",	0, L"1252"	},
	{ "us-euro",		"1140",	0, L"1252"	},
	{ "us-intl",		"37",	0, L"1252"	},
	{ NULL,			NULL,	0, NULL	}
};

/* Aliases for obsolete character set names. */
struct {
    	char	*alias;
	char	*real;
} charset_alias[] = {
    	{ "japanese-290",  "japanese-kana" },
    	{ "japanese-1027", "japanese-latin" },
	{ NULL, NULL }
};

#define CS_WIDTH	19
#define CP_WIDTH	8
#define WP_WIDTH	6
#define	CS_COLS		2

             /*  model  2   3   4   5 */
int wrows[6] = { 0, 0, 25, 33, 44, 28  };
int wcols[6] = { 0, 0, 80, 80, 80, 132 };

#define MAX_PRINTERS	256
PRINTER_INFO_1 printer_info[MAX_PRINTERS];
int num_printers = 0;
char default_printer[1024];

static struct {
    	char *name;
	char *description;
} builtin_keymaps[] = {
	{ "rctrl",	"Map PC Right Ctrl key to 3270 'Enter' and PC Enter key to 3270 'Newline'" },
	{ NULL,		NULL }
};

static struct {
    	char *name;
	char *protocol;
	char *port;
} proxies[] = {
    	{ "http",	"HTTP tunnel (RFC 2817, e.g., squid)",	"3128" },
	{ "passthru",	"Sun telnet-passthru",			NULL   },
	{ "socks4",	"SOCKS version 4",			"1080" },
	{ "socks5",	"SOCKS version 5 (RFC 1928)",		"1080" },
	{ "telnet",	"None (just send 'connect host port')",	NULL   },
	{ NULL,		NULL,					NULL   }
};

typedef struct {
	char session[STR_SIZE];		/* session name */
	char host[STR_SIZE];		/* host name */
	int  port;			/* TCP port */
	char luname[STR_SIZE];		/* LU name */
	int  ssl;			/* SSL tunnel flag */
	char proxy_type[STR_SIZE];	/* proxy type */
	char proxy_host[STR_SIZE];	/*  proxy host */
	char proxy_port[STR_SIZE];	/*  proxy port */
	int  model;			/* model number */
	char charset[STR_SIZE];		/* character set name */
	int  is_dbcs;
	int  wpr3287;			/* wpr3287 flag */
	char printerlu[STR_SIZE];	/*  printer LU */
	char printer[STR_SIZE];		/*  Windows printer name */
	char printercp[STR_SIZE];	/*  wpr3287 code page */
	char keymaps[STR_SIZE];		/* keymap names */
} session_t;

int create_session_file(session_t *s, char *path);
static int read_session(FILE *f, session_t *s);

static char mya[MAX_PATH];

char *user_settings = NULL;

char *
get_input(char *buf, int bufsize)
{
    	char *s;
	int sl;

	/* Make sure all of the output gets out. */
	fflush(stdout);

	/* Get the raw input from stdin. */
	if (fgets(buf, bufsize, stdin) == NULL)
	    	return NULL;

	/* Trim leading whitespace. */
	s = buf;
	sl = strlen(buf);
	while (*s && isspace(*s)) {
		s++;
		sl--;
	}
	if (s != buf)
		memmove(buf, s, sl + 1);

	/* Trim trailing whitespace. */
	while (sl && isspace(buf[--sl]))
		buf[sl] = '\0';

	return buf;
}

int
getyn(int defval)
{
	char yn[STR_SIZE];

	if (get_input(yn, STR_SIZE) == NULL) {
		return -1;
	}

	if (!yn[0])
		return defval;

	if (!strncasecmp(yn, "yes", strlen(yn)))
		return 1;
	if (!strncasecmp(yn, "no", strlen(yn)))
		return 0;

	printf("Please answer (y)es or (n)o.\n\n");
	return -2;
}

void
enum_printers(void)
{
	DWORD needed = 0;
	DWORD returned = 0;

	/* Get the default printer name. */
	default_printer[0] = '\0';
	if (GetProfileString("windows", "device", "", default_printer,
		    sizeof(default_printer)) != 0) {
		char *comma;

		if ((comma = strchr(default_printer, ',')) != NULL)
			*comma = '\0';
	}

	/* Get the list of printers. */
	if (EnumPrinters(
		    PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
		    NULL,
		    1,
		    (LPBYTE)&printer_info,
		    sizeof(printer_info),
		    &needed,
		    &returned) == 0)
	    return;

	num_printers = returned;
}

/* Get an 'other' printer name. */
int
get_printer_name(char *defname, char *printername)
{
	for (;;) {
		printf("\nEnter Windows printer name: [%s] ",
			defname[0]? defname: "use system default");
		fflush(stdout);
		if (get_input(printername, STR_SIZE) == NULL)
			return -1;
		if (!printername[0]) {
			if (defname[0])
			    	strcpy(printername, defname);
			break;
		}
		if (!strcmp(printername, "default")) {
		    	printername[0] = '\0';
		}
		if (strchr(printername, '!') ||
		    strchr(printername, ',')) {
			printf("Invalid printer name.\n");
			continue;
		} else {
			break;
		}
	}
	return 0;
}

typedef struct km {
	struct km *next;
    	char name[MAX_PATH];
	char description[STR_SIZE];
} km_t;
km_t *km_first = NULL;
km_t *km_last = NULL;

/* Save a keymap name.  If it is unique, return its node. */
km_t *
save_keymap_name(char *path, char *keymap_name)
{
	km_t *km;
    	int sl;
	km_t *kms;
	FILE *f;

	km = (km_t *)malloc(sizeof(km_t));
	if (km == NULL) {
	    	fprintf(stderr, "Not enough memory\n");
		return NULL;
	}
	strcpy(km->name, keymap_name);
	km->description[0] = '\0';
    	sl = strlen(km->name);

	if (sl > KS_LEN && !strcasecmp(km->name + sl - KS_LEN, KEYMAP_SUFFIX)) {
		km->name[sl - KS_LEN] = '\0';
		sl -= KS_LEN;
	}
	if (sl > LEN_3270 && !strcasecmp(km->name + sl - LEN_3270, KM_3270)) {
		km->name[sl - LEN_3270] = '\0';
		sl -= LEN_3270;
	} else if (sl > LEN_NVT &&
		    !strcasecmp(km->name + sl - LEN_NVT, KM_NVT)) {
		km->name[sl - LEN_NVT] = '\0';
		sl -= LEN_NVT;
	}

	for (kms = km_first; kms != NULL; kms = kms->next) {
	    	if (!strcasecmp(kms->name, km->name))
		    	break;
	}
	if (kms != NULL) {
	    	free(km);
		return NULL;
	}
	km->next = NULL;
	if (km_last != NULL)
	    	km_last->next = km;
	else
	    	km_first = km;
	km_last = km;

	/* Dig for a description. */
	if (path != NULL) {
		f = fopen(path, "r");
		if (f != NULL) {
			char buf[STR_SIZE];

			while (fgets(buf, STR_SIZE, f) != NULL) {
				sl = strlen(buf);
				if (sl > 0 && buf[sl - 1] == '\n')
					buf[--sl] = '\0';
				if (!strncasecmp(buf, KM_DESC, LEN_DESC)) {
					strncpy(km->description, buf + LEN_DESC,
						sl - LEN_DESC + 1);
					break;
				}
			}
			fclose(f);
		}
	}

	return km;
}

void
new_screen(session_t *s, char *title)
{
    	system("cls");
	printf(
"wc3270 Session Wizard                                            %s\n",
		wversion);
	if (s->session[0])
	    	printf("\nSession: %s\n", s->session);
	printf("\n%s\n", title);
}

/* Introductory screen. */
int
intro(session_t *s)
{
	int rc;

	new_screen(s, "\
Overview\n\
\n\
This wizard sets up a new wc3270 session, or allows you to modify an existing\n\
session.\n\
\n\
It creates or edits a session file in your wc3270 Application Data directory\n\
and can create or re-create a shortcut on your desktop.");

	for (;;) {
		printf("\nContinue? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}
	return 0;
}

/*
 * Session name screen.
 * Returns:  0 file does not exist
 *           1 file does exist and is editable, edit it
 *           2 file does exist and is editable, do not edit it
 *           3 file exists but is uneditable, overwrite it
 *          -1 bail, end of file
 *          -2 bail, uneditable and they don't want to overwrite it
 */
int
get_session(session_t *s, char *path)
{
    	FILE *f;
	int rc;
	int editable;

	/* Get the session name. */
	new_screen(s, "\
Session Name\n\
\n\
This is a unique name for the wc3270 session.  It is the name of the file\n\
containing the session configuration parameters and the name of the desktop\n\
shortcut.");
	for (;;) {
		printf("\nEnter session name: ");
		fflush(stdout);
		if (get_input(s->session, sizeof(s->session)) == NULL) {
			return -1;
		}
		if (!s->session[0])
			continue;
		if (strspn(s->session, LEGAL_CNAME) != strlen(s->session)) {
			fprintf(stdout, "\
\nIllegal character(s).\n\
Session names can only have letters, numbers, spaces, underscore '_'\n\
and dash '-')\n");
			continue;
		}

		break;
	}
	sprintf(path, "%s%s.wc3270", mya, s->session);
	f = fopen(path, "r");
	if (f != NULL) {
	    	editable = read_session(f, s);
		fclose(f);

		if (editable) {
			for (;;) {
				printf("\nSession '%s' exists.  Edit it? "
					"(y/n) [y] ", s->session);
				fflush(stdout);
				rc = getyn(1);
				if (rc == -1)
					return -1;
				if (rc == 0)
					return 2; /* do not edit */
				if (rc == 1)
					return 1; /* edit it */
			}
		} else {
			for (;;) {
				printf("\nSession '%s' already exists but "
					"cannot be edited.  Replace it? "
					"(y/n) [n] ", s->session);
				fflush(stdout);
				rc = getyn(0);
				if (rc == -1)
					return -1;
				if (rc == 0)
					return -2; /* don't overwrite */
				if (rc == 1)
					return 3; /* overwrite */
			}
		}
	} else {
	    	return 0; /* create it */
	}
}

int
get_host(session_t *s)
{
    	char buf[STR_SIZE];
        OSVERSIONINFO info;
	int has_ipv6 = 1;

	/* Win2K and earlier is IPv4-only.  WinXP and later can have IPv6. */
	memset(&info, '\0', sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	if (GetVersionEx(&info) == 0 ||
		info.dwMajorVersion < 5 ||
		(info.dwMajorVersion == 5 && info.dwMinorVersion < 1)) {
	    has_ipv6 = 0;
	}

#define COMMON_HOST_TEXT1 "\
Host Name\n\
\n\
This specifies the IBM host to connect to.  It can be a symbolic name like\n\
'foo.company.com'"

#define COMMON_HOST_TEXT2 "\
an IPv4 address in dotted-decimal notation such as\n\
'1.2.3.4'"

#define IPV6_HOST_TEXT "\
an IPv6 address in colon notation, such as 'fec0:0:0:1::27'"

	if (has_ipv6)
	    	new_screen(s, COMMON_HOST_TEXT1 ", " COMMON_HOST_TEXT2 " or "
			IPV6_HOST_TEXT ".");
	else
	    	new_screen(s, COMMON_HOST_TEXT1 " or " COMMON_HOST_TEXT2 ".");

	for (;;) {
		if (s->host[0])
			printf("\nEnter host name or IP address: [%s] ",
				s->host);
		else
			printf("\nEnter host name or IP address: ");
		fflush(stdout);
		if (get_input(buf, sizeof(s->host)) == NULL) {
			return -1;
		}
		if (strchr(buf, ' ') != NULL) {
			printf("\nHost names cannot contain spaces.\n");
			continue;
		}
		if (strchr(buf, '@') != NULL) {
			printf("\nHostnames cannot contain '@' characters.\n");
			continue;
		}
		if (strchr(buf, '[') != NULL) {
			printf("\nHostnames cannot contain '[' characters.\n");
			continue;
		}
		if (strchr(buf, ']') != NULL) {
			printf("\nHostnames cannot contain ']' characters.\n");
			continue;
		}
		if (!buf[0]) {
			if (!s->host[0])
				continue;
		} else
			strcpy(s->host, buf);
		break;
	}
	return 0;
}

int
get_port(session_t *s)
{
    	char inbuf[STR_SIZE];
	char *ptr;
	unsigned long u;

    	new_screen(s, "\
TCP Port\n\
\n\
This specifies the TCP Port to use to connect to the host.  It is a number from\n\
1 to 65535 or the name 'telnet'.  The default is the 'telnet' port, port 23.");

	for (;;) {
		printf("\nTCP port: [%d] ", s->port);
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
			return -1;
		}
		if (!inbuf[0])
			break;
		if (!strcasecmp(inbuf, "telnet")) {
		    	s->port = 23;
			break;
		}
		u = strtoul(inbuf, &ptr, 10);
		if (u < 1 || u > 65535 || *ptr != '\0') {
			printf("Invalid port.\n");
		} else {
		    	s->port = (int)u;
			break;
		}
	}
	return 0;
}

int
get_lu(session_t *s)
{
    	char buf[STR_SIZE];

    	new_screen(s, "\
Logical Unit (LU) Name\n\
\n\
This specifies a particular Logical Unit or Logical Unit group to connect to\n\
on the host.  The default is to allow the host to select the Logical Unit.");

	for (;;) {
		printf("\nEnter Logical Unit (LU) name: [%s] ",
			s->luname[0]? s->luname: "none");
		fflush(stdout);
		if (get_input(buf, sizeof(buf)) == NULL) {
			return -1;
		}
		if (!buf[0])
		    	break;
		if (!strcmp(buf, "none")) {
		    	s->luname[0] = '\0';
			break;
		}
		if (strchr(buf, ':') != NULL) {
		    	printf("\nLU name cannot contain ':' characters.\n");
			continue;
		}
		if (strchr(buf, '@') != NULL) {
		    	printf("\nLU name cannot contain '@' characters.\n");
			continue;
		}
		if (strchr(buf, '[') != NULL) {
		    	printf("\nLU name cannot contain '[' characters.\n");
			continue;
		}
		if (strchr(buf, ']') != NULL) {
		    	printf("\nLU name cannot contain ']' characters.\n");
			continue;
		}
		strcpy(s->luname, buf);
		break;
	}
	return 0;
}

int
get_model(session_t *s)
{
	int i;
    	char inbuf[STR_SIZE];
	char *ptr;
	unsigned long u;
	int max_model = is_nt? 5: 4;

	new_screen(s, "\
Model Number\n\
\n\
This specifies the dimensions of the screen.");

	printf("\n");
	for (i = 2; i <= max_model; i++) {
		if (wrows[i]) {
			printf(" Model %d has %2d rows and %3d columns.\n",
			    i, wrows[i] - 1, wcols[i]);
		}
	}
	for (;;) {
		printf("\nEnter model number: (2, 3%s) [%d] ",
			is_nt? ", 4 or 5": " or 4", s->model);
		fflush(stdout);
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
			return -1;
		}
		if (!inbuf[0]) {
			break;
		}
		u = strtoul(inbuf, &ptr, 10);
		if (u < 2 || u > max_model || *ptr != '\0') {
			printf("Invalid model number.\n");
			continue;
		}
		s->model = (int)u;
		break;
	}
	return 0;
}

int
get_charset(session_t *s)
{
    	char buf[STR_SIZE];
    	int i, k;
	char *ptr;
	unsigned long u;
#define NCS ((sizeof(charsets) / sizeof(charsets[0])) - 1)

	new_screen(s, "\
Character Set\n\
\n\
This specifies the EBCDIC character set used by the host.");

	printf("\
\nAvailable character sets:\n\n\
  #  Name                Host CP      #  Name                Host CP\n\
 --- ------------------- --------    --- ------------------- --------\n");
	k = 0;
	for (i = 0; i < NCS; i++) {
	    	int j;
		char *n, *h;


	    	if (i) {
			if (!(i % CS_COLS))
				printf("\n");
			else
				printf("   ");
		}
		if (!(i % 2))
		    	j = k;
		else {
		    	j += NCS / 2;
			k++;
		}
		if (is_nt || !charsets[j].is_dbcs) {
		    	n = charsets[j].name;
		    	h = charsets[j].hostcp;
		} else {
		    	n = "";
		    	h = "";
		}
		printf(" %2d. %-*s %-*s",
			j + 1,
			CS_WIDTH, n,
			CP_WIDTH, h);
	}
	printf("\n");
	for (;;) {
		printf("\nCharacter set: [%s] ", s->charset);
		if (get_input(buf, sizeof(buf)) == NULL) {
			return -1;
		}
		if (!buf[0])
			break;
		/* Check for numeric value. */
		u = strtoul(buf, &ptr, 10);
		if (u > 0 && u <= i && *ptr == '\0' &&
			    (is_nt || !charsets[u - 1].is_dbcs)) {
			strcpy(s->charset, charsets[u - 1].name);
			s->is_dbcs = charsets[u - 1].is_dbcs;
			break;
		}
		/* Check for alias. */
		for (i = 0; charset_alias[i].alias != NULL; i++) {
		    	if (!strcmp(buf, charset_alias[i].alias)) {
			    	strcpy(buf, charset_alias[i].real);
				break;
			}
		}
		/* Check for name match. */
		for (i = 0; charsets[i].name != NULL; i++) {
			if (!strcmp(buf, charsets[i].name) &&
				    (is_nt || !charsets[i].is_dbcs)) {
				strcpy(s->charset, charsets[i].name);
				s->is_dbcs = charsets[i].is_dbcs;
				break;
			}
		}
		if (charsets[i].name != NULL)
			break;
		printf("Invalid character set name.\n");
	}
	return 0;
}

#if defined(HAVE_LIBSSL) /*[*/
int
get_ssl(session_t *s)
{
    	new_screen(s, "\
SSL Tunnel\n\
\n\
This option causes wc3270 to first create a tunnel to the host using the\n\
Secure Sockets Layer (SSL), then to run the TN3270 session inside the tunnel.");

	do {
		printf("\nUse an SSL tunnel? (y/n) [%s] ",
			s->ssl? "y" : "n");
		fflush(stdout);
		s->ssl = getyn(s->ssl);
		if (s->ssl == -1)
			return -1;
	} while (s->ssl < 0);
	return 0;
}
#endif /*]*/

int
get_proxy_server(session_t *s)
{
    	char hbuf[STR_SIZE];

	/* Get the hostname. */
	for (;;) {
	    	if (s->proxy_host[0]) {
			printf("\nProxy server name: [%s] ", s->proxy_host);
		} else {
			printf("\nProxy server name: ");
		}
		if (get_input(hbuf, STR_SIZE) == NULL)
		    	return -1;
		if (!hbuf[0]) {
		    	if (s->proxy_host[0])
			    	break;
			else
				continue;
		}
		if (strchr(hbuf, '[') != NULL ||
		    strchr(hbuf, ']') != NULL) {
		    	printf("Server name cannot include '[' or ']'\n");
			continue;
		}
		strcpy(s->proxy_host, hbuf);
		break;
	}
    	return 0;
}

int
get_proxy_server_port(session_t *s)
{
    	char pbuf[STR_SIZE];
	int i;

	for (i = 0; proxies[i].name != NULL; i++) {
	    	if (!strcmp(s->proxy_type, proxies[i].name))
		    	break;
	}
	if (proxies[i].name == NULL) {
	    	printf("Internal error\n");
		return -1;
	}

	for (;;) {
	    	unsigned long l;
		char *ptr;

		if (s->proxy_port[0])
			printf("\nProxy server TCP port: [%s] ", s->proxy_port);
		else if (proxies[i].port != NULL)
			printf("\nProxy server TCP port: [%s] ",
				proxies[i].port);
		else
			printf("\nProxy server TCP port: ");
		if (get_input(pbuf, STR_SIZE) == NULL)
		    	return -1;
		if (!strcmp(pbuf, "default") && proxies[i].port != NULL) {
		    	strcpy(s->proxy_port, proxies[i].port);
			break;
		}
		if (!pbuf[0]) {
		    	if (s->proxy_port[0])
			    	break;
			else if (proxies[i].port != NULL) {
			    	strcpy(s->proxy_port, proxies[i].port);
				break;
			} else
				continue;
		}
		l = strtoul(pbuf, &ptr, 10);
		if (l == 0 || *ptr != '\0' || (l & ~0xffffL))
		    	printf("Invalid port\n");
		else {
			strcpy(s->proxy_port, pbuf);
		    	break;
		}
	}
    	return 0;
}

int
get_proxy(session_t *s)
{
    	int i, j;
	char tbuf[STR_SIZE];
	char old_proxy[STR_SIZE];

    	new_screen(s, "\
Proxy\n\
\n\
If you do not have a direct connection to your host, this option allows\n\
wc3270 to use a proxy server to make the connection.");

	printf("\nProxy types available:\n");
	printf(" 1. none      Direct connection to host\n");
	for (i = 0; proxies[i].name != NULL; i++) {
	    	printf(" %d. %-8s  %s\n",
			i + 2,
			proxies[i].name,
			proxies[i].protocol);
	}

	strcpy(old_proxy, s->proxy_type);

	/* Get the proxy type. */
	for (;;) {
	    	int n;

	    	printf("\nProxy type: [%s] ",
			s->proxy_type[0]? s->proxy_type: "none" );
		if (get_input(tbuf, STR_SIZE) == NULL)
		    	return -1;
		if (!tbuf[0])
		    	return 0;
		if (!strcasecmp(tbuf, "none")) {
			s->proxy_type[0] = '\0';
			s->proxy_host[0] = '\0';
			s->proxy_port[0] = '\0';
		    	return 0;
		}
		for (j = 0; proxies[j].name != NULL; j++) {
		    	if (!strcasecmp(tbuf, proxies[j].name))
			    	break;
		}
		if (proxies[j].name != NULL) {
		    	strcpy(s->proxy_type, tbuf);
		    	break;
		}
		n = atoi(tbuf);
		if (n > 0 && n <= i+1) {
		    	if (n == 1) {
				s->proxy_type[0] = '\0';
				s->proxy_host[0] = '\0';
				s->proxy_port[0] = '\0';
				return 0;
			} else {
				j = n - 2;
				strcpy(s->proxy_type, proxies[j].name);
				break;
			}
		}
		printf("Invalid proxy type.\n");
	}

	/* If the type changed, the rest of the information is invalid. */
	if (strcmp(old_proxy, s->proxy_type)) {
	    	s->proxy_host[0] = '\0';
		s->proxy_port[0] = '\0';

	    	if (get_proxy_server(s) < 0) {
			return -1;
		}

		if (proxies[j].port != NULL)
		    	strcpy(s->proxy_port, proxies[j].port);
		else if (get_proxy_server_port(s) < 0) {
			return -1;
		}
	}

	return 0;
}

int
get_wpr3287(session_t *s)
{
    	new_screen(s, "\
wpr3287 Session\n\
\n\
This option allows wc3270 to automatically start a wpr3287 printer session\n\
when it connects to the host, allowing the host to direct print jobs to a\n\
Windows printer.");

	do {
		printf("\nAutomatically start a wpr3287 printer session? (y/n) [n] ");
		fflush(stdout);
		s->wpr3287 = getyn(s->wpr3287);
		if (s->wpr3287 == -1)
		    	return -1;
	} while (s->wpr3287 < 0);
	if (s->wpr3287 == 0)
	    	strcpy(s->printerlu, ".");
	return 0;
}

int
get_printerlu(session_t *s)
{
	int rc;

	new_screen(s, "\
wpr3287 Session -- Printer Logical Unit (LU) Name\n\
\n\
The wpr3287 printer session can be configured in one of two ways.  The first\n\
method automatically associates the printer session with the current login\n\
session.  The second method specifies a particular Logical Unit (LU) to use\n\
for the printer session.");

	for (;;) {
		printf("\nAssociate the printer session with the current login session (y/n) [%s]: ",
			strcmp(s->printerlu, ".")? "n": "y");
		fflush(stdout);
		rc = getyn(!strcmp(s->printerlu, "."));
		switch (rc) {
		case -1:
		    	return -1;
		case -2:
		default:
			continue;
		case 0:
			if (!strcmp(s->printerlu, "."))
				s->printerlu[0] = '\0';
			break;
		case 1:
			strcpy(s->printerlu, ".");
			return 0;
		}
		break;
	}

	for (;;) {
		char tbuf[STR_SIZE];

	    	if (s->printerlu[0])
			printf("\nEnter printer Logical Unit (LU) name: [%s] ",
				s->printerlu);
		else
			printf("\nEnter printer Logical Unit (LU) name: ");
		fflush(stdout);
		if (get_input(tbuf, STR_SIZE) == NULL)
			return -1;
		if (!tbuf[0]) {
		    	if (s->printerlu[0])
			    	break;
			else
			    	continue;
		} else {
		    	strcpy(s->printerlu, tbuf);
			break;
		}
	}

	return 0;
}

int
get_printer(session_t *s)
{
	char tbuf[STR_SIZE];
    	int i;
	char *ptr;
	unsigned long u;

	new_screen(s, "\
wpr3287 Session -- Windows Printer Name\n\
\n\
The wpr3287 session can use the Windows default printer as its real printer,\n\
or you can specify a particular Windows printer.  You can specify a local\n\
printer, or specify a remote printer with a UNC path, e.g.,\n\
'\\\\server\\printer22'.  You can specify the Windows default printer with\n\
the name 'default'.");

	enum_printers();
	if (num_printers) {
		printf("\nWindows printers (default is '*'):\n");
		for (i = 0; i < num_printers; i++) {
			printf(" %2d. %c %s\n", i + 1,
				strcasecmp(default_printer,
				    printer_info[i].pName)? ' ': '*',
				printer_info[i].pName);
		}
		printf(" %2d.   Other\n", num_printers + 1);
		for (;;) {
			if (s->printer[0])
				printf("\nEnter Windows printer (1-%d): [%s] ",
				    num_printers + 1, s->printer);
			else
				printf("\nEnter Windows printer (1-%d): [use system default] ",
					num_printers + 1);
			fflush(stdout);
			if (get_input(tbuf, STR_SIZE) == NULL)
				return -1;
			if (!tbuf[0])
				break;
			if (!strcmp(tbuf, "default")) {
			    	s->printer[0] = '\0';
				break;
			}
			u = strtoul(tbuf, &ptr, 10);
			if (*ptr != '\0' || u == 0 ||
				    u > num_printers + 1)
				continue;
			if (u == num_printers + 1) {
				if (get_printer_name(s->printer, tbuf) < 0)
					return -1;
				strcpy(s->printer, tbuf);
				break;
			}
			strcpy(s->printer, printer_info[u - 1].pName);
			break;
		}
	} else {
		if (get_printer_name(s->printer, tbuf) < 0)
			return -1;
		strcpy(s->printer, tbuf);
	}
	return 0;
}

int
get_printercp(session_t *s)
{
    	char buf[STR_SIZE];

	new_screen(s, "\
wpr3287 Session -- Printer Code Page\n\
\n\
By default, wpr3287 uses the system's default ANSI code page.  You can\n\
override that code page here, or specify 'default' to use the system ANSI code\n\
page.");

	for (;;) {
	    	int cp;

		printf("\nPrinter code page [%s]: ",
			s->printercp[0]? s->printercp: "default");
		fflush(stdout);
		if (get_input(buf, STR_SIZE) == NULL)
			return -1;
		if (!buf[0])
		    	break;
		if (!strcmp(buf, "default")) {
		    	s->printercp[0] = '\0';
			break;
		}
		cp = atoi(buf);
		if (cp <= 0) {
		    	printf("Invald code page\n");
		} else {
		    	strcpy(s->printercp, buf);
			break;
		}
	}

	return 0;
}

int
get_keymaps(session_t *s, char *installdir)
{
    	int i;
	HANDLE h;
	WIN32_FIND_DATA find_data;
	km_t *km;
	char dpath[MAX_PATH];
	char fpath[MAX_PATH];

	new_screen(s, "\
Keymaps\n\
\n\
A keymap is a mapping from the PC keyboard to the virtual 3270 keyboard.\n\
You can override the default keymap and specify one or more built-in or \n\
user-defined keymaps, separated by commas.");

	printf("\n");
	for (i = 0; builtin_keymaps[i].name != NULL; i++) {
	    	printf(" %s\n    %s\n",
			builtin_keymaps[i].name,
			builtin_keymaps[i].description);
		(void) save_keymap_name(NULL, builtin_keymaps[i].name);
	}
	sprintf(dpath, "%s*.wc3270km", mya);
	h = FindFirstFile(dpath, &find_data);
	if (h != INVALID_HANDLE_VALUE) {
		do {
		    	km_t *km;

			sprintf(fpath, "%s%s", mya, find_data.cFileName);
			km = save_keymap_name(fpath, find_data.cFileName);
			if (km != NULL) {
				printf(" %s\n    User-defined",
					km->name);
				if (km->description[0])
				    	printf(": %s", km->description);
				printf("\n");
			}
		} while (FindNextFile(h, &find_data) != 0);
		FindClose(h);
	}
	sprintf(dpath, "%s\\*.wc3270km", installdir);
	h = FindFirstFile(dpath, &find_data);
	if (h != INVALID_HANDLE_VALUE) {
		do {
		    	km_t *km;

			sprintf(fpath, "%s\\%s", installdir,
				find_data.cFileName);
			km = save_keymap_name(fpath, find_data.cFileName);
			if (km != NULL) {
				printf(" %s\n    User-defined",
					km->name);
				if (km->description[0])
				    	printf(": %s", km->description);
				printf("\n");
			}
		} while (FindNextFile(h, &find_data) != 0);
		FindClose(h);
	}
	for (;;) {
	    	char inbuf[STR_SIZE];
	    	char tknbuf[STR_SIZE];
		char *t;
		char *buf;
		int wrong = FALSE;

	    	printf("\nEnter keymap name(s) [%s]: ",
			s->keymaps[0]? s->keymaps: "none");
		fflush(stdout);
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
			return -1;
		if (!inbuf[0])
		    	break;
		if (!strcmp(inbuf, "none")) {
		    	s->keymaps[0] = '\0';
			break;
		}
		strcpy(tknbuf, inbuf);
		wrong = FALSE;
		buf = tknbuf;
		while (!wrong && (t = strtok(buf, ",")) != NULL) {
		    	buf = NULL;
			for (km = km_first; km != NULL; km = km->next) {
				if (!strcasecmp(t, km->name))
					break;
			}
			if (km == NULL) {
			    	printf("\nInvalid keymap name '%s'\n", t);
				wrong = TRUE;
				break;
			}
		}
		if (!wrong) {
			strcpy(s->keymaps, inbuf);
			break;
		}
	}
	return 0;
}

int
summarize_and_proceed(session_t *s, char *installdir, char *how)
{
    	int rc;
	char choicebuf[32];

	for (;;) {
	    	int done = 0;
		char *cp = "?";
		int i;

		for (i = 0; charsets[i].name != NULL; i++)
			if (!strcmp(charsets[i].name, s->charset)) {
			    	cp = charsets[i].hostcp;
				break;
			}

		new_screen(s, "");

		printf("  1. Host ................... : %s\n", s->host);
		printf("  2. Logical Unit Name ...... : %s\n",
			s->luname[0]? s->luname: "(none)");
		printf("  3. TCP Port ............... : %d\n", s->port);
		printf("  4. Model Number ........... : %d (%d rows x %d columns)\n",
		    s->model, wrows[s->model] - 1, wcols[s->model]);
		printf("  5. Character Set .......... : %s (CP %s)\n",
			s->charset, cp);
#if defined(HAVE_LIBSSL) /*[*/
		printf("  6. SSL Tunnel ............. : %s\n",
			s->ssl? "Yes": "No");
#endif /*]*/
		printf("  7. Proxy .................. : %s\n",
			s->proxy_type[0]? s->proxy_type: "(none)");
		if (s->proxy_type[0]) {
			printf("  8.  Proxy Server .......... : %s\n",
				s->proxy_host);
			if (s->proxy_port[0])
				printf("  9.  Proxy Server TCP Port . : %s\n",
					s->proxy_port);
		}
		printf(" 10. wpr3287 Printer Session  : %s\n",
			s->wpr3287? "Yes": "No");
		if (s->wpr3287) {
			printf(" 11.  wpr3287 Mode .......... : ");
			if (!strcmp(s->printerlu, "."))
				printf("Associate\n");
			else
				printf("LU %s\n", s->printerlu);
			printf(" 12.  wpr3287 Windows printer : %s\n",
				s->printer[0]? s->printer: "(system default)");
			printf(" 13.  wpr3287 Code Page ..... : ");
			if (s->printercp[0])
			    	printf("%s\n", s->printercp);
			else
			    	printf("(system ANSI default of %d)\n",
					GetACP());
		}
		printf(" 14. Keymaps ................ : %s\n",
			s->keymaps[0]? s->keymaps: "(none)");

		for (;;) {
		    	int invalid = 0;
			int was_wpr3287 = 0;

			printf("\nEnter item number to change: [none] ");
			fflush(stdout);
			if (get_input(choicebuf, sizeof(choicebuf)) == NULL)
				return -1;
			if (!choicebuf[0]) {
				done = 1;
				break;
			}
			switch (atoi(choicebuf)) {
			case 1:
				if (get_host(s) < 0)
					return -1;
				break;
			case 2:
				if (get_lu(s) < 0)
					return -1;
				break;
			case 3:
				if (get_port(s) < 0)
					return -1;
				break;
			case 4:
				if (get_model(s) < 0)
					return -1;
				break;
			case 5:
				if (get_charset(s) < 0)
					return -1;
				break;
#if defined(HAVE_LIBSSL) /*[*/
			case 6:
				if (get_ssl(s) < 0)
					return -1;
				break;
#endif /*]*/
			case 7:
				if (get_proxy(s) < 0)
					return -1;
				break;
			case 8:
				if (s->proxy_type[0]) {
					if (get_proxy_server(s) < 0)
						return -1;
				} else {
					printf("Invalid entry.\n");
					invalid = 1;
				}
				break;
			case 9:
				if (s->proxy_type[0]) {
					if (get_proxy_server_port(s) < 0)
						return -1;
				} else {
					printf("Invalid entry.\n");
					invalid = 1;
				}
				break;
			case 10:
				was_wpr3287 = s->wpr3287;
				if (get_wpr3287(s) < 0)
					return -1;
				if (s->wpr3287 && !was_wpr3287) {
					if (get_printerlu(s) < 0)
						return -1;
				}
				break;
			case 11:
				if (s->wpr3287) {
					if (get_printerlu(s) < 0)
						return -1;
				} else {
					printf("Invalid entry.\n");
					invalid = 1;
				}
				break;
			case 12:
				if (s->wpr3287) {
					if (get_printer(s) < 0)
						return -1;
				} else {
					printf("Invalid entry.\n");
					invalid = 1;
				}
				break;
			case 13:
				if (s->wpr3287) {
					if (get_printercp(s) < 0)
						return -1;
				} else {
					printf("Invalid entry.\n");
					invalid = 1;
				}
				break;
			case 14:
				if (get_keymaps(s, installdir) < 0)
					return -1;
				break;
			default:
				printf("Invalid entry.\n");
				invalid = 1;
				break;
			}

			if (!invalid)
				break;
		}
		if (done)
			break;
	}

	for (;;) {
		printf("\n%s the session file? (y/n) [y] ", how);
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}
	return 0;
}

wchar_t *
reg_font_from_cset(char *cset, int *codepage)
{
    	int i, j;
	wchar_t *cpname = NULL;
	wchar_t data[1024];
	DWORD dlen;
	HKEY key;
	static wchar_t font[1024];
	DWORD type;

	*codepage = 0;

    	/* Search the table for a match. */
	for (i = 0; charsets[i].name != NULL; i++) {
	    	if (!strcmp(cset, charsets[i].name)) {
		    	cpname = charsets[i].codepage;
		    	break;
		}
	}

	/* If no match, use Lucida Console. */
	if (cpname == NULL)
	    	return L"Lucida Console";

	/*
	 * Look in the registry for the console font associated with the
	 * Windows code page.
	 */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		    "Software\\Microsoft\\Windows NT\\CurrentVersion\\"
		    "Console\\TrueTypeFont",
		    0,
		    KEY_READ,
		    &key) != ERROR_SUCCESS) {
	    	printf("RegOpenKey failed -- cannot find font\n");
		return L"Lucida Console";
	}
	dlen = sizeof(data);
    	if (RegQueryValueExW(key,
		    cpname,
		    NULL,
		    &type,
		    (LPVOID)data,
		    &dlen) != ERROR_SUCCESS) {
	    	/* No codepage-specific match, try the default. */
	    	dlen = sizeof(data);
	    	if (RegQueryValueExW(key, L"0", NULL, &type, (LPVOID)data,
			    &dlen) != ERROR_SUCCESS) {
			RegCloseKey(key);
			printf("RegQueryValueEx failed -- cannot find font\n");
			return L"Lucida Console";
		}
	}
	RegCloseKey(key);
	if (type == REG_MULTI_SZ) {
		for (i = 0; i < dlen/sizeof(wchar_t); i++) {
			if (data[i] == 0x0000)
				break;
		}
		if (i+1 >= dlen/sizeof(wchar_t) || data[i+1] == 0x0000) {
			printf("Bad registry value -- cannot find font\n");
			return L"Lucida Console";
		}
		i++;
	} else
	    i = 0;
	for (j = 0; i < dlen; i++, j++) {
		if (j == 0 && data[i] == L'*')
		    i++;
	    	if ((font[j] = data[i]) == 0x0000)
		    	break;
	}
	*codepage = _wtoi(cpname);
	return font;
}

int
session_wizard(void)
{
    	session_t session;
	int rc;
	char desktop[MAX_PATH];
	char installdir[MAX_PATH];
	char linkpath[MAX_PATH];
	char exepath[MAX_PATH];
	char args[MAX_PATH];
	HRESULT hres;
	char save_session_name[STR_SIZE];
	FILE *f;
	int shortcut_exists;
	char path[MAX_PATH];

	/* Start with nothing. */
	(void) memset(&session, '\0', sizeof(session));

	/* Figure out where the install directory is. */
	if (getcwd(installdir, MAX_PATH) == NULL) {
		printf("getcwd failed: %s\n", strerror(errno));
		return -1;
	}

	/* Get some paths from Windows. */
	if (get_dirs(desktop, mya, "wc3270") < 0)
	    	return -1;

	/* Intro screen. */
	if (intro(&session) < 0)
		return -1;

	/* Get the session name. */
	rc = get_session(&session, path);
	switch (rc) {
	case -2: /* Uneditable, and they don't want to overwrite it. */
	    	return 0;
	default:
	case -1: /* EOF */
		return -1;
	case 3: /* Overwrite old (uneditable). */
		/* Clean out the session. */
		strcpy(save_session_name, session.session);
		memset(&session, '\0', sizeof(session));
		strcpy(session.session, save_session_name);
		/* fall through... */
	case 0: /* New. */

		/* Get the host name, which defaults to the session name. */
		if (strchr(session.session, ' ') == NULL)
			strcpy(session.host, session.session);
		if (get_host(&session) < 0)
			return -1;

		/* Default eveything else. */
		session.port = 23;
		session.model = 4;
		strcpy(session.charset, "bracket");
		strcpy(session.printerlu, ".");
		/* fall through... */
	case 1: /* Edit existing file. */
		/* See what they want to change. */
		if (summarize_and_proceed(&session, installdir,
			    (rc == 3)? "Replace":
			    ((rc == 0)? "Create": "Update")) < 0)
			return -1;

		/* Create the session file. */
		printf("\nWriting session file '%s'... ", path);
		fflush(stdout);
		if (create_session_file(&session, path) < 0)
			return -1;
		printf("done\n");
		fflush(stdout);
		break;
	case 2: /* Don't edit existing file, but we do have a copy of the
		   session. */
		break;
	}

	/* Ask about the shortcut. */
	if (is_nt)
		sprintf(linkpath, "%s\\%s.lnk", desktop, session.session);
	else
		sprintf(linkpath, "%s\\%s.pif", desktop, session.session);
	f = fopen(linkpath, "r");
	if ((shortcut_exists = (f != NULL)))
		fclose(f);
	for (;;) {
	    	printf("\n%s desktop shortcut (y/n) [y]: ",
			shortcut_exists? "Replace": "Create");
		rc = getyn(1);
		if (rc <= 0)
		    	return -1;
		if (rc == 1)
		    	break;
	}

	/* Create the desktop shorcut. */
	printf("\n%s desktop shortcut '%s'... ",
		shortcut_exists? "Replacing": "Creating", linkpath);
	fflush(stdout);
	sprintf(exepath, "%s\\wc3270.exe", installdir);
	sprintf(args, "\"%s\"", path);
	if (is_nt) {
	    	wchar_t *font;
		int codepage = 0;

		font = reg_font_from_cset(session.charset, &codepage);

		hres = CreateLink(
			exepath,		/* path to executable */
			linkpath,		/* where to put the link */
			"wc3270 session",	/* description */
			args,			/* arguments */
			installdir,		/* working directory */
			wrows[session.model], wcols[session.model],
						/* console rows, columns */
			font,			/* font */
			0,			/* point size */
			codepage);		/* code page */
	} else
		hres = Piffle(
			session.session,	/* window title */
			exepath,		/* path to executable */
			linkpath,		/* where to put the link */
			"wc3270 session",	/* description */
			args,			/* arguments */
			installdir,		/* working directory */
			wrows[session.model], wcols[session.model],
						/* console rows, columns */
			"Lucida Console");	/* font */

	if (SUCCEEDED(hres)) {
		printf("done\n");
		fflush(stdout);
		return 0;
	} else {
		printf("Failed\n");
		fflush(stdout);
		return -1;
	}
}

/* Create the session file. */
int
create_session_file(session_t *session, char *path)
{
    	FILE *f;
	time_t t;
	int bracket;
	long eot;
	unsigned long csum;
	int i;
	char buf[1024];

	/*
	 * Create the AppData directory if it doesn't exist.  (If wc3270 was
	 * installed by a different user, it won't.)
	 */
	(void) _mkdir(mya);

	f = fopen(path, "w+");
	if (f == NULL) {
		perror("Cannot create session file");
		return -1;
	}

	fprintf(f, "! wc3270 session '%s'\n", session->session);

	t = time(NULL);
	fprintf(f, "! Created by the wc3270 %s session wizard %s",
		wversion, ctime(&t));

	bracket = (strchr(session->host, ':') != NULL);
	fprintf(f, "wc3270.hostname: ");
	if (session->ssl)
	    	fprintf(f, "L:");
	if (session->luname[0])
	    	fprintf(f, "%s@", session->luname);
	fprintf(f, "%s%s%s",
		bracket? "[": "",
		session->host,
		bracket? "]": "");
	if (session->port != 23)
	    	fprintf(f, ":%d", session->port);
	fprintf(f, "\n");

	if (session->proxy_type[0])
	    	fprintf(f, "wc3270.proxy: %s:%s%s%s%s%s\n",
			session->proxy_type,
			strchr(session->proxy_host, ':')? "[": "",
			session->proxy_host,
			strchr(session->proxy_host, ':')? "]": "",
			session->proxy_port[0]? ":": "",
			session->proxy_port);

	fprintf(f, "wc3270.model: %d\n", session->model);
	fprintf(f, "wc3270.charset: %s\n", session->charset);
	if (session->is_dbcs)
	    	fprintf(f, "wc3270.asciiBoxDraw: True\n");

	if (session->wpr3287) {
	    	fprintf(f, "wc3270.printerLu: %s\n", session->printerlu);
		if (session->printer[0])
		    	fprintf(f, "wc3270.printer.name: %s\n",
				session->printer);
		if (session->printercp[0])
		    	fprintf(f, "wc3270.printer.codepage: %s\n",
				session->printercp);
	}

	if (session->keymaps[0]) {
	    	fprintf(f, "wc3270.keymap: %s\n", session->keymaps);
	}

	/* Emit the warning. */
	fprintf(f, "\
!\n\
! The following block of text is used to read the contents of this file back\n\
! into the New Session Wizard.  If any of the text from the top of the file\n\
! through the line below reading \"Additional resource definitions...\" is\n\
! modified, the New Session Wizard will not be able to edit this file.\n\
!");

	/* Write out the session structure in hex. */
	for (i = 0; i < sizeof(*session); i++) {
	    	if (!(i % 32))
		    	fprintf(f, "\n!x");
		fprintf(f, "%02x", ((char *)session)[i]);
	}
	fprintf(f, "\n");

	/* Save where we are in the file. */
	fflush(f);
	eot = ftell(f);

	/* Go back and read what we wrote. */
	rewind(f);
	csum = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		for (i = 0; buf[i]; i++) {
		    	csum += buf[i] & 0xff;
		}
		if (ftell(f) >= eot)
		    	break;
	}
	fflush(f);

	/* Write out the checksum and structure version. */
	fseek(f, 0, SEEK_END);
	fprintf(f, "!c%08lx %d\n", csum, WIZARD_VER);

	fprintf(f, "!\n\
!*Additional resource definitions can go after this line.\n");

	/* Write out the user's previous extra settings. */
	if (user_settings != NULL)
	    	fprintf(f, "%s", user_settings);

	fclose(f);

	return 0;
}

/* Convert a hexadecimal digit to a nybble. */
static unsigned
hex(char c)
{
    	static char *digits = "0123456789abcdef";
	char *pos;

	pos = strchr(digits, c);
	if (pos == NULL)
	    	return 0; /* XXX */
	return pos - digits;
}

//#define DEBUG_EDIT 1

/*
 * Read an existing session file.
 * Returns 1 for success (file read and editable), 0 for failure.
 */
static int
read_session(FILE *f, session_t *s)
{
    	char buf[1024];
	int saw_hex = 0;
	int saw_star = 0;
	unsigned long csum;
	unsigned long fcsum = 0;
	int ver;
	int s_off = 0;

	/*
	 * Look for the checksum and version.  Verify the version.
	 *
	 * XXX: It might be a good idea to validate each '!x' line and
	 * make sure that the length is right.
	 */
	while (fgets(buf, sizeof(buf), f) != NULL) {
	    	if (buf[0] == '!' && buf[1] == 'x')
		    	saw_hex = 1;
		else if (buf[0] == '!' && buf[1] == '*')
		    	saw_star = 1;
		else if (buf[0] == '!' && buf[1] == 'c') {
			if (sscanf(buf + 2, "%lx %d", &csum, &ver) != 2) {
#if defined(DEBUG_EDIT) /*[*/
				printf("[bad !c line '%s']\n", buf);
#endif /*]*/
				return 0;
			}
			if (ver > WIZARD_VER) {
#if defined(DEBUG_EDIT) /*[*/
				printf("[bad ver %d > %d]\n",
					ver, WIZARD_VER);
#endif /*]*/
			    	return 0;
			}
		}
	}
	if (!saw_hex || !saw_star) {
#if defined(DEBUG_EDIT) /*[*/
	    	printf("[missing%s%s]\n",
			saw_hex? "": "hex",
			saw_star? "": "star");
#endif /*]*/
		return 0;
	}

	/* Checksum from the top up to the '!c' line. */
	fflush(f);
	fseek(f, 0, SEEK_SET);
	fcsum = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
	    	char *t;

		if (buf[0] == '!' && buf[1] == 'c')
		    	break;

		for (t = buf; *t; t++)
		    	fcsum += *t & 0xff;
	}
	if (fcsum != csum) {
#if defined(DEBUG_EDIT) /*[*/
	    	printf("[checksum mismatch, want 0x%08lx got 0x%08lx]\n",
			csum, fcsum);
#endif /*]*/
	    	return 0;
	}

	/* Once more, with feeling.  Scribble onto the session structure. */
	fflush(f);
	fseek(f, 0, SEEK_SET);
	s_off = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {

	    	if (buf[0] == '!' && buf[1] == 'x') {
		    	char *t;

			for (t = buf + 2; *t; t += 2) {
			    	if (*t == '\n')
				    	break;
				if (s_off > sizeof(*s)) {
#if defined(DEBUG_EDIT) /*[*/
					printf("[s overflow: %d > %d]\n",
						s_off, sizeof(*s));
#endif /*]*/
					return 0;
				}
			    	((char *)s)[s_off++] =
				    (hex(*t) << 4) | hex(*(t + 1));
			}
		} else if (buf[0] == '!' && buf[1] == 'c')
		    	break;
	}

	/*
	 * Read the balance of the file into a temporary buffer, ignoring
	 * the '!*' line.
	 */
	saw_star = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
	    	if (!saw_star) {
			if (buf[0] == '!' && buf[1] == '*')
				saw_star = 1;
			continue;
		}
		if (user_settings == NULL) {
		    	user_settings = malloc(strlen(buf) + 1);
			user_settings[0] = '\0';
		} else
		    	user_settings = realloc(user_settings,
				strlen(user_settings) + strlen(buf) + 1);
		if (user_settings == NULL) {
#if defined(DEBUG_EDIT) /*[*/
			printf("out of memory]\n");
#endif /*]*/
			return 0;
		}
		strcat(user_settings, buf);
	}

	/* Success */
	return 1;
}

/* Make sure the console window is long enough. */
int
resize_window(int rows)
{
    	int rv = 0;
	HANDLE h;
    	CONSOLE_SCREEN_BUFFER_INFO info;

	do {
	    	/* Get a handle to the console. */
		h = CreateFile("CONOUT$",
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);
		if (h == NULL) {
		    	rv = -1;
			break;
		}

		/* Get its current geometry. */
		if (GetConsoleScreenBufferInfo(h, &info) == 0) {
		    	rv = -1;
			break;
		}

		/* If the buffer isn't big enough, make it bigger. */
		if (info.dwSize.Y < rows) {
			COORD new_size;

			new_size.X = info.dwSize.X;
			new_size.Y = rows;

			if (SetConsoleScreenBufferSize(h, new_size) == 0) {
				rv = -1;
				break;
			}
		}

		/* If the window isn't big enough, make it bigger. */
		if (info.srWindow.Bottom - info.srWindow.Top < rows) {
		    	SMALL_RECT sr;

			sr.Top = 0;
			sr.Bottom = rows;
			sr.Left = 0;
			sr.Right = info.srWindow.Right - info.srWindow.Left;

		    	if (SetConsoleWindowInfo(h, TRUE, &sr) == 0) {
				rv = -1;
				break;
			}
		}

	} while(0);

	if (h != NULL)
	    	CloseHandle(h);
	return rv;
}

int
main(int argc, char *argv[])
{
	int rc;
	char buf[2];

	/* Figure out the version. */
	if (get_version_info() < 0)
	    	return -1;

	if (is_nt)
		resize_window(44);
	else
	    	system("mode con lines=50");

	signal(SIGINT, SIG_IGN);

	rc = session_wizard();

	printf("\nWizard %s.  [Press <Enter>] ",
		    (rc < 0)? "aborted": "complete");
	fflush(stdout);
	(void) fgets(buf, 2, stdin);

	return 0;
}
