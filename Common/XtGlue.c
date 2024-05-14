/*
 * Copyright (c) 1999-2024 Paul Mattes.
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

/* glue for missing Xt code */

#include "globals.h"
#include "glue.h"
#include "appres.h"
#include "latin1.h"
#include "task.h"
#include "trace.h"
#include "txa.h"
#include "utils.h"
#if defined(_WIN32) /*[*/
# include "w3misc.h"
# include "xio.h"
#endif /*]*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if !defined(_WIN32) /*[*/
# include <sys/wait.h>
#endif /*]*/

#if defined(SEPARATE_SELECT_H) /*[*/
# include <sys/select.h>
#endif /*]*/

#define InputReadMask	0x1
#define InputExceptMask	0x2
#define InputWriteMask	0x4

#define MILLION		1000000L

void (*Error_redirect)(const char *) = NULL;
void (*Warning_redirect)(const char *) = NULL;

void
Error(const char *s)
{
    if (Error_redirect != NULL) {
	(*Error_redirect)(s);
	return;
    }
    fprintf(stderr, "Error: %s\n", s);
    fflush(stderr);
    exit(1);
}

void
Warning(const char *s)
{
    if (Warning_redirect != NULL) {
	(*Warning_redirect)(s);
    } else {
	fprintf(stderr, "Warning: %s\n", s);
	fflush(stderr);
    }
}

