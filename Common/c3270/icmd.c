/*
 * Copyright (c) 2007-2009, 2013-2016 Paul Mattes.
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
 *	icmd.c
 *		A curses-based 3270 Terminal Emulator
 *		Interactive commands
 */

#include "globals.h"
#include "appres.h"

#include "charset.h"
#include "ft_dft.h"
#include "ft_private.h"
#include "icmdc.h"
#include "lazya.h"
#include "popups.h"
#include "utf8.h"
#include "utils.h"

/* Support functions for interactive commands. */

/**
 * Interactive command module registration.
 */
void
icmd_register(void)
{
}

/*
 * Get a buffer full of input.
 * Trims white space in the result.
 * Returns NULL if there is an input error or if the input is 'quit'.
 */
static char *
get_input(char *buf, int size)
{
    size_t sl;
    char *s;

    fflush(stdout);

    /* Get the raw input. */
    if (fgets(buf, size, stdin) == NULL) {
	return NULL;
    }

    /* Trim trailing white space. */
    sl = strlen(buf);
    while (sl && isspace((unsigned char)buf[sl - 1])) {
	buf[--sl] = '\0';
    }

    /* Trim leading white space. */
    s = buf;
    while (*s && isspace((unsigned char)*s)) {
	s++;
	sl--;
    }
    if (s != buf) {
	memmove(buf, s, sl + 1);
    }

    /* Check for 'quit'. */
    if (!strcasecmp(buf, "quit")) {
	return NULL;
    }

    return buf;
}

/* Get a yes, no or quit.  Returns 0 for no, 1 for yes, -1 for quit or error. */
static int
getyn(int defval)
{
    	char buf[64];

	for (;;) {
	    	if (get_input(buf, sizeof(buf)) == NULL)
		    	return -1;
		if (!buf[0])
		    	return defval;
		if (!strncasecmp(buf, "yes", strlen(buf)))
		    	return 1;
		else if (!strncasecmp(buf, "no", strlen(buf)))
		    	return 0;
		else {
		    	printf("Please answer 'yes', 'no' or 'quit': ");
		}
	}
}

/*
 * Get a numeric value.  Returns the number for good input, -1 for quit or
 * error.
 */
static int
getnum(int defval)
{
    	char buf[64];
	unsigned long u;
	char *ptr;

	for (;;) {
	    	if (get_input(buf, sizeof(buf)) == NULL)
		    	return -1;
		if (!buf[0])
		    	return defval;
		u = strtoul(buf, &ptr, 10);
		if (*ptr == '\0')
		    	return (int)u;	
		printf("Please enter a number or 'quit': ");
	}
}

/* Format a text string to fit on an 80-column display. */
static void
fmt80(const char *s)
{
    char *nl;
    size_t nc;

    printf("\n");

    while (*s) {
	nl = strchr(s, '\n');
	if (nl == NULL) {
	    nc = strlen(s);
	} else {
	    nc = nl - s;
	}
	if (nc > 78) {
	    const char *t = s + 78;

	    while (t > s && *t != ' ') {
		t--;
	    }
	    if (t != s) {
		nc = t - s;
	    }
	}

	printf(" %.*s\n", (int)nc, s);
	s += nc;
	if (*s == '\n' || *s == ' ') {
	    s++;
	}
    }
}

/*
 * Interactive file transfer command.
 * Called from Transfer_action.  Returns an updated ft_private.
 * Returns 0 for success, -1 for failure.
 */
