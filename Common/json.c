/*
 * Copyright (c) 2021-2024 Paul Mattes.
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
 *      json.c
 *              JSON parser and formatter, per RFC 8259.
 */

#include "globals.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>

#include "utf8.h"
#include "utils.h"
#include "varbuf.h"

#include "json.h"
#include "json_private.h"

#define HS_START	0xd800	/* Start of high surrogates */
#define LS_START	0xdc00	/* Start of low surrogates */
#define HS_END		LS_START /* End of high surrogates */
#define LS_END		0xe000	/* End of low surrogates */
#define SURR_BASE	0x10000	/* Base of code points represented by
				   surrogates */
#define SHIFT_BITS	10	/* Bits to shift for mapping */

#define HIGH_SURROGATE(u)	((u) >= HS_START && (u) < HS_END)
#define LOW_SURROGATE(u)	((u) >= LS_START && (u) < LS_END)
#define LEAD_OFFSET		(HS_START - (SURR_BASE >> SHIFT_BITS))
#define SURROGATE_OFFSET	(SURR_BASE - (HS_START << SHIFT_BITS) - \
					LS_START)

/* Sub-states for parsing tokens. */
typedef enum {
    JK_BASE,		/* ground state */
    JK_BAREWORD,	/* bare word */
    JK_NUMBER,		/* number */
    JK_STRING,		/* string */
    JK_STRING_BS,	/* backslash inside string */
    JK_TERMINAL		/* parsing complete */
} json_token_state_t;

/* Number parsing return values. */
typedef enum {
    NP_SUCCESS,		/* successful parsing */
    NP_FAILURE,		/* unsuccessful parsing */
    NP_OVERFLOW		/* overflow */
} np_ret_t;

/* String parsing return values. */
typedef enum {
    SP_SUCCESS,		/* successful parsing */
    SP_FAILURE		/* unsuccessful parsing */
} sp_ret_t;

/* Barewords. */
static ucs4_t u_null[] = { 'n', 'u', 'l', 'l', 0 };
static ucs4_t u_true[] = { 't', 'r', 'u', 'e', 0 };
static ucs4_t u_false[] = { 'f', 'a', 'l', 's', 'e', 0 };

/**
 * Check is a character is a JSON whitespace character.
 * @param[in] ucs4	Character to inspect
 * @returns true if whitespace.
 */
static bool
is_json_space(ucs4_t ucs4)
{
    switch (ucs4) {
	case ' ':
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	    return true;
	default:
	    return false;
    }
}

/**
 * Validate and parse a UCS-4 string as an integer.
 * @param[in] s		String to parse
 * @param[in] len	Length of string
 * @param[out] ret	Returned integer
 * @returns np_ret_t
 */
static np_ret_t
valid_integer(ucs4_t *s, size_t len, int64_t *ret)
{
    char *str;
    size_t i;
    long long l;
    char *end;

    if (!*s) {
	return NP_FAILURE;
    }
    str = Malloc(len + 1);
    for (i = 0; i < len; i++) {
	str[i] = (char)s[i];
    }
    str[len] = '\0';
    l = strtoll(str, &end, 10);
    Free(str);
    if (end != &str[len]) {
	return NP_FAILURE;
    }
    if ((l == LLONG_MIN || l == LLONG_MAX) && errno == ERANGE) {
	return NP_OVERFLOW;
    }

    /*
     * Here is an ugly edge between specific-width integer types and abstract
     * C types. We want to use a specific-width integer type for our integers
     * (mostly so our users can use the same), but the library does not have
     * functions to convert strings to fixed-width types -- just long and long
     * long. So we need to do an extra overflow test here, in case (long long)
     * is wider than 64 bits.
     */
#if LONG_WIDTH == 64 && LLONG_WIDTH > LLONG_WIDTH /*[*/
    if (l < LONG_MIN || l > LONG_MAX) {
	return NP_OVERFLOW;
    }
#endif /*]*/

    *ret = (int64_t)l;
    return NP_SUCCESS;
}

/**
 * Validate and parse a UCS-4 string as a double.
 * @param[in] s		String to parse
 * @param[in] len	Length of string
 * @param[out] ret	Returned double
 * @returns np_ret_t
 */
static np_ret_t
valid_double(ucs4_t *s, size_t len, double *ret)
{
    char *str;
    size_t i;
    char *end;

    str = Malloc(len + 1);
    for (i = 0; i < len; i++) {
	str[i] = (char)s[i];
    }
    str[len] = '\0';
    *ret = strtod(str, &end);
    Free(str);
    if (end != &str[len])
    {
	return NP_FAILURE;
    }
    if (*ret == HUGE_VALF || *ret == HUGE_VALL) {
	return NP_OVERFLOW;
    }
    return NP_SUCCESS;
}