static struct {
    /*const*/ char *name;	/* not const because of ancient X11 API */
    latin1_symbol_t key;
} latin1[] = {
    /* HTML entities and X11 KeySym names. */
    { "sp",            latin1_sp },
    {  "space",        latin1_sp },
    { "excl",          latin1_excl },
    {  "exclam",       latin1_excl },
    { "quot",          latin1_quot },
    {  "quotedbl",     latin1_quot },
    { "num",           latin1_num },
    {  "numbersign",   latin1_num },
    { "dollar",        latin1_dollar },
    { "percnt",        latin1_percnt },
    {  "percent",      latin1_percnt },
    { "amp",           latin1_amp },
    {  "ampersand",    latin1_amp },
    { "apos",          latin1_apos },
    {  "apostrophe",   latin1_apos },
    {  "quoteright",   latin1_apos },
    { "lpar",          latin1_lpar },
    {  "parenleft",    latin1_lpar },
    { "rpar",          latin1_rpar },
    {  "parenright",   latin1_rpar },
    { "ast",           latin1_ast },
    {  "asterisk",     latin1_ast },
    { "plus",          latin1_plus },
    { "comma",         latin1_comma },
    { "minus",         latin1_minus },
    {  "hyphen",       latin1_minus }, /* There is a conflict here between
					  HTML and X11, which uses 'hyphen'
					  for shy (U+00AD). HTML wins. */
    { "period",        latin1_period },
    { "sol",           latin1_sol },
    {  "slash",        latin1_sol },
    { "0",             latin1_0 },
    { "1",             latin1_1 },
    { "2",             latin1_2 },
    { "3",             latin1_3 },
    { "4",             latin1_4 },
    { "5",             latin1_5 },
    { "6",             latin1_6 },
    { "7",             latin1_7 },
    { "8",             latin1_8 },
    { "9",             latin1_9 },
    { "colon",         latin1_colon },
    { "semi",          latin1_semi },
    {  "semicolon",    latin1_semi },
    { "lt",            latin1_lt },
    {  "less",         latin1_lt },
    { "equals",        latin1_equals },
    {  "equal",        latin1_equals },
    { "gr",            latin1_gt },
    {  "greater",      latin1_gt },
    { "quest",         latin1_quest },
    {  "question",     latin1_quest },
    { "commat",        latin1_commat },
    {  "at",           latin1_commat },
    { "A",             latin1_A },
    { "B",             latin1_B },
    { "C",             latin1_C },
    { "D",             latin1_D },
    { "E",             latin1_E },
    { "F",             latin1_F },
    { "G",             latin1_G },
    { "H",             latin1_H },
    { "I",             latin1_I },
    { "J",             latin1_J },
    { "K",             latin1_K },
    { "L",             latin1_L },
    { "M",             latin1_M },
    { "N",             latin1_N },
    { "O",             latin1_O },
    { "P",             latin1_P },
    { "Q",             latin1_Q },
    { "R",             latin1_R },
    { "S",             latin1_S },
    { "T",             latin1_T },
    { "U",             latin1_U },
    { "V",             latin1_V },
    { "W",             latin1_W },
    { "X",             latin1_X },
    { "Y",             latin1_Y },
    { "Z",             latin1_Z },
    { "lsqb",          latin1_lsqb },
    {  "bracketleft",  latin1_lsqb },
    { "bsol",          latin1_bsol },
    {  "backslash",    latin1_bsol },
    { "rsqb",          latin1_rsqb },
    {  "bracketright", latin1_rsqb },
    { "circ",          latin1_circ },
    {  "asciicircum",  latin1_circ },
    { "lowbar",        latin1_lowbar },
    {  "horbar",       latin1_lowbar },
    {  "underscore",   latin1_lowbar },
    { "grave",         latin1_grave },
    {  "quoteleft",    latin1_grave },
    { "a",             latin1_a },
    { "b",             latin1_b },
    { "c",             latin1_c },
    { "d",             latin1_d },
    { "e",             latin1_e },
    { "f",             latin1_f },
    { "g",             latin1_g },
    { "h",             latin1_h },
    { "i",             latin1_i },
    { "j",             latin1_j },
    { "k",             latin1_k },
    { "l",             latin1_l },
    { "m",             latin1_m },
    { "n",             latin1_n },
    { "o",             latin1_o },
    { "p",             latin1_p },
    { "q",             latin1_q },
    { "r",             latin1_r },
    { "s",             latin1_s },
    { "t",             latin1_t },
    { "u",             latin1_u },
    { "v",             latin1_v },
    { "w",             latin1_w },
    { "x",             latin1_x },
    { "y",             latin1_y },
    { "z",             latin1_z },
    { "lcub",          latin1_lcub },
    {  "braceleft",    latin1_lcub },
    { "verbar",        latin1_verbar },
    {  "bar",          latin1_verbar },
    { "rcub",          latin1_rcub },
    {  "braceright",   latin1_rcub },
    { "tilde",         latin1_tilde },
    {  "asciitilde",   latin1_tilde },
    { "nbsp",          latin1_nbsp },
    {  "nobreakspace", latin1_nbsp },
    { "iexcl",         latin1_iexcl },
    {  "exclamdown",   latin1_iexcl },
    { "cent",          latin1_cent },
    { "pound",         latin1_pound },
    {  "sterling",     latin1_pound },
    { "curren",        latin1_curren },
    {  "currency",     latin1_curren },
    { "yen",           latin1_yen },
    { "brkbar",        latin1_brkbar },
    {  "brvbar",       latin1_brkbar },
    {  "brokenbar",    latin1_brkbar },
    { "sect",          latin1_sect },
    {  "section",      latin1_sect },
    { "uml",           latin1_uml },
    {  "die",          latin1_uml },
    {  "diaeresis",    latin1_uml },
    { "copy",          latin1_copy },
    {  "copyright",    latin1_copy },
    { "ordf",          latin1_ordf },
    {  "ordfeminine",  latin1_ordf },
    { "laquo",         latin1_laquo },
    {  "guillemotleft",latin1_laquo },
    { "not",           latin1_not },
    {  "notsign",      latin1_not },
    { "shy",           latin1_shy },
    { "reg",           latin1_reg },
    {  "registered",   latin1_reg },
    { "macr",          latin1_macr },
    {  "hibar",        latin1_macr },
    {  "macron",       latin1_macr },
    { "deg",           latin1_deg },
    {  "degree",       latin1_deg },
    { "plusmn",        latin1_plusmn },
    {  "plusminus",    latin1_plusmn },
    { "sup2",          latin1_sup2 },
    {  "twosuperior",  latin1_sup2 },
    { "sup3",          latin1_sup3 },
    {  "threesuperior",latin1_sup3 },
    { "acute",         latin1_acute },
    { "micro",         latin1_micro },
    {  "mu",           latin1_micro },
    { "para",          latin1_para },
    {  "paragraph",    latin1_para },
    { "middot",        latin1_middot },
    {  "periodcentered",latin1_middot },
    { "cedil",         latin1_cedil },
    {  "cedilla",      latin1_cedil },
    { "sup1",          latin1_sup1 },
    {  "onesuperior",  latin1_sup1 },
    { "ordm",          latin1_ordm },
    {  "masculine",    latin1_ordm },
    { "raquo",         latin1_raquo },
    {  "guillemotright",latin1_raquo },
    { "frac14",        latin1_frac14 },
    {  "onequarter",   latin1_frac14 },
    { "frac12",        latin1_frac12 },
    {  "half",         latin1_frac12 },
    {  "onehalf",      latin1_frac12 },
    { "frac34",        latin1_frac34 },
    {  "threequarters",latin1_frac34 },
    { "iquest",        latin1_iquest },
    {  "questiondown", latin1_iquest },
    { "Agrave",        latin1_Agrave },
    { "Aacute",        latin1_Aacute },
    { "Acirc",         latin1_Acirc },
    {  "Acircumflex",  latin1_Acirc },
    { "Atilde",        latin1_Atilde },
    { "Auml",          latin1_Auml },
    {  "Adiaeresis",   latin1_Auml },
    { "Aring",         latin1_Aring },
    { "AElig",         latin1_AElig },
    {  "AE",           latin1_AElig },
    { "Ccedil",        latin1_Ccedil },
    {  "Ccedilla",     latin1_Ccedil },
    { "Egrave",        latin1_Egrave },
    { "Eacute",        latin1_Eacute },
    { "Ecirc",         latin1_Ecirc },
    {  "Ecircumflex",  latin1_Ecirc },
    { "Euml",          latin1_Euml },
    {  "Ediaeresis",   latin1_Euml },
    { "Igrave",        latin1_Igrave },
    { "Iacute",        latin1_Iacute },
    { "Icirc",         latin1_Icirc },
    {  "Icircumflex",  latin1_Icirc },
    { "Iuml",          latin1_Iuml },
    {  "Idiaeresis",   latin1_Iuml },
    { "ETH",           latin1_ETH },
    {  "Eth",          latin1_ETH },
    { "Ntilde",        latin1_Ntilde },
    { "Ograve",        latin1_Ograve },
    { "Oacute",        latin1_Oacute },
    { "Ocirc",         latin1_Ocirc },
    {  "Ocircumflex",  latin1_Ocirc },
    { "Otilde",        latin1_Otilde },
    { "Ouml",          latin1_Ouml },
    {  "Odiaeresis",   latin1_Ouml },
    { "times",         latin1_times },
    {  "multiply",     latin1_times },
    { "Oslash",        latin1_Oslash },
    {  "Ooblique",     latin1_Oslash },
    { "Ugrave",        latin1_Ugrave },
    { "Uacute",        latin1_Uacute },
    { "Ucirc",         latin1_Ucirc },
    {  "Ucircumflex",  latin1_Ucirc },
    { "Uuml",          latin1_Uuml },
    {  "Udiaeresis",   latin1_Uuml },
    { "Yacute",        latin1_Yacute },
    { "THORN",         latin1_THORN },
    {  "Thorn",        latin1_THORN },
    { "szlig",         latin1_szlig },
    {  "ssharp",       latin1_szlig },
    { "agrave",        latin1_agrave },
    { "aacute",        latin1_aacute },
    { "acirc",         latin1_acirc },
    {  "acircumflex",  latin1_acirc },
    { "atilde",        latin1_atilde },
    { "auml",          latin1_auml },
    {  "adiaeresis",   latin1_auml },
    { "aring",         latin1_aring },
    { "aelig",         latin1_aelig },
    {  "ae",           latin1_aelig },
    { "ccedil",        latin1_ccedil },
    {  "ccedilla",     latin1_ccedil },
    { "egrave",        latin1_egrave },
    { "eacute",        latin1_eacute },
    { "ecirc",         latin1_ecirc },
    {  "ecircumflex",  latin1_ecirc },
    { "euml",          latin1_euml },
    {  "ediaeresis",   latin1_euml },
    { "igrave",        latin1_igrave },
    { "iacute",        latin1_iacute },
    { "icirc",         latin1_icirc },
    {  "icircumflex",  latin1_icirc },
    { "iuml",          latin1_iuml },
    {  "idiaeresis",   latin1_iuml },
    { "eth",           latin1_eth },
    { "ntilde",        latin1_ntilde },
    { "ograve",        latin1_ograve },
    { "oacute",        latin1_oacute },
    { "ocirc",         latin1_ocirc },
    {  "ocircumflex",  latin1_ocirc },
    { "otilde",        latin1_otilde },
    { "ouml",          latin1_ouml },
    {  "odiaeresis",   latin1_ouml },
    { "divide",        latin1_divide },
    {  "division",     latin1_divide },
    { "oslash",        latin1_oslash },
    { "ugrave",        latin1_ugrave },
    { "uacute",        latin1_uacute },
    { "ucirc",         latin1_ucirc },
    {  "ucircumflex",  latin1_ucirc },
    { "uuml",          latin1_uuml },
    {  "udiaeresis",   latin1_uuml },
    { "yacute",        latin1_yacute },
    { "thorn",         latin1_thorn },
    { "yuml",          latin1_yuml },
    {  "ydiaeresis",   latin1_yuml },

    /*
     * The following are, umm, hacks to allow symbolic names for
     * control codes.
     */
#if !defined(_WIN32) /*[*/
    { "BackSpace",     0x08 },
    { "Tab",           0x09 },
    { "LineFeed",      0x0a },
    { "Return",        0x0d },
    { "Escape",        0x1b },
    { "Delete",        0x7f },
#endif /*]*/

    { NULL,            0 }
};