int
interactive_transfer(ft_conf_t *p)
{
#define KW_SIZE 1024
    char inbuf[KW_SIZE];
    int n;
    enum { CR_REMOVE, CR_ADD, CR_KEEP } cr_mode = CR_REMOVE;
    char *default_cr;
    enum { FE_KEEP, FE_REPLACE, FE_APPEND } fe_mode = FE_KEEP;
    char *default_fe;

    printf("\n\
File Transfer\n\
\n\
Type 'quit' at any prompt to abort this dialog.\n\
\n\
Note: In order to initiate a file transfer, the 3270 cursor must be\n\
positioned on an input field that can accept the IND$FILE command, e.g.,\n\
at the VM/CMS or TSO command prompt.\n");

    printf("\nContinue? (y/n) [y] ");
    if (getyn(1) <= 0) {
	return -1;
    }

    printf("\n\
 'send' means copy a file from this workstation to the host.\n\
 'receive' means copy a file from the host to this workstation.\n");
    for (;;) {
	printf("Direction: (send/receive) [%s] ",
		p->receive_flag? "receive": "send");
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    break;
	}
	if (!strncasecmp(inbuf, "receive", strlen(inbuf))) {
	    p->receive_flag = true;
	    break;
	}
	if (!strncasecmp(inbuf, "send", strlen(inbuf))) {
	    p->receive_flag = false;
	    break;
	}
    }

    printf("\n");
    for (;;) {
	printf("Name of source file on %s: ",
		p->receive_flag? "the host": "this workstation");
	if (p->receive_flag && p->host_filename) {
	    printf("[%s] ", p->host_filename);
	} else if (!p->receive_flag && p->local_filename) {
	    printf("[%s] ", p->local_filename);
	}
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    if ((p->receive_flag && p->host_filename) ||
		 (!p->receive_flag && p->local_filename)) {
		break;
	    } else {
		continue;
	    }
	}
	if (p->receive_flag) {
	    Replace(p->host_filename, NewString(inbuf));
	} else {
	    Replace(p->local_filename, NewString(inbuf));
	}
	break;
    }

    for (;;) {
	printf("Name of destination file on %s: ",
		p->receive_flag? "this workstation": "the host");
	if (!p->receive_flag && p->host_filename) {
	    printf("[%s] ", p->host_filename);
	} else if (p->receive_flag && p->local_filename) {
	    printf("[%s] ", p->local_filename);
	}
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    if ((!p->receive_flag && p->host_filename) ||
		 (p->receive_flag && p->local_filename)) {
		break;
	    } else {
		continue;
	    }
	}
	if (!p->receive_flag) {
	    Replace(p->host_filename, NewString(inbuf));
	} else {
	    Replace(p->local_filename, NewString(inbuf));
	}
	break;
    }

    printf("\n");
    for (;;) {
	printf("Host type: (tso/vm/cics) [%s] ",
		ft_decode_host_type(p->host_type));
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    break;
	}
	if (ft_encode_host_type(inbuf, &p->host_type)) {
	    break;
	}
    }

    printf("\n\
 An 'ascii' transfer does automatic translation between EBCDIC on the host and\n\
 ASCII on the workstation.\n\
 A 'binary' transfer does no data translation.\n");

    for (;;) {
	printf("Transfer mode: (ascii/binary) [%s] ",
		p->ascii_flag? "ascii": "binary");
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0]) {
	    break;
	}
	if (!strncasecmp(inbuf, "ascii", strlen(inbuf))) {
	    p->ascii_flag = true;
	    break;
	}
	if (!strncasecmp(inbuf, "binary", strlen(inbuf))) {
	    p->ascii_flag = false;
	    break;
	}
    }

    if (p->ascii_flag) {
	printf("\n\
 For ASCII transfers, carriage return (CR) characters can be handled specially.\n");
	if (p->receive_flag) {
	    printf("\
 'add' means that CRs will be added to each record during the transfer.\n");
	} else {
	    printf("\
 'remove' means that CRs will be removed during the transfer.\n");
	}
	printf("\
 'keep' means that no special action is taken with CRs.\n");
	default_cr = p->cr_flag? (p->receive_flag? "add": "remove"): "keep";
	for (;;) {
	    printf("CR handling: (%s/keep) [%s] ",
		    p->receive_flag? "add": "remove",
		    default_cr);
	    if (get_input(inbuf, sizeof(inbuf)) == NULL) {
		return -1;
	    }
	    if (!inbuf[0]) {
		cr_mode = p->cr_flag? (p->receive_flag? CR_ADD: CR_REMOVE):
					      CR_KEEP;
		break;
	    }
	    if (!strncasecmp(inbuf, "remove", strlen(inbuf))) {
		p->cr_flag = true;
		cr_mode = CR_REMOVE;
		break;
	    }
	    if (!strncasecmp(inbuf, "add", strlen(inbuf))) {
		p->cr_flag = true;
		cr_mode = CR_ADD;
		break;
	    }
	    if (!strncasecmp(inbuf, "keep", strlen(inbuf))) {
		p->cr_flag = false;
		cr_mode = CR_KEEP;
		break;
	    }
	}
	fmt80(lazyaf("For ASCII transfers, "
#if defined(WC3270) /*[*/
"w"
#endif /*]*/
"c3270 can either remap the text to ensure as "
"accurate a translation between "
#if defined(WC3270) /*[*/
"the Windows code page"
#else /*][*/
"%s"
#endif /*]*/
" and EBCDIC code page %s as possible, or it can transfer text as-is and "
"leave all translation to the IND$FILE program on the host.\n\
'yes' means that text will be translated.\n\
'no' means that text will be transferred as-is.",
#if !defined(WC3270) /*[*/
	    locale_codeset,
#endif /*]*/
	    get_host_codepage()));
	for (;;) {
	    printf("Re-map character set? (yes/no) [%s] ",
		    p->remap_flag? "yes": "no");
	    if (get_input(inbuf, sizeof(inbuf)) == NULL) {
		return -1;
	    }
	    if (!inbuf[0]) {
		break;
	    }
	    if (!strncasecmp(inbuf, "yes", strlen(inbuf))) {
		p->remap_flag = true;
		break;
	    }
	    if (!strncasecmp(inbuf, "no", strlen(inbuf))) {
		p->remap_flag = false;
		break;
	    }
	}
#if defined(_WIN32) /*[*/
	if (p->remap_flag) {
	    for (;;) {
		int cp;

		printf("Windows code page for re-mapping: [%d] ",
			p->windows_codepage);
		cp = getnum(p->windows_codepage);
		if (cp < 0) {
		    return -1;
		}
		if (cp > 0) {
		    p->windows_codepage = cp;
		    break;
		}
	    }
	}
#endif /*]*/
    }

    if (p->receive_flag) {
	printf("\n\
 If the destination file exists, you can choose to keep it (and abort the\n\
 transfer), replace it, or append the source file to it.\n");
	if (p->allow_overwrite) {
	    default_fe = "replace";
	} else if (p->append_flag) {
	    default_fe = "append";
	} else {
	    default_fe = "keep";
	}
	printf("\n");
	for (;;) {
	    printf("Action if destination file exists: "
		    "(keep/replace/append) [%s] ", default_fe);
	    if (get_input(inbuf, sizeof(inbuf)) == NULL) {
		return -1;
	    }
	    if (!inbuf[0]) {
		fe_mode = p->allow_overwrite? FE_REPLACE:
		    (p->append_flag? FE_APPEND: FE_KEEP);
		break;
	    }
	    if (!strncasecmp(inbuf, "keep", strlen(inbuf))) {
		p->append_flag = false;
		p->allow_overwrite = false;
		fe_mode = FE_KEEP;
		break;
	    }
	    if (!strncasecmp(inbuf, "replace", strlen(inbuf))) {
		p->append_flag = false;
		p->allow_overwrite = true;
		fe_mode = FE_REPLACE;
		break;
	    }
	    if (!strncasecmp(inbuf, "append", strlen(inbuf))) {
		p->append_flag = true;
		p->allow_overwrite = false;
		fe_mode = FE_APPEND;
		break;
	    }
	}
    }

    if (!p->receive_flag) {
	if (p->host_type != HT_CICS) {
	    printf("\n");
	    for (;;) {
		printf("[optional] Destination file record "
			"format:\n (default/fixed/variable/undefined) [%s] ",
			ft_decode_recfm(p->recfm));
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
		    return -1;
		}
		if (!inbuf[0]) {
		    break;
		}
		if (ft_encode_recfm(inbuf, &p->recfm)) {
		    break;
		}
	    }

	    printf("\n");
	    printf("[optional] Destination file logical record length: ");
	    if (p->lrecl) {
		printf("[%d] ", p->lrecl);
	    }
	    n = getnum(p->lrecl);
	    if (n < 0) {
		return -1;
	    }
	    p->lrecl = n;
	}

	if (p->host_type == HT_TSO) {

	    printf("[optional] Destination file block size: ");
	    if (p->blksize) {
		printf("[%d] ", p->blksize);
	    }
	    n = getnum(p->blksize);
	    if (n < 0) {
		return -1;
	    }
	    p->blksize = n;

	    printf("\n");
	    for (;;) {
		printf("[optional] Destination file "
			"allocation type:\n"
			" (default/tracks/cylinders/avblock) ");
		if (p->units) {
		    printf("[%s] ", ft_decode_units(p->units));
		}
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
		    return -1;
		}
		if (!inbuf[0]) {
		    break;
		}
		if (ft_encode_units(inbuf, &p->units)) {
		    break;
		}
	    }

	    if (p->units != DEFAULT_UNITS) {
		printf("\n");
		for (;;) {
		    printf("Destination file primary space: ");
		    if (p->primary_space) {
			printf("[%d] ", p->primary_space);
		    }
		    n = getnum(p->primary_space);
		    if (n < 0) {
			return -1;
		    }
		    if (n > 0) {
			p->primary_space = n;
			break;
		    }
		}

		printf("[optional] Destination file secondary space: ");
		if (p->secondary_space) {
		    printf("[%d] ", p->secondary_space);
		}
		n = getnum(p->secondary_space);
		if (n < 0) {
		    return -1;
		}
		p->secondary_space = n;

		if (p->units == AVBLOCK) {
		    for (;;) {
			printf("Destination file avblock size: ");
			if (p->avblock) {
			    printf("[%d] ", p->avblock);
			}
			n = getnum(p->avblock);
			if (n < 0) {
			    return -1;
			}
			if (n > 0) {
			    p->avblock = n;
			    break;
			}
		    }
		}
	    }
	}
    }

    if (!HOST_FLAG(STD_DS_HOST)) {
	printf("\n");
	for (;;) {
	    int nsize;

	    printf("DFT buffer size: [%d] ", p->dft_buffersize);
	    if (p->avblock) {
		printf("[%d] ", p->avblock);
	    }
	    n = getnum(p->dft_buffersize);
	    if (n < 0) {
		return -1;
	    }
	    nsize = set_dft_buffersize(n);
	    if (nsize != n) {
		printf("Size changed to %d.\n", nsize);
	    }
	    p->dft_buffersize = nsize;
	    break;
	}
    }

    printf("\nFile Transfer Summary:\n");
    if (p->receive_flag) {
	printf(" Source file on Host: %s\n", p->host_filename);
	printf(" Destination file on Workstation: %s\n", p->local_filename);
    } else {
	printf(" Source file on workstation: %s\n", p->local_filename);
	printf(" Destination file on Host: %s\n", p->host_filename);
    }
    printf(" Host type: ");
    switch (p->host_type) {
    case HT_TSO:
	printf("TSO");
	break;
    case HT_VM:
	printf("VM/CMS");
	break;
    case HT_CICS:
	printf("CICS");
	break;
    }
    printf(" \n Transfer mode: %s", p->ascii_flag? "ASCII": "Binary");
    if (p->ascii_flag) {
	switch (cr_mode) {
	case CR_REMOVE:
	    printf(", remove CRs");
	    break;
	case CR_ADD:
	    printf(", add CRs");
	    break;
	case CR_KEEP:
	    break;
	}
	if (p->remap_flag) {
	    printf(", remap text");
	} else {
	    printf(", don't remap text");
	}
#if defined(_WIN32) /*[*/
	if (p->remap_flag) {
	    printf(", Windows code page %d", p->windows_codepage);
	}
#endif /*]*/
	printf("\n");
    } else {
	printf("\n");
    }
    if (p->receive_flag) {
	printf(" If destination file exists, ");
	switch (fe_mode) {
	case FE_KEEP:
	    printf("abort the transfer\n");
	    break;
	case FE_REPLACE:
	    printf("replace it\n");
	    break;
	case FE_APPEND:
	    printf("append to it\n");
	    break;
	}
    }
    if (!p->receive_flag &&
	    (p->recfm != DEFAULT_RECFM || p->lrecl || p->primary_space ||
	     p->secondary_space)) {

	printf(" Destination file:\n");

	switch (p->recfm) {
	case DEFAULT_RECFM:
	    break;
	case RECFM_FIXED:
	    printf("  Record format: fixed\n");
	    break;
	case RECFM_VARIABLE:
	    printf("  Record format: variable\n");
	    break;
	case RECFM_UNDEFINED:
	    printf("  Record format: undefined\n");
	    break;
	}
	if (p->lrecl) {
	    printf("  Logical record length: %d\n", p->lrecl);
	}
	if (p->blksize) {
	    printf("  Block size: %d\n", p->blksize);
	}
	if (p->primary_space || p->secondary_space) {
	    printf("  Allocation:");
	    if (p->primary_space) {
		printf(" primary %d", p->primary_space);
	    }
	    if (p->secondary_space) {
		printf(" secondary %d", p->secondary_space);
	    }
	    switch (p->units) {
	    case DEFAULT_UNITS:
		break;
	    case TRACKS:
		printf(" tracks");
		break;
	    case CYLINDERS:
		printf(" cylinders");
		break;
	    case AVBLOCK:
		printf(" avblock %d", p->avblock);
		break;
	    }
	    printf("\n");
	}
    }
    if (!HOST_FLAG(STD_DS_HOST)) {
	printf(" DFT buffer size: %d\n", p->dft_buffersize);
    }

    printf("\nContinue? (y/n) [y] ");
    if (getyn(1) <= 0) {
	return -1;
    }

    /* Let it go. */
    return 0;
}

