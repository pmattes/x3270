/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *      stringscript.c
 *              The String action.
 */

#include "globals.h"

#include "wincmn.h"
#include <errno.h>
#include <fcntl.h>

#include "actions.h"
#include "kybd.h"
#include "names.h"
#include "popups.h"
#include "stringscript.h"
#include "task.h"
#include "trace.h"
#include "unicodec.h"
#include "utils.h"
#include "varbuf.h"

static const char hex_digits[] = "0123456789abcdefABCDEF";

static bool string_run(task_cbh handle, bool *success);
static void string_child_data(task_cbh handle, const char *buf, size_t len,
	bool success);
static bool string_child_done(task_cbh handle, bool success, bool abort);

/* Leaf callback block for String. */
static tcb_t string_cb = {
    "String",
    IA_MACRO,
    CB_NEEDS_RUN,
    string_child_data,
    string_child_done,
    string_run
};

/* State for one instance of String. */
typedef struct {
    char *data;		/* data pointer */
    size_t len;		/* remaining length */
    ucs4_t *pdata;	/* paste data */
    size_t pdata_len;	/* size of paste data */
    ia_t ia;		/* cause */
    bool is_hex;	/* true if data is hexadecimal */
    bool is_paste;	/* true to use paste mode */
    bool force_utf8;	/* true to force UTF-8 conversion */
    char *result;	/* error message from child action */
    bool aborted;	/* action aborted due to child error */
} string_t;

/**
 * Incremental run command for a String.
 *
 * @param[in] handle		Context
 * @param[out] success		Returned success/failure if function returns
 *				true
 *
 * @return true if complete, false if blocked
 */
static bool
string_run(task_cbh handle, bool *success)
{
    string_t *s = (string_t *)handle;
    bool done = false;

    *success = true;

    /* Check for an abort triggered by a child failure. */
    if (s->aborted) {
	if (!IA_IS_KEY(s->ia)) {
	    /* For anything but a keymap, pop up an error message. */
	    if (s->result) {
		popup_an_error(AnString "() failed: %s", s->result);
	    } else {
		popup_an_error(AnString "() terminated due to error");
	    }
	} else {
	    vtrace(AnString "() terminated due to error\n");
	}
	*success = false;
	done = true;
	goto clean_up;
    }

    /* Check for a pre-existing operator error. */
    if (kybdlock & KL_OERR_MASK) {
	popup_an_error("Operator error");
	*success = false;
	done = true;
	goto clean_up;
    }

    /* Check for some waitable keyboard lock. */
    if (task_can_kbwait()) {
	task_kbwait();
	goto clean_up;
    }

    /* Any other keyboard lock is fatal, such as disconnect. */
    if (kybdlock) {
	popup_an_error("Canceled");
	*success = false;
	done = true;
	goto clean_up;
    }

    if (s->is_paste) {
	/* Push in paste data. */
	emulate_uinput(s->pdata, s->pdata_len, true);
	done = true;
    } else if (s->is_hex) {
	/* Run the whole string. */
	hex_input(s->data);
	done = true;
    } else {
	/* Run what we can. */
	size_t len_left = emulate_input(s->data, s->len, false, s->force_utf8);

	s->data += s->len - len_left;
	s->len = len_left;
	if (s->len == 0) {
	    done = true;
	}
    }

    /* Check for induced operator error. */
    if (kybdlock & KL_OERR_MASK) {
	popup_an_error("Operator error");
	*success = false;
	done = true;
    }

    /* Check for some waitable keyboard lock. */
    if (task_can_kbwait()) {
	task_kbwait();
	done = false;
	goto clean_up;
    }
    
clean_up:
    if (done) {
	Replace(s->result, NULL);
	Replace(s->pdata, NULL);
	Free(s);
    }

    return done;
}