ks_t
string_to_key(char *s)
{
    int i;

    if (strlen(s) == 1 && (*(unsigned char *)s & 0x7f) > ' ') {
	return *(unsigned char *)s;
    }
    for (i = 0; latin1[i].name != NULL; i++) {
	if (!strcmp(s, latin1[i].name)) {
	    return latin1[i].key;
	}
    }
    return KS_NONE;
}

char *
key_to_string(ks_t k)
{
    int i;

    for (i = 0; latin1[i].name != NULL; i++) {
	if (latin1[i].key == k) {
	    return latin1[i].name;
	}
    }
    return NULL;
}

/* Timeouts. */

#if defined(_WIN32) /*[*/
static void
ms_ts(unsigned long long *u)
{
    FILETIME t;

    /* Get the system time, in 100ns units. */
    GetSystemTimeAsFileTime(&t);
    memcpy(u, &t, sizeof(unsigned long long));

    /* Divide by 10,000 to get ms. */
    *u /= 10000ULL;
}
#endif /*]*/

typedef struct timeout {
    struct timeout *next;
#if defined(_WIN32) /*[*/
    unsigned long long ts;
#else /*][*/
    struct timeval tv;
#endif /*]*/
    tofn_t proc;
    bool in_play;
} timeout_t;
static timeout_t *timeouts = NULL;