/**
 * Validate and parse a UCS-4 string as a string.
 * @param[in] s		String to parse
 * @param[in] len	Length of string
 * @param[out] s_ret	Returned string
 * @param[out] len_ret	Returned string length
 * @returns sp_ret_t
 */
static sp_ret_t
valid_string(ucs4_t *s, size_t len, char **s_ret, size_t *len_ret)
{
    char *ret = NULL;
    size_t rlen = 0;
    ucs4_t c;
    bool backslash = false;
    size_t i;
    char xbuf[5];
    ucs4_t u;
    char ubuf[6];
    int j;
    int nr;
    ucs4_t surrogate_lead = 0;
#   define STASH(c) do { \
    ret = (char *)Realloc(ret, rlen + 1); \
    ret[rlen++] = c; \
} while (false)
#   define DUMP_LEAD do { \
    nr = unicode_to_utf8(surrogate_lead, ubuf); \
    for (j = 0; j < nr; j++) { \
	STASH(ubuf[j]); \
    } \
    surrogate_lead = 0; \
} while (false)

    for (i = 0; i < len; i++) {
	c = s[i];
	if (backslash) {
	    if ((surrogate_lead != 0) && c != 'u') {
		DUMP_LEAD;
	    }
	    switch (c) {
	    case '\\':
		STASH('\\');
		break;
	    case '/':
		STASH('/');
		break;
	    case 'r':
		STASH('\r');
		break;
	    case 'n':
		STASH('\n');
		break;
	    case 't':
		STASH('\t');
		break;
	    case 'f':
		STASH('\f');
		break;
	    case 'u':
		/* We need 4 hex digits. */
		for (j = 0; j < 4; j++) {
		    if (++i >= len || !isxdigit((int)s[i])) {
			Free(ret);
			return SP_FAILURE;
		    }
		    xbuf[j] = (char)s[i];
		}
		xbuf[j] = '\0';
		u = (ucs4_t)strtoul(xbuf, NULL, 16);
		if ((surrogate_lead != 0) &&
			!LOW_SURROGATE(u) &&
			!HIGH_SURROGATE(u)) {
		    DUMP_LEAD;
		}
		if (HIGH_SURROGATE(u)) {
		    if (surrogate_lead != 0) {
			DUMP_LEAD;
		    }
		    surrogate_lead = u;
		    break;
		}
		if (LOW_SURROGATE(u)) {
		    if (surrogate_lead != 0) {
			/* Encode the surrogate pair as a single codepoint. */
			u += SURROGATE_OFFSET + (surrogate_lead << SHIFT_BITS);
			surrogate_lead = 0;
		    }
		}
		nr = unicode_to_utf8(u, ubuf);
		if (nr < 0) {
		    Free(ret);
		    return SP_FAILURE;
		}
		for (j = 0; j < nr; j++) {
		    STASH(ubuf[j]);
		}
		break;
	    default:
		Free(ret);
		return SP_FAILURE;
	    }
	    backslash = false;
	} else {
	    if (c == '\\') {
		backslash = true;
	    } else {
		if (surrogate_lead != 0) {
		    DUMP_LEAD;
		}
		nr = unicode_to_utf8(c, ubuf);
		if (nr < 0) {
		    Free(ret);
		    return SP_FAILURE;
		}
		for (j = 0; j < nr; j++) {
		    STASH(ubuf[j]);
		}
	    }
	}
    }

    if (surrogate_lead != 0) {
	DUMP_LEAD;
    }

    STASH('\0');
    *s_ret = ret;
    *len_ret = rlen - 1;
    return SP_SUCCESS;
#   undef STASH
#   undef DUMP_LEAD
}

/**
 * Compare two null-terminated UCS-4 strings.
 * @param[in] a		First string
 * @param[in] b		Second string
 * @returns true if equal
 */
static bool
ucs4streq(ucs4_t *a, ucs4_t *b)
{
    ucs4_t ac, bc;

    while (true) {
	ac = *a++;
	bc = *b++;
	if (ac == 0 && bc == 0) {
	    return true;
	}
	if (ac != bc) {
	    return false;
	}
    }
}

/**
 * Format an error message that ends with a Unicode character.
 * @param[in] text	Body of message
 * @param[in] u		Unicode character
 *
 * @returns Formatted message
 */
static char *
format_uerror(const char *text, ucs4_t u)
{
    return (u < 0xff && isprint((int)u))?
	Asprintf("%s '%c'", text, u):
	Asprintf("%s U+%04x", text, u);
}

/**
 * Parse text into JSON.
 * @param[in,out] line	Line number
 * @param[in,out] column Column number
 * @param[in] text	Text to parse
 * @param[in,out] offset Offset to start of text
 * @param[in] len	Length of text in bytes
 * @param[out] result	Result if successful
 * @param[out] error	Error if not successful
 * @param[out] stop_token Character after this element
 * @param[out] any	True if something parsed
 * @returns error code
 */
