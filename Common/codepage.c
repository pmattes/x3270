/*
 * Copyright (c) 1993-2024 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	codepage.c
 *		This module handles host code pages.
 */

#include "globals.h"

#include "3270ds.h"
#include "resources.h"
#include "appres.h"

#include "actions.h"
#include "codepage.h"
#include "popups.h"
#include "screen.h"
#include "toggles.h"
#include "txa.h"
#include "unicodec.h"
#include "unicode_dbcs.h"
#include "utf8.h"
#include "utils.h"

#include <locale.h>
#if !defined(_WIN32) && defined(HAVE_LANGINFO_H) /*[*/
# include <langinfo.h>
#endif /*]*/

#if defined(__CYGWIN__) /*[*/
# include <w32api/windows.h>
#undef _WIN32
#endif /*]*/

#if defined(_WIN32) /*[*/
# define LOCAL_CODEPAGE	appres.local_cp
#else /*][*/
# define LOCAL_CODEPAGE	0
#endif

/* Globals. */
bool codepage_changed = false;
#define DEFAULT_CGEN	0x02b90000
#define DEFAULT_CSET	0x00000025
unsigned long cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
unsigned long cgcsgid_dbcs = 0L;

/* Statics. */
static enum cs_result codepage_init2(const char *cpname, const char *realname,
	const char *codepage, const char *cgcsgid, bool is_dbcs);
static void set_cgcsgids(const char *spec);
static bool set_cgcsgid(char *spec, unsigned long *idp);
static void set_codepage_number(char *codepage);
static void set_codepage_name(const char *cpname);

static char *codepage_number = NULL;
static char *codepage_name = NULL;
static char *canon_codepage = NULL;

#if !defined(_WIN32) && !defined(HAVE_LANGINFO_H) /*[*/
/*
 * Guess the codeset based on environment variables. */
static char *
guess_codeset(void)
{
    char *e = getenv("LC_CTYPE");

    if (e == NULL) {
	e = getenv("LANG");
    }
    if (e != NULL) {
	char *dot = strchr(e, '.');

	if (dot != NULL) {
	    return dot + 1;
	}
    }
    return (char *)"ASCII";
}
#endif /*]*/

/*
 * Change host code pages.
 */
enum cs_result
codepage_init(const char *cpname)
{
    enum cs_result rc;
    char *codeset_name;
    const char *codepage;
    const char *cgcsgid;
    const char *dbcs_cgcsgid = NULL;
    const char *realname;
    bool is_dbcs;

#if !defined(_WIN32) /*[*/
    /* Get all of the locale stuff right. */
    setlocale(LC_ALL, "");

# if defined(HAVE_LANGINFO_H) /*[*/
    /* Figure out the locale code set (character set encoding). */
    codeset_name = nl_langinfo(CODESET);
# else /*][*/
    /* No nl_langinfo. See if there's anything in the environment. */
    codeset_name = guess_codeset();
# endif /*]*/
#else /*][*/
    codeset_name = txAsprintf("CP%d", appres.local_cp);
#endif /*]*/
    set_codeset(codeset_name, appres.utf8);

    if (cpname == NULL) {
	cpname = "bracket";
    }

    if (!set_uni(cpname, LOCAL_CODEPAGE, &codepage, &cgcsgid, &realname,
		&is_dbcs)) {
	return CS_NOTFOUND;
    }
    if (appres.sbcs_cgcsgid != NULL) {
	cgcsgid = appres.sbcs_cgcsgid; /* override */
    }
    if (set_uni_dbcs(cpname, &dbcs_cgcsgid)) {
	if (appres.dbcs_cgcsgid != NULL) {
	    dbcs_cgcsgid = appres.dbcs_cgcsgid; /* override */
	}
	cgcsgid = txAsprintf("%s+%s", cgcsgid, dbcs_cgcsgid);
    }

    rc = codepage_init2(cpname, realname, codepage, cgcsgid, is_dbcs);
    if (rc != CS_OKAY) {
	return rc;
    }

    return CS_OKAY;
}

/* Set a CGCSGID.  Return true for success, false for failure. */
static bool
set_cgcsgid(char *spec, unsigned long *r)
{
    unsigned long cp;
    char *ptr;

    if (spec != NULL &&
	    (cp = strtoul(spec, &ptr, 0)) &&
	    ptr != spec &&
	    *ptr == '\0') {
	if (!(cp & ~0xffffL)) {
	    *r = DEFAULT_CGEN | cp;
	} else {
	    *r = cp;
	}
	return true;
    } else {
	return false;
    }
}