ioid_t
AddTimeOut(unsigned long interval_ms, tofn_t proc)
{
    timeout_t *t_new;
    timeout_t *t;
    timeout_t *prev = NULL;

    t_new = (timeout_t *)Malloc(sizeof(timeout_t));
    t_new->proc = proc;
    t_new->in_play = false;
#if defined(_WIN32) /*[*/
    ms_ts(&t_new->ts);
    t_new->ts += interval_ms;
#else /*][*/
    gettimeofday(&t_new->tv, NULL);
    t_new->tv.tv_sec += interval_ms / 1000L;
    t_new->tv.tv_usec += (interval_ms % 1000L) * 1000L;
    if (t_new->tv.tv_usec > MILLION) {
	t_new->tv.tv_sec += t_new->tv.tv_usec / MILLION;
	t_new->tv.tv_usec %= MILLION;
    }
#endif /*]*/

    /* Find where to insert this item. */
    for (t = timeouts; t != NULL; t = t->next) {
#if defined(_WIN32) /*[*/
	if (t->ts > t_new->ts)
#else /*][*/
	if (t->tv.tv_sec > t_new->tv.tv_sec ||
	    (t->tv.tv_sec == t_new->tv.tv_sec &&
	     t->tv.tv_usec > t_new->tv.tv_usec))
#endif /*]*/
	{
	    break;
	}
	prev = t;
    }

    /* Insert it. */
    if (prev == NULL) {	/* Front. */
	t_new->next = timeouts;
	timeouts = t_new;
    } else if (t == NULL) {	/* Rear. */
	t_new->next = NULL;
	prev->next = t_new;
    } else {			/* Middle. */
	t_new->next = t;
	prev->next = t_new;
    }

    return (ioid_t)t_new;
}