static json_errcode_t
json_parse_internal(int *line, int *column, const char *text, size_t *offset,
	size_t len, json_t **result, json_parse_error_t **error,
	ucs4_t *stop_token, bool *any)
{
    json_token_state_t token_state = JK_BASE;
    ucs4_t *token_buf = NULL;
    size_t token_buf_len = 0;
    json_errcode_t e;
    unsigned length;
    ucs4_t internal_stop;

    *result = NULL;
    *error = NULL;
    *stop_token = 0;
    *any = false;

#   define FAIL(e, m) do { \
    Replace(token_buf, NULL); \
    *error = (json_parse_error_t *)Malloc(sizeof(json_parse_error_t)); \
    (*error)->errcode = e; \
    (*error)->line = *line; \
    (*error)->column = (*column)? *column: 1; \
    (*error)->errmsg = m; \
    (*error)->offset = *offset; \
    json_free(*result); \
    return e; \
} while (false)

#   define ADD_TOKEN(u) do { \
    token_buf = (ucs4_t *)Realloc(token_buf, \
	    (token_buf_len + 1) * sizeof(ucs4_t)); \
    token_buf[token_buf_len++] = u; \
} while (false)

#   define BAREWORD_DONE do { \
    ADD_TOKEN(0); \
    if (ucs4streq(token_buf, u_null)) { \
	*any = true; \
	*result = NULL; \
    } else if (ucs4streq(token_buf, u_true)) { \
	*any = true; \
	*result = (json_t *)Calloc(1, sizeof(json_t)); \
	(*result)->type = JT_BOOLEAN; \
	(*result)->value.v_boolean = true; \
    } else if (ucs4streq(token_buf, u_false)) { \
	*any = true; \
	*result = (json_t *)Calloc(1, sizeof(json_t)); \
	(*result)->type = JT_BOOLEAN; \
	(*result)->value.v_boolean = false; \
    } else { \
	FAIL(JE_SYNTAX, NewString("Invalid bareword")); \
    } \
    Replace(token_buf, NULL); \
} while (false)

