/*
 * Copyright (c) 2021-2022 Paul Mattes.
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
 *      json.h
 *              JSON parsing library.
 */

#define NT	((ssize_t)-1)

/* Integer data type. */
#define JSON_INT_PRINT	PRId64

/*
 * Data types.
 * A 'null' token is a NULL pointer, not a json_t with type NULL.
 */
typedef enum {
    JT_NULL,			/* null (not really a type, but returned by
				    json_type()) */
    JT_BOOLEAN,			/* Boolean */
    JT_INTEGER,			/* integer (int64_t) */
    JT_DOUBLE,			/* floating-point number (double) */
    JT_STRING,			/* string */
    JT_OBJECT,			/* object { } */
    JT_ARRAY,			/* array [ ] */
} json_type_t;

/* Error codes. */
typedef enum {
    JE_OK,			/* no error */
    JE_UTF8,			/* parse: UTF-8 decoding error */
    JE_SYNTAX,			/* parse: syntax error */
    JE_INCOMPLETE,		/* parse: incomplete object */
    JE_OVERFLOW,		/* parse: numeric overflow */
    JE_EXTRA,			/* parse: extra junk after object */
} json_errcode_t;

/* Parse error structure. */
typedef struct {
    json_errcode_t errcode;	/* error code */
    int line;			/* line number */
    int column;			/* column number */
    const char *errmsg;		/* error message */
    size_t offset;		/* byte offset */
} json_parse_error_t;

/* Parse text into JSON. */
json_errcode_t json_parse(const char *text, ssize_t len, json_t **result,
	json_parse_error_t **error);
#define json_parse_s(t, r, e) json_parse(t, NT, r, e)

/* Free a JSON node recursively. */
json_t *_json_free(json_t *json);
json_parse_error_t *_json_free_error(json_parse_error_t *error);
#define json_free(j) do { \
    j = _json_free(j); \
} while (false)
#define json_free_error(e) do { \
    e = _json_free_error(e); \
} while (false)
#define json_free_both(j, e) do { \
    j = _json_free(j); \
    e = _json_free_error(e); \
} while (false)

/* Converts a JSON node to a JSON-formatted string. */
#define JW_NONE			0x0
#define JW_EXPAND_SURROGATES	0x1
#define JW_ONE_LINE		0x2
char *json_write_o(const json_t *json, unsigned options);
#define json_write(j)	json_write_o(j, JW_NONE)

/* Returns the type of a JSON object. Works for NULL. */
json_type_t json_type(const json_t *json);
#define json_is_null(j)		((j) == NULL)
#define json_is_boolean(j)	(json_type(j) == JT_BOOLEAN)
#define json_is_integer(j)	(json_type(j) == JT_INTEGER)
#define json_is_double(j)	(json_type(j) == JT_DOUBLE)
#define json_is_string(j)	(json_type(j) == JT_STRING)
#define json_is_object(j)	(json_type(j) == JT_OBJECT)
#define json_is_array(j)	(json_type(j) == JT_ARRAY)

/* Returns the length of an array. */
unsigned json_array_length(const json_t *json);

/* Returns the length of an object (number of members). */
unsigned json_object_length(const json_t *json);

/* Returns the array element at the given index. */
json_t *json_array_element(const json_t *json, unsigned index);

/* Returns the object member with the given key. */
bool json_object_member(const json_t *json, const char *key,
	ssize_t key_length, json_t **ret);

/* Returns the next object member. */
bool _json_object_member_next(void **context, const json_t *json,
	const char **key, size_t *key_length, const json_t **ret);

/* Returns the integer value. */
int64_t json_integer_value(const json_t *json);

/* Returns the double value. */
double json_double_value(const json_t *json);

/* Returns the string value. */
const char *json_string_value(const json_t *json, size_t *len);

/* Returns the boolean value. */
bool json_boolean_value(const json_t *json);

/* Constructors. */
json_t *json_boolean(bool value);
json_t *json_integer(int64_t value);
json_t *json_double(double value);
json_t *json_string(const char *text, ssize_t length);
#define json_string_s(t) json_string(t, NT)
json_t *json_object(void);
json_t *json_array(void);
void json_object_set(json_t *json, const char *key, ssize_t key_length,
	json_t *value);
void json_array_set(json_t *json, unsigned index, json_t *value);
void json_array_append(json_t *json, json_t *value);

/* Iterators. */
#define BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, member) do { \
    void *_context = NULL; \
    while (_json_object_member_next(&_context, j, &key, &key_length, &member))
#define END_JSON_OBJECT_FOREACH(j, key, key_length, member) } while (false)

/* Clone. */
json_t *json_clone(const json_t *json);
