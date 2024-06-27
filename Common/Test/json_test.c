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
 *      json_test.c
 *              JSON parser/formatter unit tests
 */

#include "globals.h"

#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>

#include "json.h"
#include "sa_malloc.h"

static jmp_buf jbuf;
static bool got_sigabrt;
static int dev_null;
static int old_stderr;

/* Macro to free a JSON object and make sure there are no memory leaks. */
#define CLEAN_UP do { \
    json_free(j); \
    sa_malloc_leak_check(); \
} while (false)

/*
 * Macro to free a JSON object and parse error object and make sure there are
 * no memory leaks.
 */
#define CLEAN_UP_BOTH do { \
    json_free_both(j, e); \
    sa_malloc_leak_check(); \
} while (false)

/* Macros to wrap code that is expected to fail an assertion. */
#define SIGABRT_START \
    signal(SIGABRT, sigabrt); \
    (void)dup2(dev_null, 2); \
    if (!setjmp(jbuf)) {
#define SIGABRT_END \
    } \
    signal(SIGABRT, SIG_DFL); \
    (void)dup2(old_stderr, 2); \
    assert(got_sigabrt);

static void positive_parse_tests(void);
static void negative_parse_tests(void);
static void get_tests(void);
static void write_tests(void);
static void set_tests(void);
static void iterator_tests(void);
static void clone_tests(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Positive parse", positive_parse_tests },
    { "Negative parse", negative_parse_tests },
    { "Get", get_tests },
    { "Write", write_tests },
    { "Set", set_tests },
    { "Iterator", iterator_tests },
    { "Clone", clone_tests },
    { NULL, NULL }
};

/* SIGABRT handler. */
static void
sigabrt(int signo)
{
    got_sigabrt = true;
    longjmp(jbuf, 1);
}

int
main(int argc, char *argv[])
{
    int i;
    bool verbose = false;

    if (argc > 1 && !strcmp(argv[1], "-v")) {
	verbose = true;
    }
#if defined(_MSC_VER) /*]*/
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif /*]*/

    /* Get ready for stderr redirection when we expect an assertion to fail. */
#if !defined(_WIN32) /*[*/
    dev_null = open("/dev/null", O_WRONLY);
#else /*][*/
    dev_null = open("nul", O_WRONLY);
#endif /*]*/
    old_stderr = dup(2);
    assert(dev_null >= 0);

    /* Loop through the tests. */
    for (i = 0; test[i].name != NULL; i++) {
	(*test[i].function)();
	if (verbose) {
	    printf("%s test - PASS\n", test[i].name);
	} else {
	    printf(".");
	    fflush(stdout);
	}
    }

    /* Success. */
    printf("\nPASS\n");
    return 0;
}