#   define NUMBER_DONE do { \
    int64_t i_ret; \
    double d_ret; \
    np_ret_t np; \
    np = valid_integer(token_buf, token_buf_len, &i_ret); \
    if (np == NP_OVERFLOW) { \
	FAIL(JE_OVERFLOW, NewString("Integer overflow")); \
    } else if (np == NP_SUCCESS) { \
	Replace(token_buf, NULL); \
	*any = true; \
	*result = (json_t *)Calloc(1, sizeof(json_t)); \
	(*result)->type = JT_INTEGER; \
	(*result)->value.v_integer = i_ret; \
    } else { \
	np = valid_double(token_buf, token_buf_len, &d_ret); \
	if (np == NP_OVERFLOW) { \
	    FAIL(JE_OVERFLOW, NewString("Floating-point overflow")); \
	} else if (np == NP_SUCCESS) { \
	    Replace(token_buf, NULL); \
	    *any = true; \
	    *result = (json_t *)Calloc(1, sizeof(json_t)); \
	    (*result)->type = JT_DOUBLE; \
	    (*result)->value.v_double = d_ret; \
	} else { \
	    FAIL(JE_SYNTAX, NewString("Invalid number")); \
	} \
    } \
    Replace(token_buf, NULL); \
} while (false)

    /* Start parsing. */
    while (*offset < len) {
	int nr;
	ucs4_t ucs4;

	/* Decode the next UTF-8 character. */
	nr = utf8_to_unicode(text + *offset, len - *offset, &ucs4);
	if (nr <= 0) {
	    FAIL(JE_UTF8, NewString("UTF-8 decoding error"));
	}

	/* Account for it. */
	*offset += nr;
	if (ucs4 == '\n') {
	    (*line)++;
	    *column = 0;
	} else {
	    (*column)++;
	}

	switch (token_state) {
	    case JK_TERMINAL:
		/* Skip white space until we get something useful. */
		if (is_json_space(ucs4)) {
		    continue;
		}

		/* Return whatever follows an element. */
		*stop_token = ucs4;
		return JE_OK;
	    case JK_BASE:
		/* Ground state. */
		if (is_json_space(ucs4)) {
		    continue;
		}
		switch (ucs4) {
		    case '{':
			/* A struct. */
			*any = true;
			*result = (json_t *)Calloc(1, sizeof(json_t));
			(*result)->type = JT_OBJECT;
			do {
			    json_t *element = NULL;
			    bool r_any = false;
			    char *key;
			    key_value_t kv;
			    size_t key_length;

			    /* Parse what should be a string followed by ':'. */
			    e = json_parse_internal(line, column, text, offset,
				    len, &element, error, &internal_stop,
				    &r_any);
			    if (e != JE_OK) {
				json_free(*result);
				return e;
			    }
			    if (!r_any && internal_stop == '}') {
				break;
			    }
			    if (internal_stop != ':') {
				json_free(element);
				if (internal_stop == 0) {
				    FAIL(JE_INCOMPLETE,
					    NewString("Incomplete struct"));
				} else {
				    FAIL(JE_SYNTAX,
					    format_uerror("Expected ':', got",
						internal_stop));
				}
			    }
			    if (element == NULL) {
				FAIL(JE_SYNTAX,
					NewString("Expected string, got ':'"));
			    }
			    if (element->type != JT_STRING) {
				json_free(element);
				FAIL(JE_SYNTAX, NewString("Expected string"));
			    }

			    /* Save the key. */
			    key_length = element->value.v_string.length;
			    key = Malloc(key_length + 1);
			    memcpy(key, element->value.v_string.text,
				    key_length);
			    key[key_length] = '\0';
			    json_free(element);

			    /* Parse the value, followed by ',' or '}'. */
			    e = json_parse_internal(line, column, text, offset,
				    len, &element, error, &internal_stop,
				    &r_any);
			    if (e != JE_OK) {
				Replace(key, NULL);
				json_free(*result);
				return e;
			    }
			    if (internal_stop != ',' && internal_stop != '}') {
				Replace(key, NULL);
				json_free(element);
				if (internal_stop == 0) {
				    FAIL(JE_INCOMPLETE,
					    NewString("Incomplete struct"));
				} else {
				    FAIL(JE_SYNTAX,
					    format_uerror("Expected ',' or '}'"
						", got", internal_stop));
				}
			    }
			    if (!r_any) {
				assert(element == NULL);
				Replace(key, NULL);
				FAIL(JE_SYNTAX,
					NewString("Missing element value"));
			    }

			    /* Save the key-value pair. */
			    kv.key_length = key_length;
			    kv.key = key;
			    kv.value = element;
			    length = ++((*result)->value.v_object.length);
			    (*result)->value.v_object.key_values =
				Realloc((key_value_t *)(*result)->
					    value.v_object.key_values,
					length * sizeof(key_value_t));
			    (*result)->value.v_object.key_values[length - 1] =
				kv; /* struct copy */
			} while (internal_stop == ',');
			token_state = JK_TERMINAL;
			break;
		    case '[':
			/* An array. */
			*any = true;
			*result = (json_t *)Calloc(1, sizeof(json_t));
			(*result)->type = JT_ARRAY;
			do {
			    json_t *element = NULL;
			    bool r_any = false;

			    e = json_parse_internal(line, column, text, offset,
				    len, &element, error, &internal_stop,
				    &r_any);
			    if (e != JE_OK) {
				json_free(*result);
				return e;
			    }
			    if (r_any) {
				length = ++((*result)->value.v_array.length);
				(*result)->value.v_array.array =
				    Realloc((*result)->value.v_array.array,
					    length * sizeof(json_t *));
				(*result)->value.v_array.array[length - 1] =
				    element;
			    }
			} while (internal_stop == ',');
			if (internal_stop == 0) {
			    FAIL(JE_INCOMPLETE, NewString("Incomplete array"));
			} else if (internal_stop != ']') {
			    FAIL(JE_SYNTAX,
				    format_uerror("Improperly terminated "
					"array at", ucs4));
			}
			token_state = JK_TERMINAL;
			break;
		    case '"':
			/* The start of a string. */
			token_state = JK_STRING;
			break;
		    default:
			if (ucs4 == '-' || isdigit((int)ucs4)) {
			    /* The start of a number. */
			    ADD_TOKEN(ucs4);
			    token_state = JK_NUMBER;
			} else if (isalpha((int)ucs4)) {
			    /* The start of a bareword. */
			    ADD_TOKEN(ucs4);
			    token_state = JK_BAREWORD;
			} else {
			    *stop_token = ucs4;
			    return JE_OK;
			}
		}
		break;
	    case JK_BAREWORD:
		/* Have seen at least one bareword character. */
		if (isalpha((int)ucs4)) {
		    ADD_TOKEN(ucs4);
		} else {
		    BAREWORD_DONE;
		    if (is_json_space(ucs4)) {
			token_state = JK_TERMINAL;
		    } else {
			*stop_token = ucs4;
			return JE_OK;
		    }
		}
		break;
	    case JK_NUMBER:
		/* Have seen at least one part of a number. */
		if (isdigit((int)ucs4) ||
			ucs4 == '.' ||
			ucs4 == 'e' ||
			ucs4 == '-' ||
			ucs4 == '+') {
		    ADD_TOKEN(ucs4);
		} else {
		    NUMBER_DONE;
		    if (is_json_space(ucs4)) {
			token_state = JK_TERMINAL;
		    } else {
			*stop_token = ucs4;
			return JE_OK;
		    }
		}
		break;
	    case JK_STRING:
		/* Have seen an opening double quote. */
		if (ucs4 == '\\') {
		    token_state = JK_STRING_BS;
		} else if (ucs4 == '"') {
		    sp_ret_t sp;
		    char *s_ret;
		    size_t len_ret;

		    sp = valid_string(token_buf, token_buf_len, &s_ret,
			    &len_ret);
		    if (sp == SP_FAILURE) {
			FAIL(JE_SYNTAX, NewString("Invalid string"));
		    }
		    Replace(token_buf, NULL);
		    *any = true;
		    *result = (json_t *)Calloc(1, sizeof(json_t));
		    (*result)->type = JT_STRING;
		    (*result)->value.v_string.length = len_ret;
		    (*result)->value.v_string.text = s_ret;
		    token_state = JK_TERMINAL;
		} else {
		    ADD_TOKEN(ucs4);
		}
		break;
	    case JK_STRING_BS:
		/* Have seen a backslash within a string. */
		if (ucs4 == '"') {
		    ADD_TOKEN(ucs4);
		} else {
		    ADD_TOKEN('\\');
		    ADD_TOKEN(ucs4);
		}
		token_state = JK_STRING;
		break;
	}
    }

    switch (token_state) {
    case JK_BASE:
	FAIL(JE_INCOMPLETE, NewString("Empty input or incomplete object"));
	break;
    case JK_BAREWORD:
	BAREWORD_DONE;
	return JE_OK;
    case JK_NUMBER:
	NUMBER_DONE;
	return JE_OK;
    case JK_STRING:
    case JK_STRING_BS:
	FAIL(JE_INCOMPLETE, NewString("Unterminated string"));
	break;
    default:
    case JK_TERMINAL:
	/* Success. */
	return JE_OK;
    }