/**
 * Callback for data returned to the String action by its children. It is
 * ignored unless a command it executes fails.
 *
 * @param[in] handle	Callback handle
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 */
static void
string_child_data(task_cbh handle, const char *buf, size_t len, bool success)
{
    string_t *s = (string_t *)handle;
    char *b = Malloc(len + 1);

    strncpy(b, buf, len);
    b[len] = '\0';
    Replace(s->result, b);
}

/**
 * Callback for completion of one action executed by the String action.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in] abort		True if aborting
 *
 * @return True if context is complete
 */
static bool
string_child_done(task_cbh handle, bool success, bool abort)
{
    string_t *s = (string_t *)handle;

    if (!success) {
	s->aborted = true;
    }

    if (abort) {
	Replace(s->result, NULL);
	Replace(s->pdata, NULL);
	Free(s);
	return true;
    }

    return false;
}

/**
 * Translate a hex digit to a nybble.
 *
 * @param[in] c		Hex digit
 *
 * @return Nybble (value from 0 to 16).
 */
static unsigned char
hex_to_nybble(char c)
{
    size_t ix = (unsigned char)(strchr(hex_digits, c) - hex_digits);

    if (ix > 15) {
	ix -= 6;
    }
    return (unsigned char)ix;
}

/**
 * Transform a hex string from multibyte to Unicode characters.
 *
 * @param[in] s		String
 * @param[out] lenp	Returned length (number of ucs4 elements)
 * @param[in] force_utf8 True to force UTF-8 conversion
 *
 * @return Translated string, or NULL for an error.
 */
static ucs4_t *
hex_to_unicode(const char *s, size_t *lenp, bool force_utf8)
{
    size_t text_len;
    size_t utf8_len;
    char *utf8_buf;
    char *u8p;
    size_t i;
    ucs4_t *ucs4_buf;
    int len;

    /* Do gross syntax checking first to make the code simpler below. */
    if (strspn(s, hex_digits) != strlen(s)) {
	return NULL;
    }
    text_len = strlen(s);
    if (!text_len || (text_len % 2) != 0) {
	return NULL;
    }

    /* Translate text to binary. */
    utf8_len = text_len / 2;
    u8p = utf8_buf = Malloc(utf8_len);
    for (i = 0; i < text_len; i += 2) {
	*u8p++ = (hex_to_nybble(s[i]) << 4) | hex_to_nybble(s[i + 1]);
    }

    /* Translate to Unicode. */
    ucs4_buf = (ucs4_t *)Malloc(utf8_len * sizeof(ucs4_t));
    len = multibyte_to_unicode_string(utf8_buf, utf8_len, ucs4_buf, utf8_len,
	    force_utf8);
    if (len < 0) {
	Free(utf8_buf);
	Free(ucs4_buf);
	return NULL;
    }

    Free(utf8_buf);
    *lenp = len;
    return ucs4_buf;
}

/**
 * Back end of the String() action.
 *
 * @param[in] st	String to execute
 * @param[in] is_hex	True if string is in hex
 * @param[in] is_paste	True if paste mode
 * @param[in] force_utf8 True to force UTF-8 conversion
 */
void
push_string(char *st, bool is_hex, bool is_paste, bool force_utf8)
{
    string_t *s;
    ucs4_t *pdata = NULL;
    size_t pdata_len = 0;

    if (is_paste) {
	if ((pdata = hex_to_unicode(st, &pdata_len, force_utf8)) == NULL) {
	    popup_an_error("Invalid hexadecimal paste data");
	    return;
	}
    }

    /* Construct the context. */
    s = (string_t *)Calloc(sizeof(string_t) + strlen(st) + 1, 1);
    s->data = (char *)(s + 1);
    strcpy(s->data, st);
    s->len = strlen(st);
    s->pdata = pdata;
    s->pdata_len = pdata_len;
    s->ia = IA_MACRO;
    s->is_hex = is_hex;
    s->is_paste = is_paste;
    s->force_utf8 = force_utf8;
    s->result = NULL;
    s->aborted = false;

    /* Push a leaf callback. */
    push_cb(NULL, 0, &string_cb, (task_cbh)s);
}
