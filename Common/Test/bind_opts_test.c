/*
 * Copyright (c) 2021-2025 Paul Mattes.
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
 *      bind_opts_test.c
 *              bind options unit tests
 */

#include "globals.h"

#include <assert.h>
#if !defined(_WIN32) /*[*/
# include <sys/types.h>
# include <netdb.h>
#endif /*]*/

#include "bind-opt.h"
#include "resolver.h"
#include "sa_malloc.h"
#include "utils.h"

#if defined(_WIN32) /*[*/
# include "w3misc.h"
#endif /*]*/

/* Macro to make sure there are no memory leaks. */
#define CLEAN_UP do { \
    sa_malloc_leak_check(); \
} while (false)

static void positive_parse_tests(void);
static void negative_parse_tests(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Positive parse", positive_parse_tests },
    { "Negative parse", negative_parse_tests },
    { NULL, NULL }
};

int
main(int argc, char *argv[])
{
    int i;
    bool verbose = false;

    if (argc > 1 && !strcmp(argv[1], "-v")) {
	verbose = true;
    }

#if defined(_WIN32) /*[*/
    sockstart();
#endif /*]*/

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
    struct sockaddr *sa;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;
    socklen_t len;
    static uint8_t in6_loopback[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
    };
    static uint64_t in6_any[2] = { 0, 0 };
    static uint8_t in6_mapped[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 127, 0, 0, 1
    };

    /* Basic IPv4. */
    assert(parse_bind_opt("5", &sa, &len));
    assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *)sa;
    assert(sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    assert(sin->sin_port == htons(5));
    Free(sa);
    CLEAN_UP;

    assert(parse_bind_opt(":6", &sa, &len));
    assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *)sa;
    assert(sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    assert(sin->sin_port == htons(6));
    Free(sa);
    CLEAN_UP;

    /* IPv6 any. */
    assert(parse_bind_opt("0.0.0.0:7", &sa, &len));
    assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *)sa;
    assert(sin->sin_addr.s_addr == htonl(INADDR_ANY));
    assert(sin->sin_port == htons(7));
    Free(sa);
    CLEAN_UP;

    /* Basic IPv6. */
    assert(parse_bind_opt("[::1]:8", &sa, &len));
    assert(sa->sa_family == AF_INET6);
    sin6 = (struct sockaddr_in6 *)sa;
    assert(!memcmp(&sin6->sin6_addr, in6_loopback, sizeof(in6_loopback)));
    assert(sin6->sin6_port == htons(8));
    Free(sa);
    CLEAN_UP;

    /* IPv6 any. */
    assert(parse_bind_opt("[::]:9", &sa, &len));
    assert(sa->sa_family == AF_INET6);
    sin6 = (struct sockaddr_in6 *)sa;
    assert(!memcmp(&sin6->sin6_addr, in6_any, sizeof(in6_any)));
    assert(sin6->sin6_port == htons(9));
    Free(sa);
    CLEAN_UP;

    /* IPv6-mapped IPv4. */
    assert(parse_bind_opt("[::ffff:127.0.0.1]:10", &sa, &len));
    assert(sa->sa_family == AF_INET6);
    sin6 = (struct sockaddr_in6 *)sa;
    assert(!memcmp(&sin6->sin6_addr, in6_mapped, sizeof(in6_mapped)));
    assert(sin6->sin6_port == htons(10));
    Free(sa);
    CLEAN_UP;

    /* Quoted IPv4. */
    assert(parse_bind_opt("[127.0.0.1]:11", &sa, &len));
    assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *)sa;
    assert(sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    assert(sin->sin_port == htons(11));
    Free(sa);
    CLEAN_UP;
}

/* Negative tests. */
static void
negative_parse_tests(void)
{
    struct sockaddr *sa;
    socklen_t len;

    /* Pure junk. */
    assert(!parse_bind_opt("?", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt(":", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("3x", &sa, &len));
    CLEAN_UP;

    /* Incomplete. */
    assert(!parse_bind_opt("[::]", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("[::]:", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("[::", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("[", &sa, &len));
    CLEAN_UP;

    /* More junk. */
    assert(!parse_bind_opt("[?]:22", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("[::]:22x", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt(":22x", &sa, &len));
    CLEAN_UP;
    assert(!parse_bind_opt("22x", &sa, &len));
    CLEAN_UP;
}