#   undef ADD_TOKEN
#   undef FAIL
#   undef BAREWORD_DONE
#   undef NUMBER_DONE
}

/**
 * Parse text into JSON.
 * @param[in] text	Text to parse
 * @param[in] len	Length of text in bytes
 * @param[out] result	Result if successful
 * @param[out] error	Error if not successful
 * @returns error code
 */
json_errcode_t
json_parse(const char *text, ssize_t len, json_t **result,
	json_parse_error_t **error)
{
    int line = 1;
    int column = 0;
    size_t offset = 0;
    json_errcode_t e;
    ucs4_t stop_token;
    bool r_any;

    if (len < 0) {
	len = strlen(text);
    }

    e = json_parse_internal(&line, &column, text, &offset, len, result, error,
	    &stop_token, &r_any);
    if (e == JE_OK && stop_token != 0) {
	const char *adj = r_any? "Extra text": "Unexpected text";

	e = r_any? JE_EXTRA: JE_SYNTAX;
	*error = (json_parse_error_t *)Malloc(sizeof(json_parse_error_t));
	(*error)->errcode = e;
	(*error)->line = line;
	(*error)->column = column;
	(*error)->errmsg = format_uerror(adj, stop_token);
	(*error)->offset = offset - 1;
    }
    return e;
}

/**
 * Free a JSON node, recursively.
 * @param[in,out] json	JSON node to free, or NULL.
 * @returns NULL
 */
json_t *
_json_free(json_t *json)
{
    if (json != NULL) {
	unsigned i;

	switch (json->type) {
	    case JT_STRING:
		Free((char *)json->value.v_string.text);
		json->value.v_string.length = 0;
		json->value.v_string.text = NULL;
		break;
	    case JT_ARRAY:
		for (i = 0; i < json->value.v_array.length; i++) {
		    json_free(json->value.v_array.array[i]);
		}
		Replace(json->value.v_array.array, NULL);
		break;
	    case JT_OBJECT:
		for (i = 0; i < json->value.v_object.length; i++) {
		    json->value.v_object.key_values[i].key_length = 0;
		    Free((char *)json->value.v_object.key_values[i].key);
		    json->value.v_object.key_values[i].key = NULL;
		    json_free(json->value.v_object.key_values[i].value);
		    json->value.v_object.key_values[i].value = NULL;
		}
		Replace(json->value.v_object.key_values, NULL);
		break;
	    default:
		break;
	}
	Free(json);
    }

    return NULL;
}

/**
 * Free a JSON parse_error.
 * @param[in,out] error	Error structure to free, or NULL.
 * @returns NULL
 */