void
RemoveTimeOut(ioid_t timer)
{
    timeout_t *st = (timeout_t *)timer;
    timeout_t *t;
    timeout_t *prev = NULL;

    if (st->in_play) {
	return;
    }
    for (t = timeouts; t != NULL; t = t->next) {
	if (t == st) {
	    if (prev != NULL) {
		prev->next = t->next;
	    } else {
		timeouts = t->next;
	    }
	    Free(t);
	    return;
	}
	prev = t;
    }
}

/* Input events. */ 
typedef struct input {  
    struct input *next;
    iosrc_t source; 
    int condition;
    iofn_t proc;
} input_t;          
static input_t *inputs = NULL;
static bool inputs_changed = false;

ioid_t
AddInput(iosrc_t source, iofn_t fn)
{
    input_t *ip;

    assert(source != INVALID_IOSRC);

    ip = (input_t *)Malloc(sizeof(input_t));
    ip->source = source;
    ip->condition = InputReadMask;
    ip->proc = fn;
    ip->next = inputs;
    inputs = ip;
    inputs_changed = true;
    return (ioid_t)ip;
}

ioid_t
AddExcept(iosrc_t source, iofn_t fn)
{
#if defined(_WIN32) /*[*/
    return 0;
#else /*][*/
    input_t *ip;

    ip = (input_t *)Malloc(sizeof(input_t));
    ip->source = source;
    ip->condition = InputExceptMask;
    ip->proc = fn;
    ip->next = inputs;
    inputs = ip;
    inputs_changed = true;
    return (ioid_t)ip;
#endif /*]*/
}

#if !defined(_WIN32) /*[*/
ioid_t
AddOutput(iosrc_t source, iofn_t fn)
{
    input_t *ip;

    ip = (input_t *)Malloc(sizeof(input_t));
    ip->source = source;
    ip->condition = InputWriteMask;
    ip->proc = fn;
    ip->next = inputs;
    inputs = ip;
    inputs_changed = true;
    return (ioid_t)ip;
}
#endif /*]*/

void
RemoveInput(ioid_t id)
{
    input_t *ip;
    input_t *prev = NULL;

    for (ip = inputs; ip != NULL; ip = ip->next) {
	if (ip == (input_t *)id) {
	    break;
	}
	prev = ip;
    }
    if (ip == NULL) {
	return;
    }
    if (prev != NULL) {
	prev->next = ip->next;
    } else {
	inputs = ip->next;
    }
    Free(ip);
    inputs_changed = true;
}

#if !defined(_WIN32) /*[*/
/* Child exit events. */ 
typedef struct child_exit {  
    struct child_exit *next;
    pid_t pid;
    childfn_t proc;
} child_exit_t;          
static child_exit_t *child_exits = NULL;

ioid_t
AddChild(pid_t pid, childfn_t fn)
{
    child_exit_t *cx;

    assert(pid != 0 && pid != -1);

    cx = (child_exit_t *)Malloc(sizeof(child_exit_t));
    cx->pid = pid;
    cx->proc = fn;
    cx->next = child_exits;
    child_exits = cx;
    return (ioid_t)cx;
}

/**
 * Poll for an exited child processes.
 *
 * @return true if a waited-for child exited
 */
static bool
poll_children(void)
{
    pid_t pid;
    int status = 0;
    child_exit_t *c;
    child_exit_t *next = NULL;
    child_exit_t *prev = NULL;
    bool any = false;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	for (c = child_exits; c != NULL; c = next) {
	    next = c->next;
	    if (c->pid == pid) {
		(*c->proc)((ioid_t)c, status);
		if (prev) {
		    prev->next = next;
		} else {
		    child_exits = next;
		}
		Free(c);
		any = true;
	    } else {
		prev = c;
	    }
	}
    }
    return any;
}
#endif /*]*/

#if defined(_WIN32) /*[*/
#define MAX_HA	256
#endif /*]*/