/* Set the CGCSGIDs. */
static void
set_cgcsgids(const char *spec)
{
    int n_ids = 0;
    char *spec_copy;
    char *buf;
    char *token;

    if (spec != NULL) {
	buf = spec_copy = NewString(spec);
	while (n_ids >= 0 && (token = strtok(buf, "+")) != NULL) {
	    unsigned long *idp = NULL;

	    buf = NULL;
	    switch (n_ids) {
	    case 0:
		idp = &cgcsgid;
		break;
	    case 1:
		idp = &cgcsgid_dbcs;
		break;
	    default:
		popup_an_error("Extra CGCSGID(s), ignoring");
		break;
	    }
	    if (idp == NULL)
		break;
	    if (!set_cgcsgid(token, idp)) {
		popup_an_error("Invalid CGCSGID '%s', ignoring", token);
		n_ids = -1;
		break;
	    }
	    n_ids++;
	}
	Free(spec_copy);
	if (n_ids > 0) {
	    return;
	}
    }

    if (appres.sbcs_cgcsgid != NULL) {
	cgcsgid = strtoul(appres.sbcs_cgcsgid, NULL, 0);
    } else {
	cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
    }
    if (appres.dbcs_cgcsgid != NULL) {
	cgcsgid_dbcs = strtoul(appres.dbcs_cgcsgid, NULL, 0);
    } else {
	cgcsgid_dbcs = 0L;
    }
}

/* Set the codepage number. */
static void
set_codepage_number(char *codepage)
{
    if (codepage == NULL) {
	Replace(codepage_number, NewString("037"));
	return;
    }
    if (codepage_number == NULL || strcmp(codepage_number, codepage)) {
	Replace(codepage_number, NewString(codepage));
    }
}

/**
 * Return the canonical form of a code page, given a resource value.
 * This is needed because the resource definition may be a valid alias, but
 * we always want to display and return the canonical name.
 *
 * @param[in] res	Resource value, or NULL
 *
 * @returns Canonical representation.
 */
static const char *
canonical_cs(const char *res)
{
    const char *canon;

    if (res == NULL) {
	return NULL;
    }
    canon = canonical_codepage(res);
    if (canon == NULL) {
	return NULL;
    }
    return canon;
}

/* Set the code page name. */
static void
set_codepage_name(const char *cpname)
{
    const char *canon;

    if (cpname == NULL) {
	Replace(codepage_name, NewString("bracket"));
	codepage_changed = false;
	return;
    }

    canon = canonical_cs(cpname);
    if (canon == NULL) {
	canon = cpname;
    }

    if ((codepage_name != NULL && strcmp(codepage_name, canon)) ||
	    (appres.codepage != NULL && strcmp(appres.codepage, canon))) {
	Replace(codepage_name, NewString(canon));
	codepage_changed = true;
    }
}

/* Code page init, part 2. */
static enum cs_result
codepage_init2(const char *cpname, const char *realname, const char *codepage,
	const char *cgcsgid, bool is_dbcs)
{
    /* Can't swap DBCS modes while connected. */
    if (IN_3270 && is_dbcs != dbcs) {
	popup_an_error("Cannot change DBCS modes while connected");
	return CS_ILLEGAL;
    }

    if (!screen_new_display_charsets(realname)) {
	return CS_PREREQ;
    }

    /* Set the global DBCS mode. */
    dbcs = is_dbcs;

    /* Set up the cgcsgids. */
    set_cgcsgids(cgcsgid);

    /* Set up the code page number. */
    set_codepage_number((char *)codepage);

    /* Set up the code page name. */
    set_codepage_name(cpname);

    /* Remember the canonical code page name. */
    Replace(canon_codepage, NewString(realname));

    return CS_OKAY;
}

/* Return the current host codepage. */
const char *
get_codepage_number(void)
{
    return (codepage_number != NULL)? codepage_number: "037";
}

/* Return the canonical host codepage name. */
const char *
get_canonical_codepage(void)
{
    return (canon_codepage != NULL)? canon_codepage: "cp037";
}

/* Return the current code page name. */
const char *
get_codepage_name(void)
{
    return (codepage_name != NULL)? codepage_name:
	((appres.codepage != NULL)? appres.codepage: "bracket");
}

/**
 * Extended toggle for the host code page.
 *
 * @param[in] name	Toggle name.
 * @param[in] value	New value, might be NULL.
 * @param[in] flags	Operation flags.
 * @param[in] ia	Cause of operation.
 *
 * @returns toggle_upcall_ret_t
 */
static toggle_upcall_ret_t
toggle_codepage(const char *name _is_unused, const char *value, unsigned flags _is_unused, ia_t ia _is_unused)
{
    enum cs_result result;

    if (value == NULL) {
	value = "bracket";
    }
    result = codepage_init(value);
    switch (result) {
    case CS_OKAY:
	st_changed(ST_CODEPAGE, true);
	codepage_changed = true;
	Replace(appres.codepage, NewString(canonical_cs(value)));
	return TU_SUCCESS;
    case CS_NOTFOUND:
	popup_an_error("Cannot find definition of host code page \"%s\"", value);
	return TU_FAILURE;
    case CS_BAD:
	popup_an_error("Invalid code page definition for \"%s\"", value);
	return TU_FAILURE;
    case CS_PREREQ:
	popup_an_error("No fonts for host code page \"%s\"", value);
	return TU_FAILURE;
    default:
    case CS_ILLEGAL:
	/* error already popped up */
	return TU_FAILURE;
    }
}

/*
 * Codepage module registration.
 */
void
codepage_register(void)
{
    /* Register the toggle. */
    register_extended_toggle(ResCodePage, toggle_codepage, NULL, canonical_cs,
	    (void **)&appres.codepage, XRM_STRING);
}