json_parse_error_t *
_json_free_error(json_parse_error_t *error)
{
    if (error != NULL) {
	Free((char *)error->errmsg);
	error->errmsg = NULL;
	Free(error);
    }

    return NULL;
}

/**
 * Expand a JSON string into something safe to display.
 * @param[in] s		String to expand
 * @param[in] len	String length
 * @param[in] options	Option flags
 */
static char *
json_expand_string(const char *s, size_t len, unsigned options)
{
    varbuf_t r;

    vb_init(&r);
    while (len > 0) {
	int nr;
	ucs4_t ucs4;

	/* Decode the next UTF-8 character. */
	nr = utf8_to_unicode(s, len, &ucs4);
	if (nr <= 0) {
	    vb_append(&r, s, 1);
	    s++;
	    len--;
	    continue;
	}
	switch (ucs4) {
	case '\r':
	    vb_appends(&r, "\\r");
	    break;
	case '\n':
	    vb_appends(&r, "\\n");
	    break;
	case '\t':
	    vb_appends(&r, "\\t");
	    break;
	case '\f':
	    vb_appends(&r, "\\f");
	    break;
	case '\\':
	    vb_appends(&r, "\\\\");
	    break;
	case '"':
	    vb_appends(&r, "\\\"");
	    break;
	default:
	    if (ucs4 < ' ') {
		vb_appendf(&r, "\\u%04x", ucs4);
	    } else if ((options & JW_EXPAND_SURROGATES) && ucs4 >= 0x10000) {
		/* Not strictly necessary, but helpful. */
		vb_appendf(&r, "\\u%04x\\u%04x",
			LEAD_OFFSET + (ucs4 >> 10),
			0xdc00 + (ucs4 & 0x3ff));
	    } else {
		vb_append(&r, s, nr);
	    }
	    break;
	}

	s += nr;
	len -= nr;
    }
    return vb_consume(&r);
}

/* Write out a JSON object. */
static char *
json_write_indent(const json_t *json, unsigned options, int indent)
{
    varbuf_t r;
    unsigned i;
    size_t len;
    char *s;
    char *t;
    const char *v;
    int indent1 = (options & JW_ONE_LINE)? 0: indent + 1;

    if (options & JW_ONE_LINE) {
	indent = 0;
    }

    switch (json_type(json)) {
    case JT_NULL:
    default:
	return NewString("null");
    case JT_BOOLEAN:
	return NewString(json_boolean_value(json)? "true": "false");
    case JT_INTEGER:
	return Asprintf("%"JSON_INT_PRINT, json_integer_value(json));
    case JT_DOUBLE:
	return Asprintf("%g", json_double_value(json));
    case JT_STRING:
	v = json_string_value(json, &len);
	s = json_expand_string(v, len, options);
	t = Asprintf("\"%s\"", s);
	Free(s);
	return t;
    case JT_OBJECT:
	vb_init(&r);
	vb_appendf(&r, "{%s", (options & JW_ONE_LINE)? "": "\n");
	for (i = 0; i < json->value.v_object.length; i++) {
	    key_value_t *kv = &json->value.v_object.key_values[i];
	    char *s;
	    bool last = i >= json->value.v_object.length - 1;

	    s = json_expand_string(kv->key, kv->key_length, options);
	    vb_appendf(&r, "%*s\"%s\":%s", indent1 * 2, "", s,
		    (options & JW_ONE_LINE)? "": " ");
	    Free(s);
	    s = json_write_indent(kv->value, options, indent1);
	    vb_appendf(&r, "%s%s%s", s, last? "": ",",
		    (options & JW_ONE_LINE)? "": "\n");
	    Free(s);
	}
	vb_appendf(&r, "%*s}", indent * 2, "");
	return vb_consume(&r);
    case JT_ARRAY:
	vb_init(&r);
	vb_appendf(&r, "[%s", (options & JW_ONE_LINE)? "": "\n");
	for (i = 0; i < json->value.v_array.length; i++) {
	    char *s = json_write_indent(json->value.v_array.array[i],
		    options, indent1);
	    bool last = i >= json->value.v_array.length - 1;

	    vb_appendf(&r, "%*s%s%s%s",
		    indent1 * 2, "",
		    s,
		    last? "": ",",
		    (options & JW_ONE_LINE)? "": "\n");
	    Free(s);
	}
	vb_appendf(&r, "%*s]", indent * 2, "");
	return vb_consume(&r);
    }
}

/* Write out a JSON object. */
char *
json_write_o(const json_t *json, unsigned options)
{
    return json_write_indent(json, options, 0);
}

/**
 * Returns the type of a JSON node.
 * @param[in] json	JSON node.
 * @returns node type
 */
json_type_t
json_type(const json_t *json)
{
    return (json == NULL)? JT_NULL: json->type;
}

