/*
 * Copyright (c) 2007-2009, 2013-2015 Paul Mattes.
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
#include "ft_private.h"
#include "icmdc.h"
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
    	int sl;
	char *s;

	fflush(stdout);

	/* Get the raw input. */
    	if (fgets(buf, size, stdin) == NULL)
	    	return NULL;

	/* Trim trailing white space. */
	sl = strlen(buf);
	while (sl && isspace(buf[sl - 1]))
	    	buf[--sl] = '\0';

	/* Trim leading white space. */
	s = buf;
	while (*s && isspace(*s)) {
	    	s++;
		sl--;
	}
	if (s != buf)
	    	memmove(buf, s, sl + 1);

	/* Check for 'quit'. */
	if (!strcasecmp(buf, "quit"))
	    	return NULL;

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

/*
 * Interactive file transfer command.
 * Called from Transfer_action.  Returns an updated ft_private.
 * Returns 0 for success, -1 for failure.
 */
int
interactive_transfer(ft_private_t *p)
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

    printf(" 'send' means copy a file from this workstation to the host.\n");
    printf(" 'receive' means copy a file from the host to this workstation.\n");
    for (;;) {
	printf("Direction (send/receive) [%s]: ",
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

    for (;;) {
	printf("Name of source file on %s",
		p->receive_flag? "the host": "this workstation");
	if (p->receive_flag && p->host_filename) {
	    printf(" [%s]", p->host_filename);
	} else if (!p->receive_flag && p->local_filename) {
	    printf(" [%s]", p->local_filename);
	}
	printf(": ");
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0] &&
		((p->receive_flag && p->host_filename) ||
		 (!p->receive_flag && p->local_filename))) {
	    break;
	}
	if (p->receive_flag) {
	    Replace(p->host_filename, NewString(inbuf));
	} else {
	    Replace(p->local_filename, NewString(inbuf));
	}
	break;
    }

    for (;;) {
	printf("Name of destination file on %s",
		p->receive_flag? "this workstation": "the host");
	if (!p->receive_flag && p->host_filename) {
	    printf(" [%s]", p->host_filename);
	} else if (p->receive_flag && p->local_filename) {
	    printf(" [%s]", p->local_filename);
	}
	printf(": ");
	if (get_input(inbuf, sizeof(inbuf)) == NULL) {
	    return -1;
	}
	if (!inbuf[0] &&
		((!p->receive_flag && p->host_filename) ||
		 (p->receive_flag && p->local_filename))) {
	    break;
	}
	if (!p->receive_flag) {
	    Replace(p->host_filename, NewString(inbuf));
	} else {
	    Replace(p->local_filename, NewString(inbuf));
	}
	break;
    }

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

    printf("\
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
#if defined(_WIN32) /*[*/
	for (;;) {
	    int cp;

	    printf("Windows code page for transfer: [%d] ",
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
#endif /*]*/
	printf("\
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
	printf("\
 For ASCII transfers, "
#if defined(WC3270) /*[*/
	       "wc3270"
#else /*][*/
	       "c3270"
#endif /*]*/
		       " can either remap the text to ensure as\n\
 accurate a translation between "
#if defined(WC3270) /*[*/
			"Windows code page %d"
#else /*][*/
			"%s"
#endif /*]*/
					     " and EBCDIC code\n\
 page %s as possible, or it can transfer text as-is and leave all\n\
 translation to the IND$FILE program on the host.\n\
 'yes' means that text will be translated.\n\
 'no' means that text will be transferred as-is.\n",
#if defined(WC3270) /*[*/
	    p->windows_codepage,
#else /*][*/
	    locale_codeset,
#endif /*]*/
	    get_host_codepage());
	for (;;) {
	    printf("Remap character set: (yes/no) [%s] ",
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
    }

    if (p->receive_flag) {
	printf("\
If the destination file exists, you can choose to keep it (and abort the\n\
transfer), replace it, or append the source file to it.\n");
	if (p->allow_overwrite) {
	    default_fe = "replace";
	} else if (p->append_flag) {
	    default_fe = "append";
	} else {
	    default_fe = "keep";
	}
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
	    for (;;) {
		printf("[optional] Destination file record "
			"format (default/fixed/variable/undefined) [%s]: ",
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

	    printf("[optional] Destination file logical record length");
	    if (p->lrecl) {
		printf(" [%d]", p->lrecl);
	    }
	    printf(": ");
	    n = getnum(p->lrecl);
	    if (n < 0) {
		return -1;
	    }
	    p->lrecl = n;
	}

	if (p->host_type == HT_TSO) {

	    printf("[optional] Destination file block size");
	    if (p->blksize) {
		printf(" [%d]", p->blksize);
	    }
	    printf(": ");
	    n = getnum(p->blksize);
	    if (n < 0) {
		return -1;
	    }
	    p->blksize = n;

	    for (;;) {
		printf("[optional] Destination file "
			"allocation type "
			"(default/tracks/cylinders/avblock)");
		if (p->units) {
		    printf(" [%s]", ft_decode_units(p->units));
		}
		printf(": ");
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
		for (;;) {
		    printf("Destination file primary space");
		    if (p->primary_space) {
			printf(" [%d]", p->primary_space);
		    }
		    printf(": ");
		    n = getnum(p->primary_space);
		    if (n < 0) {
			return -1;
		    }
		    if (n > 0) {
			p->primary_space = n;
			break;
		    }
		}

		printf("[optional] Destination file secondary space");
		if (p->secondary_space) {
		    printf(" [%d]", p->secondary_space);
		}
		printf(": ");
		n = getnum(p->secondary_space);
		if (n < 0) {
		    return -1;
		}
		p->secondary_space = n;

		if (p->units == AVBLOCK) {
		    for (;;) {
			printf("Destination file avblock size");
			if (p->avblock) {
			    printf(" [%d]", p->avblock);
			}
			printf(": ");
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
	printf(", Windows code page %d", p->windows_codepage);
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

    printf("\nContinue? (y/n) [y] ");
    if (getyn(1) <= 0) {
	return -1;
    }

    /* Let it go. */
    return 0;
}