/* Help for the interactive Transfer action. */
void
ft_help(bool as_action _is_unused)
{
    ft_conf_t conf;
    char *s;

    memset(&conf, 0, sizeof(ft_conf_t));
    ft_init_conf(&conf);
    action_output(
"Syntax:\n\
  To be prompted interactively for parameters:\n\
    Transfer\n\
  To specify parameters on the command line:\n\
    Transfer <keyword>=<value>...\n\
Keywords:");

    action_output(
"  Direction=send|receive               default '%s'",
	    conf.receive_flag? "send": "receive");

    if ((conf.receive_flag && conf.host_filename) ||
	    (!conf.receive_flag && conf.local_filename)) {
	s = lazyaf("default '%s'",
		conf.receive_flag? conf.host_filename: conf.local_filename);
    } else {
	s = "(required)";
    }
    action_output(
"  HostFile=<path>                      %s", s);

    if ((!conf.receive_flag && conf.host_filename) ||
	    (conf.receive_flag && conf.local_filename)) {
	s = lazyaf("default '%s'",
		conf.receive_flag? conf.local_filename: conf.host_filename);
    } else {
	s = "(required)";
    }
    action_output(
"  LocalFile=<path>                     %s", s);

    action_output(
"  Host=tso|vm                          default '%s'",
	    ft_decode_host_type(conf.host_type));
    action_output(
"  Mode=ascii|binary                    default '%s'",
	    conf.ascii_flag? "ascii": "binary");
    action_output(
"  Cr=remove|add|keep                   default '%s'",
	    conf.cr_flag? (conf.receive_flag? "add": "remove"): "keep");
    action_output(
"  Remap=yes|no                         default '%s'",
	    conf.remap_flag? "yes": "no");
#if defined(_WIN32) /*[*/
    action_output(
"  WindowsCodePage=<n>                  default %d",
	    conf.windows_codepage);
#endif /*]*/
    action_output(
"  Exist=keep|replace|append            default '%s'",
	    conf.allow_overwrite? "replace":
		(conf.append_flag? "append": "keep"));
    action_output(
"  Recfm=fixed|variable|undefined       for Direction=send");
    if (conf.recfm != DEFAULT_RECFM) {
	action_output(
"                                        default '%s'",
		ft_decode_recfm(conf.recfm));
    }
    action_output(
"  Lrecl=<n>                            for Direction=send");
    if (conf.lrecl) {
	action_output(
"                                        default %d",
		conf.lrecl);
    }
    action_output(
"  Blksize=<n>                          for Direction=send Host=tso");
    if (conf.blksize) {
	action_output(
"                                        default %d",
		conf.blksize);
    }
    action_output(
"  Allocation=tracks|cylinders|avblock  for Direction=send Host=tso");
    if (conf.units != DEFAULT_UNITS) {
	action_output(
"                                        default '%s'",
		ft_decode_units(conf.units));
    }
    action_output(
"  PrimarySpace=<n>                     for Direction=send Host=tso");
    if (conf.primary_space) {
	action_output(
"                                        default %d",
		conf.primary_space);
    }
    action_output(
"  SecondarySpace=<n>                   for Direction=send Host=tso");
    if (conf.secondary_space) {
	action_output(
"                                        default %d",
		conf.secondary_space);
    }
    action_output(
"  Avblock=<n>                          for Direction=send Host=tso Allocation=avblock");
    if (conf.avblock) {
	action_output(
"                                        default %d",
		conf.avblock);
    }
    action_output(
"Note that to embed a space in a value, you must quote the keyword, e.g.:\n\
  Transfer Direction=send LocalFile=/tmp/foo \"HostFile=foo text a\" Host=vm");

    if (conf.local_filename) {
	Free(conf.local_filename);
    }
    if (conf.host_filename) {
	Free(conf.host_filename);
    }
}