/**
 * Returns the length of an object.
 * @param[in] json	Node to evaluate.
 * @returns length
 */
unsigned
json_object_length(const json_t *json)
{
    assert(json != NULL);
    assert(json->type == JT_OBJECT);
    return json->value.v_object.length;
}

/**
 * Returns the length of an array.
 * @param[in] json	Node to evaluate.
 * @returns length
 */
unsigned
json_array_length(const json_t *json)
{
    assert(json != NULL);
    assert(json->type == JT_ARRAY);
    return json->value.v_array.length;
}

/**
 * Returns the array element at the given index.
 * @param[in] json	Node to search
 * @param[in] index	Index
 * @returns array element
 */
json_t *
json_array_element(const json_t *json, unsigned index)
{
    assert(json != NULL);
    assert(json->type == JT_ARRAY);
    assert(index < json->value.v_array.length);
    return json->value.v_array.array[index];
}

/**
 * Return the object member with the given key.
 * @param[in] json	Node to search
 * @param[in] key	Key
 * @param[in] key_length Key length
 * @param[out] ret	Returned member
 * 			not found)
 * @returns true if member found
 */
bool
json_object_member(const json_t *json, const char *key, ssize_t key_length,
	json_t **ret)
{
    unsigned i;

    assert(json != NULL);
    assert(json->type == JT_OBJECT);
    if (key_length < 0)  {
	key_length = strlen(key);
    }
    for (i = 0; i < json->value.v_object.length; i++) {
	if (json->value.v_object.key_values[i].key_length ==
		(size_t)key_length &&
		!memcmp(key, json->value.v_object.key_values[i].key,
		    key_length)) {
	    *ret = json->value.v_object.key_values[i].value;
	    return true;
	}
    }
    *ret = NULL;
    return false;
}

/**
 * Iterator for objects.
 * @param[in,out] context	Opaque context.
 * @param[in] json		Object to walk.
 * @param[out] key		Returned member key.
 * @param[out] key_length	Returned member key length.
 * @param[out] value		Returned member value.
 * @returns true if member returned
 */
bool
_json_object_member_next(void **context, const json_t *json, const char **key,
	size_t *key_length, const json_t **value)
{
    key_value_t *kv;

    assert(json != NULL);
    assert(json->type == JT_OBJECT);

    *key = NULL;
    *key_length = 0;
    *value = NULL;

    if (json->value.v_object.length == 0) {
	return false;
    }

    if (*context == NULL) {
	*context = json->value.v_object.key_values - 1;
    }

    kv = (key_value_t *)*context;
    kv = kv + 1;

    if (kv >= json->value.v_object.key_values + json->value.v_object.length) {
	return false;
    }

    *key = kv->key;
    *key_length = kv->key_length;
    *value = kv->value;
    *context = kv;
    return true;
}

/**
 * Return a pointer to the integer value.
 * @param[in] json	Node to examine.
 * @returns Integer
 */
int64_t
json_integer_value(const json_t *json)
{
    assert(json != NULL);
    assert(json->type == JT_INTEGER);
    return json->value.v_integer;
}

/**
 * Returns the double value.
 * @param[in] json	Node to examine.
 * @returns Double value
 */
double
json_double_value(const json_t *json)
{
    assert(json != NULL);
    assert(json->type == JT_DOUBLE);
    return json->value.v_double;
}

/**
 * Return a pointer to the string value.
 * @param[in] json	Node to examine.
 * @param[out] length	Returned length (can be NULL).
 * @returns String value
 */
const char *
json_string_value(const json_t *json, size_t *length)
{
    assert(json != NULL);
    assert(json->type == JT_STRING);
    if (length != NULL) {
	*length = json->value.v_string.length;
    }
    return json->value.v_string.text;
}

/**
 * Returns the boolean value.
 * @param[in] json	Node to examine.
 * @returns Boolean pointer or null.
 */
bool
json_boolean_value(const json_t *json)
{
    assert(json != NULL);
    assert(json->type == JT_BOOLEAN);
    return json->value.v_boolean;
}

/* Constructors. */

/**
 * Allocates a Boolean.
 * @param[in] value	Value
 * @returns Boolean
 */
json_t *
json_boolean(bool value)
{
    json_t *j = Calloc(1, sizeof(json_t));

    j->type = JT_BOOLEAN;
    j->value.v_boolean = value;
    return j;
}

/**
 * Allocates an integer.
 * @param[in] value	Value
 * @returns integer
 */
json_t *
json_integer(int64_t value)
{
    json_t *j = Calloc(1, sizeof(json_t));

    j->type = JT_INTEGER;
    j->value.v_integer = value;
    return j;
}

/**
 * Allocates a double.
 * @param[in] value	Value
 * @returns double
 */