/*
 * Inner event dispatcher.
 * Processes one or more pending I/O and timeout events.
 * Waits for the first event if block is true.
 * Returns in *processed_any if any events were processed.
 *
 * Returns true if all pending events have been processed.
 * Returns false if the set of events changed while events were being processed
 *  and new ones may be ready; this function should be called again (with block
 *  set to false) to try to process them.
 */
static bool
process_some_events(bool block, bool *processed_any)
{
#if defined(_WIN32) /*[*/
    HANDLE ha[MAX_HA];
    DWORD nha;
    DWORD tmo;
    DWORD ret;
    unsigned long long now;
    int i;
#else /*][*/
    int ne = 0;
    fd_set rfds, wfds, xfds;
    int ns;
    struct timeval now, twait, *tp;
#endif /*]*/
    input_t *ip, *ip_next;
    struct timeout *t;
    bool any_events_pending;

#   if defined(_WIN32) /*[*/
#    define SOURCE_READY    (ret == WAIT_OBJECT_0 + i)
#    define WAIT_BAD        (ret == WAIT_FAILED)
#    define GET_TS(v)       ms_ts(v)
#    define EXPIRED(t, now) (t->ts <= now)
#   else /*][*/
#    define SOURCE_READY    FD_ISSET(ip->source, &rfds)
#    define WAIT_BAD        (ns < 0)
#    define GET_TS(v)       gettimeofday(v, NULL);
#    define EXPIRED(t, now) (t->tv.tv_sec < now.tv_sec || \
			     (t->tv.tv_sec == now.tv_sec && \
			      t->tv.tv_usec < now.tv_usec))
#   endif /*]*/

    *processed_any = false;

    any_events_pending = false;

#if defined(_WIN32) /*[*/
    nha = 0;
#else /*][*/
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
#endif /*]*/

    for (ip = inputs; ip != NULL; ip = ip->next) {
	/* Set pending input event. */
	if ((unsigned long)ip->condition & InputReadMask) {
#if defined(_WIN32) /*[*/
	    ha[nha++] = ip->source;
#else /*][*/
	    FD_SET(ip->source, &rfds);
	    ne++;
#endif /*]*/
	    any_events_pending = true;
	}

#if !defined(_WIN32) /*[*/
	/* Set pending output event. */
	if ((unsigned long)ip->condition & InputWriteMask) {
	    FD_SET(ip->source, &wfds);
	    ne++;
	    any_events_pending = true;
	}
	/* Set pending exception event. */
	if ((unsigned long)ip->condition & InputExceptMask) {
	    FD_SET(ip->source, &xfds);
	    ne++;
	    any_events_pending = true;
	}
#endif /*]*/
    }

    if (block) {
	if (timeouts != NULL) {
	    /* Compute how long to wait for the first event. */
	    GET_TS(&now);
#if defined(_WIN32) /*[*/
	    if (now > timeouts->ts) {
		tmo = 0;
	    } else {
		tmo = (DWORD)(timeouts->ts - now);
	    }
#else /*][*/
	    twait.tv_sec = timeouts->tv.tv_sec - now.tv_sec;
	    twait.tv_usec = timeouts->tv.tv_usec - now.tv_usec;
	    if (twait.tv_usec < 0L) {
		twait.tv_sec--;
		twait.tv_usec += MILLION;
	    }
	    if (twait.tv_sec < 0L) {
		twait.tv_sec = twait.tv_usec = 0L;
	    }
	    tp = &twait;
#endif /*]*/
	    any_events_pending = true;
	} else {
	    /* Block infinitely. */
#if defined(_WIN32) /*[*/
	    tmo = INFINITE;
#else /*][*/
	    tp = NULL;
#endif /*]*/
	}
    } else {
	/* Don't block. */
#if defined(_WIN32) /*[*/
	tmo = 1;
#else /*][*/
	twait.tv_sec = twait.tv_usec = 0L;
	tp = &twait;
#endif /*]*/
    }

#if !defined(_WIN32) /*[*/
    /* Poll for children. */
    if (poll_children()) {
	return false;
    }
#endif /*]*/

    /* If there's nothing to do now, we're done. */
    if (!any_events_pending) {
	return true;
    }

    /* Wait for events. */