/* Positive parsing tests. */
static void
positive_parse_tests(void)
{
    json_errcode_t errcode;
    json_t *j = NULL;
    json_parse_error_t *e = NULL;
    const char *s;
    size_t length;
    json_t *k;
    json_t *l;

    /* Test an integer. */
#   define TEST_INT "123"
    errcode = json_parse_s(TEST_INT, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_integer(j));
    assert(json_integer_value(j) == 123);
    CLEAN_UP_BOTH;

    /* Test json_parse (no _s) with an integer. */
    errcode = json_parse(TEST_INT, strlen(TEST_INT), &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_integer(j));
    assert(json_integer_value(j) == 123);
    CLEAN_UP_BOTH;

    /* Test a negative integer. */
#   define TEST_NINT "-123"
    errcode = json_parse_s(TEST_NINT, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_integer(j));
    assert(json_integer_value(j) == -123);
    CLEAN_UP_BOTH;

    /* Test an integer with some funky white space around it. */
#   define TEST_INTW "\t\f 123\r\n"
    errcode = json_parse_s(TEST_INTW, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_integer(j));
    assert(json_integer_value(j) == 123);
    CLEAN_UP_BOTH;

    /* Test a bareword. */
#   define TEST_BARE "false"
    errcode = json_parse_s(TEST_BARE, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_boolean(j));
    assert(json_boolean_value(j) == false);
    CLEAN_UP_BOTH;

    /* Test a string. */
#   define TEST_STRING "\"xyz\""
    errcode = json_parse_s(TEST_STRING, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_string(j));
    assert(!strcmp(json_string_value(j, &length), "xyz"));
    CLEAN_UP_BOTH;

    /* Test a string with an embedded NUL. */
#   define TEST_STRINGN "\"xy\\u0000z\""
    errcode = json_parse_s(TEST_STRINGN, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_string(j));
    s = json_string_value(j, &length);
    assert(length == 4);
    assert(!memcmp(s, "xy\000z", 4));
    CLEAN_UP_BOTH;

    /* Test a simple array of integers. */
#   define TEST_ARRAY_INT "[ 1, 2, 3 ]"
    errcode = json_parse_s(TEST_ARRAY_INT, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 3);
    assert(json_integer_value(json_array_element(j, 0)) == 1);
    assert(json_integer_value(json_array_element(j, 1)) == 2);
    assert(json_integer_value(json_array_element(j, 2)) == 3);
    CLEAN_UP_BOTH;

    /* Test a simple array of strings. */
#   define TEST_ARRAY_STRING "[ \"hello\", \"there\", \"folks\" ]"
    errcode = json_parse_s(TEST_ARRAY_STRING, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 3);
    assert(!strcmp(json_string_value(json_array_element(j, 0), &length), "hello"));
    assert(!strcmp(json_string_value(json_array_element(j, 1), &length), "there"));
    assert(!strcmp(json_string_value(json_array_element(j, 2), &length), "folks"));
    CLEAN_UP_BOTH;

    /* Test a simple array of barewords. */
#   define TEST_ARRAY_BARE "[ null, true, false ]"
    errcode = json_parse_s(TEST_ARRAY_BARE, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 3);
    assert(json_array_element(j, 0) == NULL);
    assert(json_boolean_value(json_array_element(j, 1)) == true);
    assert(json_boolean_value(json_array_element(j, 2)) == false);
    CLEAN_UP_BOTH;

    /* Test a simple array of doubles. */
#   define TEST_ARRAY_DOUBLE "[ 1.2, 2.3, 3.4 ]"
    errcode = json_parse_s(TEST_ARRAY_DOUBLE, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 3);
    assert(json_double_value(json_array_element(j, 0)) == 1.2);
    assert(json_double_value(json_array_element(j, 1)) == 2.3);
    assert(json_double_value(json_array_element(j, 2)) == 3.4);
    CLEAN_UP_BOTH;

    /* Test an empty array. */
#   define TEST_EMPTY_ARRAY "[ ]"
    errcode = json_parse_s(TEST_EMPTY_ARRAY, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 0);
    CLEAN_UP_BOTH;

    /* Test a simple object of integers. */
#   define TEST_OBJECT_INT "{ \"a\": 1, \"b\": 2, \"c\": 3 }"
    errcode = json_parse_s(TEST_OBJECT_INT, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_object(j));
    assert(json_object_length(j) == 3);
    assert(json_object_member(j, "a", NT, &k));
    assert(json_integer_value(k) == 1);
    assert(json_object_member(j, "b", NT, &k));
    assert(json_integer_value(k) == 2);
    assert(json_object_member(j, "c", NT, &k));
    assert(json_integer_value(k) == 3);
    CLEAN_UP_BOTH;

    /* Test an array embedded in an object. */
#   define TEST_NEST1 "{ \"a\": [ 1, 2, 3 ], \"b\": 4 }"
    errcode = json_parse_s(TEST_NEST1, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_object(j));
    assert(json_object_length(j) == 2);
    assert(json_object_member(j, "a", NT, &k));
    assert(json_is_array(k));
    assert(json_array_length(k) == 3);
    assert(json_object_member(j, "b", NT, &k));
    assert(json_is_integer(k));
    assert(json_integer_value(k) == 4);
    CLEAN_UP_BOTH;

    /* Test an object embedded in an array. */
#   define TEST_NEST2 "[ \"a\", { \"b\": [ 1, 2, 3 ], \"c\": 4 }, true ]"
    errcode = json_parse_s(TEST_NEST2, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_array(j));
    assert(json_array_length(j) == 3);
    k = json_array_element(j, 1);
    assert(json_is_object(k));
    assert(json_object_length(k) == 2);
    assert(json_object_member(k, "b", NT, &l));
    assert(json_is_boolean(json_array_element(j, 2)));
    CLEAN_UP_BOTH;

    /* Test basic escapes. */
#   define TEST_ESCAPES "\"abc\\r\\n\\t\\f\\/\\u0041\\\\\""
    errcode = json_parse_s(TEST_ESCAPES, &j, &e);
    assert(errcode == JE_OK);
    assert(json_is_string(j));
    assert(!strcmp(json_string_value(j, &length), "abc\r\n\t\f/A\\"));
    CLEAN_UP_BOTH;

    /* Test high surrogates. */
#   define TEST_SURR "\"abc\\ud83d\\ude00\""
    errcode = json_parse_s(TEST_SURR, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc😀"));
    CLEAN_UP_BOTH;

    /* Test funky Unicode surrogates. */
#   define TEST_BAD_SURR1 "\"abc\\ud83f\""
    errcode = json_parse_s(TEST_BAD_SURR1, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\277"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR2 "\"abc\\ud83fq\""
    errcode = json_parse_s(TEST_BAD_SURR2, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\277q"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR3 "\"abc\\ud83f\n\""
    errcode = json_parse_s(TEST_BAD_SURR3, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\277\n"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR4 "\"abc\\udc00\n\""
    errcode = json_parse_s(TEST_BAD_SURR4, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\260\200\n"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR5 "\"abc\\ud83f\\ud83f\n\""
    errcode = json_parse_s(TEST_BAD_SURR5, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\277\355\240\277\n"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR6 "\"abc\\ud83d\\n\""
    errcode = json_parse_s(TEST_BAD_SURR6, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\275\n"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR7 "\"abc\\ud83d\\ud83d\""
    errcode = json_parse_s(TEST_BAD_SURR7, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\275\355\240\275"));
    CLEAN_UP_BOTH;

#   define TEST_BAD_SURR8 "\"abc\\ud83d\\u0001\""
    errcode = json_parse_s(TEST_BAD_SURR8, &j, &e);
    assert(errcode == JE_OK);
    assert(!strcmp(json_string_value(j, &length), "abc\355\240\275\001"));
    CLEAN_UP_BOTH;
}

/* Negative tests. */
static void
negative_parse_tests(void)
{
    json_errcode_t errcode;
    json_t *j = NULL;
    json_parse_error_t *e = NULL;

    /* Test empty input. */
    errcode = json_parse_s("", &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

    errcode = json_parse_s("\t", &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

    /* Test bad escapes. */
#   define TEST_BAD_ESCAPE "\"\\q\""
    errcode = json_parse_s(TEST_BAD_ESCAPE, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test junk at the end of the string. */
#   define TEST_JUNK "true?"
    errcode = json_parse_s(TEST_JUNK, &j, &e);
    assert(errcode == JE_EXTRA);
    assert(e->offset == 4);
    assert(json_is_boolean(j));
    CLEAN_UP_BOTH;

#   define TEST_JUNK2 "{\"a\":3}[1]"
    errcode = json_parse_s(TEST_JUNK2, &j, &e);
    assert(errcode == JE_EXTRA);
    assert(e->offset == 7);
    assert(json_is_object(j));
    CLEAN_UP_BOTH;

#   define TEST_JUNK3 "22 44 54"
    errcode = json_parse_s(TEST_JUNK3, &j, &e);
    assert(errcode == JE_EXTRA);
    assert(e->offset == 3);
    assert(json_is_integer(j));
    CLEAN_UP_BOTH;

    /* Test a missing tag. */
#   define TEST_MISSING_TAG "{:"
    errcode = json_parse_s(TEST_MISSING_TAG, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test integer overflow. */
#   define TEST_IOVERFLOW "92233720368547758079223372036854775807"
    errcode = json_parse_s(TEST_IOVERFLOW, &j, &e);
    assert(errcode == JE_OVERFLOW);
    CLEAN_UP_BOTH;

    /* Test integer underflow. */
#   define TEST_IUNDERFLOW "-92233720368547758079223372036854775807"
    errcode = json_parse_s(TEST_IUNDERFLOW, &j, &e);
    assert(errcode == JE_OVERFLOW);
    CLEAN_UP_BOTH;

    /* Test floating-point overflow. */
#   define TEST_FOVERFLOW "1e100000"
    errcode = json_parse_s(TEST_FOVERFLOW, &j, &e);
    assert(errcode == JE_OVERFLOW);
    CLEAN_UP_BOTH;

    /* Test floating-point garbage. */
#   define TEST_FJUNK "1eeeee"
    errcode = json_parse_s(TEST_FJUNK, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test invalid UTF-8 (not exhaustive). */
#   define TEST_BAD_UTF8 "\xc3\x28"
    errcode = json_parse_s(TEST_BAD_UTF8, &j, &e);
    assert(errcode == JE_UTF8);
    CLEAN_UP_BOTH;

    /* Test incomplete arrays. */
#   define TEST_INCOMPLETE_ARRAY1 "[ "
    errcode = json_parse_s(TEST_INCOMPLETE_ARRAY1, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_ARRAY2 "[ 1"
    errcode = json_parse_s(TEST_INCOMPLETE_ARRAY2, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_ARRAY3 "[ 1,"
    errcode = json_parse_s(TEST_INCOMPLETE_ARRAY3, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_ARRAY4 "[ 1#"
    errcode = json_parse_s(TEST_INCOMPLETE_ARRAY4, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test incomplete objects. */
#   define TEST_INCOMPLETE_OBJECT1 "{ "
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT1, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT2 "{ \"a\""
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT2, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT3 "{ \"a\":"
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT3, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT4 "{ \"a\": 3"
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT4, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT5 "{ \"a\": 3,"
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT5, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT6 "{ \"a\": {"
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT6, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_OBJECT7 "{ \"a\"&"
    errcode = json_parse_s(TEST_INCOMPLETE_OBJECT7, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test a bad tag type. */
#   define TEST_BAD_TAG "{ 13: 14}"
    errcode = json_parse_s(TEST_BAD_TAG, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test a bad separator. */
#   define TEST_BAD_SEPARATOR "{ \"a\": 14;}"
    errcode = json_parse_s(TEST_BAD_SEPARATOR, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test a nested incomplete object. */
#   define TEST_INCOMPLETE_NESTED_OBJECT "{ \"a\": { \"a\": { \"a\":"
    errcode = json_parse_s(TEST_INCOMPLETE_NESTED_OBJECT, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

    /* Test bad UTF-8 escapes. */
#   define TEST_BAD_UTF8_ESC1 "\"abc\\u\""
    errcode = json_parse_s(TEST_BAD_UTF8_ESC1, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

#   define TEST_BAD_UTF8_ESC2 "\"abc\\uZ\""
    errcode = json_parse_s(TEST_BAD_UTF8_ESC2, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

    /* Test bad escapes. */
#   define TEST_BAD_ESC1 "\"abc\\z\""
    errcode = json_parse_s(TEST_BAD_ESC1, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;

#   define TEST_INCOMPLETE_STRING1 "\"abc\\\""
    errcode = json_parse_s(TEST_INCOMPLETE_STRING1, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

    /* Incomplete string. */
#   define TEST_INCOMPLETE_STRING2 "\"abc"
    errcode = json_parse_s(TEST_INCOMPLETE_STRING2, &j, &e);
    assert(errcode == JE_INCOMPLETE);
    CLEAN_UP_BOTH;

    /* Missing value. */
#   define TEST_MISSING_VALUE "{ \"a\": }"
    errcode = json_parse_s(TEST_MISSING_VALUE, &j, &e);
    assert(errcode == JE_SYNTAX);
    CLEAN_UP_BOTH;
}

/* Getter tests. */
static void
get_tests(void)
{
    json_t *j = NULL;
    json_parse_error_t *e = NULL;
    json_t *r;
    int64_t i;
    double d;
    const char *s;
    bool b;
    size_t len;
    unsigned length;
    bool found;

    /* Test json_array_element and json_array_length. */
#   define TEST_ARRAY "[ 1, \"a\", true ]"
    json_parse_s(TEST_ARRAY, &j, &e);
    length = json_array_length(j);
    assert(length == 3);
    r = json_array_element(j, 0);
    assert(json_is_integer(r));
    r = json_array_element(j, 1);
    assert(json_is_string(r));
    r = json_array_element(j, 2);
    assert(json_is_boolean(r));
    SIGABRT_START {
	r = json_array_element(j, 3);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    json_parse_s("12", &j, &e);
    SIGABRT_START {
	r = json_array_element(j, 0);
    } SIGABRT_END;
    SIGABRT_START {
	length = json_array_length(j);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	r = json_array_element(NULL, 0);
    } SIGABRT_END;

    SIGABRT_START {
	length = json_array_length(NULL);
    } SIGABRT_END;

    /* Test json_object_member and json_object_length. */
#   define TEST_OBJECT "{ \"a\": 1, \"b\": \"xyz\", \"c\": null }"
    json_parse_s(TEST_OBJECT, &j, &e);
    assert(json_object_length(j) == 3);
    found = json_object_member(j, "a", -1, &r);
    assert(found);
    assert(json_is_integer(r));
    found = json_object_member(j, "b", -1, &r);
    assert(found);
    assert(json_is_string(r));
    found = json_object_member(j, "c", -1, &r);
    assert(found);
    assert(r == NULL);
    found = json_object_member(j, "d", -1, &r);
    assert(!found);
    assert(r == NULL);
    found = json_object_member(j, "a", 1, &r);
    assert(found);
    assert(r != NULL);
    assert(json_is_integer(r));
    CLEAN_UP_BOTH;

    /* Test json_object_member on non-object. */
    json_parse_s(TEST_ARRAY, &j, &e);
    SIGABRT_START {
	found = json_object_member(j, "a", -1, &r);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	found = json_object_member(NULL, "a", -1, &r);
    } SIGABRT_END;
    CLEAN_UP;

    /* More json_object_length tests. */
    SIGABRT_START {
	length = json_object_length(NULL);
    } SIGABRT_END;
    j = json_integer(3);
    SIGABRT_START {
	length = json_object_length(j);
    } SIGABRT_END;
    CLEAN_UP;

    /* Test json_integer_value. */
    json_parse_s(TEST_INT, &j, &e);
    i = json_integer_value(j);
    assert(i == 123);
    CLEAN_UP_BOTH;

    json_parse_s(TEST_STRING, &j, &e);
    SIGABRT_START {
	i = json_integer_value(j);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	i = json_integer_value(NULL);
    } SIGABRT_END;
    CLEAN_UP;

    /* Test json_double_value. */
#define TEST_DOUBLE "3.14"
    json_parse_s(TEST_DOUBLE, &j, &e);
    d = json_double_value(j);
    assert(d == 3.14);
    CLEAN_UP_BOTH;

    json_parse_s(TEST_INT, &j, &e);
    SIGABRT_START {
	d = json_double_value(j);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	d = json_double_value(NULL);
    } SIGABRT_END;
    CLEAN_UP;

    /* Test json_string_value. */
    json_parse_s(TEST_STRING, &j, &e);
    s = json_string_value(j, &len);
    assert(s != NULL);
    assert(len == 3);
    assert(!strcmp(s, "xyz"));
    CLEAN_UP_BOTH;

    json_parse_s(TEST_INT, &j, &e);
    SIGABRT_START {
	s = json_string_value(j, &len);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	s = json_string_value(NULL, &len);
    } SIGABRT_END;
    CLEAN_UP;

    /* Test json_boolean_value. */
#define TEST_BOOLEAN "true"
    json_parse_s(TEST_BOOLEAN, &j, &e);
    b = json_boolean_value(j);
    assert(b == true);
    CLEAN_UP_BOTH;

    json_parse_s(TEST_INT, &j, &e);
    SIGABRT_START {
	b = json_boolean_value(j);
    } SIGABRT_END;
    CLEAN_UP_BOTH;

    SIGABRT_START {
	b = json_boolean_value(NULL);
    } SIGABRT_END;
    CLEAN_UP;
}

static void
write_tests(void)
{
    json_t *j = NULL;
    json_parse_error_t *e = NULL;
    char *s;

    /* Test writing a simple array. */
#   define TEST_WARRAY "[ 1, \"a\", true ]"
    json_parse_s(TEST_WARRAY, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, "[\n  1,\n  \"a\",\n  true\n]"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test writing a simple object. */
#   define TEST_WOBJECT "{ \"a\": 1, \"b\": \"a\", \"c\": true }"
    json_parse_s(TEST_WOBJECT, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, "{\n  \"a\": 1,\n  \"b\": \"a\",\n  \"c\": true\n}"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test writing a nested array. */
#   define TEST_WARRAY_NEST "[ 1, \"a\", [ 3, [ ] ] ]"
    json_parse_s(TEST_WARRAY_NEST, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, "[\n  1,\n  \"a\",\n  [\n    3,\n    [\n    ]\n  ]\n]"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test writing a nested array with no whitespace. */
    json_parse_s(TEST_WARRAY_NEST, &j, &e);
    s = json_write_o(j, JW_ONE_LINE);
    assert(!strcmp(s, "[1,\"a\",[3,[]]]"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test writing a nested object. */
#   define TEST_WOBJECT_NEST "{ \"a\": false, \"b\":{}, \"c\": { \"d\": 3 }}"
    json_parse_s(TEST_WOBJECT_NEST, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, "{\n  \"a\": false,\n  \"b\": {\n  },\n  \"c\": {\n    \"d\": 3\n  }\n}"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test writing a nested object with no whitespace. */
#   define TEST_WOBJECT_NEST "{ \"a\": false, \"b\":{}, \"c\": { \"d\": 3 }}"
    json_parse_s(TEST_WOBJECT_NEST, &j, &e);
    s = json_write_o(j, JW_ONE_LINE);
    assert(!strcmp(s, "{\"a\":false,\"b\":{},\"c\":{\"d\":3}}"));
    Free(s);
    CLEAN_UP_BOTH;

    /* Now a funky string. */
#   define TEST_WSTRING "\"abc\r\n\\u001f\\\"s\""
    s = TEST_WSTRING;
    json_parse_s(s, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, "\"abc\\r\\n\\u001f\\\"s\""));
    Free(s);
    CLEAN_UP_BOTH;

    /* Now a funkier string. */
#   define TEST_WSTRING2 "\"😀\""
    s = TEST_WSTRING2;
    json_parse_s(s, &j, &e);
    s = json_write(j);
    assert(!strcmp(s, TEST_WSTRING2));
    Free(s);
    CLEAN_UP_BOTH;

    /* Now a funky string, expanded. */
    s = TEST_WSTRING2;
    json_parse_s(s, &j, &e);
    s = json_write_o(j, JW_EXPAND_SURROGATES);
    assert(!strcmp(s, "\"\\ud83d\\ude00\""));
    Free(s);
    CLEAN_UP_BOTH;

    /* Test an invalid UTF-8 string. */
    j = json_string("\303\050", NT);
    s = json_write(j);
    assert(!strcmp(s, "\"\303\050\""));
    Free(s);
    CLEAN_UP;

    /* Test miscellaneous control characters. */
    j = json_string("\t\f\\", NT);
    s = json_write(j);
    assert(!strcmp(s, "\"\\t\\f\\\\\""));
    Free(s);
    CLEAN_UP;

    /* Test null. */
    s = json_write(NULL);
    assert(!strcmp(s, "null"));
    Free(s);
    CLEAN_UP;

    /* Test double. */
    j = json_double(1.2);
    s = json_write(j);
    assert(!strcmp(s, "1.2"));
    Free(s);
    CLEAN_UP;
}

/* Constructor and setter tests. */
static void
set_tests(void)
{
    json_t *j = NULL;
    json_t *k;

    /* Construct a Boolean. */
    j = json_boolean(true);
    assert(json_is_boolean(j));
    assert(json_boolean_value(j) == true);
    json_free(j);

    j = json_boolean(false);
    assert(json_is_boolean(j));
    assert(json_boolean_value(j) == false);
    CLEAN_UP;

    /* Construct an integer. */
    j = json_integer(12345);
    assert(json_is_integer(j));
    assert(json_integer_value(j) == 12345);
    json_free(j);

    /* Construct a double. */
    j = json_double(1.2345);
    assert(json_is_double(j));
    assert(json_double_value(j) == 1.2345);
    json_free(j);

    /* Construct an object. */
    j = json_object();
    assert(json_is_object(j));
    json_object_set(j, "a", NT, json_integer(3));
    assert(json_object_member(j, "a", NT, &k));
    assert(json_is_integer(k));
    json_object_set(j, "a", NT, json_double(3.0));
    assert(json_object_member(j, "a", NT, &k));
    assert(json_is_double(k));
    json_object_set(j, "b", NT, NULL);
    assert(json_object_member(j, "b", NT, &k));
    assert(json_is_null(k));

    CLEAN_UP;

    SIGABRT_START {
	json_object_set(NULL, "a", NT, NULL);
    } SIGABRT_END;

    j = json_integer(3);
    SIGABRT_START {
	json_object_set(j, "a", NT, NULL);
    } SIGABRT_END;
    CLEAN_UP;

    /* Construct an array. */
    j = json_array();
    assert(json_is_array(j));
    json_array_set(j, 0, json_integer(3));
    k = json_array_element(j, 0);
    assert(json_is_integer(k));
    json_array_set(j, 2, json_double(3.0));
    k = json_array_element(j, 1);
    assert(json_is_null(k));
    k = json_array_element(j, 2);
    assert(json_is_double(k));
    CLEAN_UP;

    /* Construct an array by appending. */
    j = json_array();
    assert(json_is_array(j));
    json_array_append(j, json_integer(3));
    k = json_array_element(j, 0);
    assert(json_is_integer(k));
    json_array_append(j, json_double(3.0));
    k = json_array_element(j, 1);
    assert(json_is_double(k));
    CLEAN_UP;

    SIGABRT_START {
	json_array_set(NULL, 27, NULL);
    } SIGABRT_END;

    j = json_integer(3);
    SIGABRT_START {
	json_array_set(j, 27, NULL);
    } SIGABRT_END;
    CLEAN_UP;
}

/* Iterator tests. */
static void
iterator_tests(void)
{
    json_t *j;
    const char *key;
    size_t key_length;
    const json_t *element;
    int count = 0;
    size_t key_lengths = 0;

    j = json_object();
    json_object_set(j, "a", NT, json_integer(1));
    json_object_set(j, "bc", NT, json_string("hello", NT));
    json_object_set(j, "def", NT, json_double(1.2));

    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, element) {
	count++;
	key_lengths += key_length;
    } END_JSON_OBJECT_FOREACH(j, key, key_length, element);
    assert(count == 3);
    assert(key_lengths == 6);
    CLEAN_UP;

    j = json_integer(3);
    SIGABRT_START {
	BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, element) {
	    count++;
	} END_JSON_OBJECT_FOREACH(j, key, key_length, element);
    } SIGABRT_END;
    CLEAN_UP;

    SIGABRT_START {
	BEGIN_JSON_OBJECT_FOREACH(NULL, key, key_length, element) {
	    count++;
	} END_JSON_OBJECT_FOREACH(NULL, key, key_length, element);
    } SIGABRT_END;
    CLEAN_UP;

    j = json_object();
    count = 0;
    BEGIN_JSON_OBJECT_FOREACH(j, key, key_length, element) {
	count++;
    } END_JSON_OBJECT_FOREACH(j, key, key_length, element);
    assert(count == 0);
    CLEAN_UP;
}

/* Clone tests. */
static void
clone_tests(void)
{
    json_t *j = NULL;
    json_t *k;
    size_t length;
    json_t *l;

    k = json_clone(NULL);
    assert(json_is_null(k));
    CLEAN_UP;

    j = json_boolean(true);
    k = json_clone(j);
    assert(json_is_boolean(k));
    assert(json_boolean_value(k) == true);
    json_free(k);
    CLEAN_UP;

    j = json_integer(3);
    k = json_clone(j);
    assert(json_is_integer(k));
    assert(json_integer_value(k) == 3);
    json_free(k);
    CLEAN_UP;

    j = json_double(3.14);
    k = json_clone(j);
    assert(json_is_double(k));
    assert(json_double_value(k) == 3.14);
    json_free(k);
    CLEAN_UP;

    j = json_string("foo", NT);
    k = json_clone(j);
    assert(json_is_string(k));
    assert(!strcmp(json_string_value(k, &length), "foo"));
    json_free(k);
    CLEAN_UP;

    j = json_object();
    json_object_set(j, "a", NT, json_integer(22));
    json_object_set(j, "b", NT, json_double(1.414));
    k = json_clone(j);
    assert(json_is_object(k));
    assert(json_object_length(k) == 2);
    assert(json_object_member(k, "a", NT, &l));
    assert(json_is_integer(l));
    assert(json_integer_value(l) == 22);
    json_free(k);
    CLEAN_UP;

    j = json_array();
    json_array_set(j, 0, json_integer(1));
    json_array_set(j, 1, NULL);
    k = json_clone(j);
    assert(json_is_array(k));
    assert(json_array_length(k) == 2);
    assert(json_integer_value(json_array_element(k, 0)) == 1);
    json_free(k);
    CLEAN_UP;

    /* Nested. */
    j = json_array();
    k = json_array();
    json_array_set(k, 0, json_integer(1));
    json_array_set(k, 1, NULL);
    json_array_set(j, 0, k);
    json_array_set(j, 1, json_double(9.99));
    k = json_clone(j);
    assert(json_is_array(k));
    assert(json_array_length(k) == 2);
    assert(json_is_array(json_array_element(k, 0)));
    assert(json_integer_value(json_array_element(json_array_element(k, 0), 0)) == 1);
    assert(json_double_value(json_array_element(k, 1)) == 9.99);
    json_free(k);
    CLEAN_UP;
}