json_t *
json_double(double value)
{
    json_t *j = Calloc(1, sizeof(json_t));

    j->type = JT_DOUBLE;
    j->value.v_double = value;
    return j;
}

/**
 * Allocates a string.
 * @param[in] text	String
 * @param[in] length	String length, or -1
 * @returns string
 */
json_t *
json_string(const char *text, ssize_t length)
{
    json_t *j = Calloc(1, sizeof(json_t));
    char *s;

    if (length < 0) {
	length = strlen(text);
    }
    j->type = JT_STRING;
    s = Malloc(length + 1);
    memcpy(s, text, length);
    s[length] = '\0';
    j->value.v_string.text = s;
    j->value.v_string.length = length;
    return j;
}

/**
 * Allocates an empty object.
 * @returns object
 */
json_t *
json_object(void)
{
    json_t *j = Calloc(1, sizeof(json_t));

    j->type = JT_OBJECT;
    return j;
}

/**
 * Allocates an empty array.
 * @returns array
 */
json_t *
json_array(void)
{
    json_t *j = Calloc(1, sizeof(json_t));

    j->type = JT_ARRAY;
    return j;
}

/**
 * Sets an object member.
 * The value is copied by reference, not cloned.
 * @param[in,out] json	Object to modify
 * @param[in] key	Field key
 * @param[in] key_length Key length
 * @param[in] value	Value to set
 * @returns error code
 */
void
json_object_set(json_t *json, const char *key, ssize_t key_length,
        json_t *value)
{
    unsigned i;
    key_value_t *kv;
    char *s;

    assert(json != NULL);
    assert(json->type == JT_OBJECT);
    if (key_length < 0) {
	key_length = strlen(key);
    }
    for (i = 0; i < json->value.v_object.length; i++) {
	/* Replace. */
	kv = &json->value.v_object.key_values[i];
	if (kv->key_length == (size_t)key_length &&
		!memcmp(kv->key, key, key_length)) {
	    _json_free(kv->value);
	    kv->value = value;
	    return;
	}
    }

    /* Extend. */
    json->value.v_object.key_values =
	(key_value_t *)Realloc(json->value.v_object.key_values,
		(json->value.v_object.length + 1) * sizeof(key_value_t));
    kv = &json->value.v_object.key_values[json->value.v_object.length++];
    kv->key_length = key_length;
    s = Malloc(key_length + 1);
    memcpy(s, key, key_length);
    s[key_length] = '\0';
    kv->key = s;
    kv->value = value;
}

/**
 * Sets an array value.
 * The array is extended if needed, with NULLs.
 * The value is copied by reference, not cloned.
 * @param[in,out] json	Object to modify
 * @param[in] index	Array index
 * @param[in] value	Value to set
 */
void
json_array_set(json_t *json, unsigned index, json_t *value)
{
    assert(json != NULL);
    assert(json->type == JT_ARRAY);
    if (index >= json->value.v_array.length) {
	unsigned i;

	json->value.v_array.array =
	    (struct json **)Realloc(json->value.v_array.array,
		    (index + 1) * sizeof(json_t *));
	for (i = json->value.v_array.length; i <= index; i++) {
	    json->value.v_array.array[i] = NULL;
	}
	json->value.v_array.length = index + 1;
    }
    _json_free(json->value.v_array.array[index]);
    json->value.v_array.array[index] = value;
}

/**
 * Appends to an array.
 * The value is copied by reference, not cloned.
 * @param[in,out] json	Object to modify
 * @param[in] value	Value to set
 */
void
json_array_append(json_t *json, json_t *value)
{
    json_array_set(json, json_array_length(json), value);
}

/**
 * Clones a JSON object.
 * @param[in] json	Object to clone.
 * @returns cloned object
 */
json_t *
json_clone(const json_t *json)
{
    const char *s;
    size_t len;
    json_t *j;
    const char *key;
    size_t key_length;
    const json_t *member;
    unsigned i;

    switch (json_type(json)) {
    case JT_NULL:
    default:
	return NULL;
    case JT_BOOLEAN:
	return json_boolean(json_boolean_value(json));
    case JT_INTEGER:
	return json_integer(json_integer_value(json));
    case JT_DOUBLE:
	return json_double(json_double_value(json));
    case JT_STRING:
	s = json_string_value(json, &len);
	return json_string(s, len);
    case JT_OBJECT:
	j = json_object();
	BEGIN_JSON_OBJECT_FOREACH(json, key, key_length, member) {
	    json_object_set(j, key, key_length, json_clone(member));
	} END_JSON_OBJECT_FOREACH(json, key, key_length, member);
	return j;
    case JT_ARRAY:
	j = json_array();
	for (i = 0; i < json_array_length(json); i++) {
	    json_array_set(j, i, json_clone(json_array_element(json, i)));
	}
	return j;
    }
}