#if defined(_WIN32) /*[*/
    if (tmo == INFINITE) {
	vtrace("Waiting for %d event%s\n",
		(int)nha,
		(nha == 1)? "": "s");
    } else {
	vtrace("Waiting for %d event%s or %d msec\n",
		(int)nha,
		(nha == 1)? "": "s",
		(int)tmo);
    }
    ret = WaitForMultipleObjects(nha, ha, FALSE, tmo);
#else /*][*/
    if (tp == NULL) {
	vtrace("Waiting for %d event%s\n",
		ne,
		(ne == 1)? "": "s");
    } else {
	unsigned msec = (tp->tv_usec + 500) / 1000;
	unsigned sec = tp->tv_sec;

	/* Check for funky round-up. */
	if (msec >= 1000) {
	    sec++;
	    msec -= 1000;
	}
	vtrace("Waiting for %d event%s or %u.%03us\n",
		ne,
		(ne == 1)? "": "s",
		sec, msec);
    }
    ns = select(FD_SETSIZE, &rfds, &wfds, &xfds, tp);
#endif /*[*/

    if (WAIT_BAD) {
#if !defined(_WIN32) /*[*/
	if (errno != EINTR) {
	    xs_warning("process_events: select() failed: %s", strerror(errno));
	}
#else /*][*/
	xs_warning("WaitForMultipleObjects failed: %s",
		win32_strerror(GetLastError()));
#endif /*]*/
	return true;
    }

#if defined(_WIN32) /*[*/
    vtrace("Got event 0x%lx\n", ret);
#else /*][*/
    vtrace("Got %u event%s\n", ns, (ns == 1)? "": "s");
#endif /*]*/

    inputs_changed = false;

    /* Process the event(s) that occurred. */
#if defined(_WIN32) /*[*/
    for (i = 0, ip = inputs; ip != NULL; ip = ip_next, i++)
#else /*][*/
    for (ip = inputs; ip != NULL; ip = ip_next)
#endif /*]*/
    {
	ip_next = ip->next;

	/* Check for input ready. */
	if (((unsigned long)ip->condition & InputReadMask) &&
		SOURCE_READY) {
	    (*ip->proc)(ip->source, (ioid_t)ip);
	    *processed_any = true;
	    if (inputs_changed) {
		/* Other events may no longer be valid. Try again. */
		return false;
	    }
	}

#if !defined(_WIN32) /*[*/
	/* Check for output ready. */
	if (((unsigned long)ip->condition & InputWriteMask) &&
		FD_ISSET(ip->source, &wfds)) {
	    (*ip->proc)(ip->source, (ioid_t)ip);
	    *processed_any = true;
	    if (inputs_changed) {
		/* Other events may no longer be valid. Try again. */
		return false;
	    }
	}

	/* Check for exception ready. */
	if (((unsigned long)ip->condition & InputExceptMask) &&
		FD_ISSET(ip->source, &xfds)) {
	    (*ip->proc)(ip->source, (ioid_t)ip);
	    *processed_any = true;
	    if (inputs_changed) {
		/* Other events may no longer be valid. Try again. */
		return false;
	    }
	}
#endif /*]*/
    }

    /* See what's expired. */
    if (timeouts != NULL) {
	GET_TS(&now);
	while ((t = timeouts) != NULL) {
	    if (EXPIRED(t, now)) {
		timeouts = t->next;
		t->in_play = true;
		(*t->proc)((ioid_t)t);
		*processed_any = true;
		Free(t);
	    } else {
		break;
	    }
	}
    }

    /* If inputs have changed, retry. */
    return !inputs_changed;
}

/*
 * Event dispatcher.
 * Processes all pending I/O and timeout events.
 * Waits for the first event if block is true.
 * Returns true if events were proccessed, false otherwise.
 */
bool
process_events(bool block)
{
    bool processed_any = false;
    bool any_this_time = false;
    bool done = false;

    /* Process events until no more are ready. */
    while (!done) {
	if (run_tasks()) {
	    return true;
	}

	/* Process some events. */
	done = process_some_events(block, &any_this_time);

	/* Free transaction memory. */
	txflush();

	/* Don't block a second time. */
	block = false;

	/* Record what happened this time. */
	processed_any |= any_this_time;
    }

    return processed_any;
}
