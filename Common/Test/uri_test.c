/*
 * Copyright (c) 2022-2024 Paul Mattes.
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
 *      uri_test.c
 *              URI parser unit tests
 */

#include "globals.h"

#include <assert.h>

#include "split_host.h"
#include "uri.h"

static void basic_test(void);
static void tn3270_test(void);
static void telnet_test(void);
static void telnets_test(void);
static void ipv6_test(void);
static void ipv6_noport_test(void);
static void ipv6_percent_test(void);
static void percent_query_test(void);
static void percent_host_test(void);
static void percent_host_port_test(void);
static void percent_username_test(void);
static void percent_username_password_test(void);
static void path_edge_test(void);
static void fragment_edge_test(void);
static void all_percent_test(void);
static void is_uri_test(void);
static void fail_test(void);

static struct {
    const char *name;
    void (*function)(void);
} test[] = {
    { "Basic", basic_test },
    { "tn3270", tn3270_test },
    { "telnet", telnet_test },
    { "telnets", telnets_test },
    { "ipv6", ipv6_test },
    { "ipv6_noport", ipv6_noport_test },
    { "ipv6_percent", ipv6_percent_test },
    { "percent_query", percent_query_test },
    { "percent_host", percent_host_test },
    { "percent_host_port", percent_host_port_test },
    { "percent_username", percent_username_test },
    { "percent_username_password", percent_username_password_test },
    { "path_edge", path_edge_test },
    { "fragment_edge", fragment_edge_test },
    { "all_percent", all_percent_test },
    { "is_uri", is_uri_test },
    { "fail", fail_test },
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

/* Basic test. */
static void
basic_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("tn3270s://user:pass@localhost:2023?lu=IBMXYZ?accepthostname=bob?waitoutput=false?verifyhostcert=false",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "2023"));
    assert(prefixes == ((1 << TLS_HOST) | (1 << NO_LOGIN_HOST) | (1 << NO_VERIFY_CERT_HOST)));
    assert(!strcmp(username, "user"));
    assert(!strcmp(password, "pass"));
    assert(!strcmp(lu, "IBMXYZ"));
    assert(!strcmp(accept, "bob"));
}

/* Other schemes. */
static void
tn3270_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("tn3270://localhost",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "23"));
    assert(prefixes == 0);
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
telnet_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnet://localhost",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "23"));
    assert(prefixes == 1 << ANSI_HOST);
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
telnets_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://localhost",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* IPv6. */
static void
ipv6_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://[1:2:3]:29",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "1:2:3"));
    assert(!strcmp(port, "29"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
ipv6_noport_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://[1:2:3]",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "1:2:3"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
ipv6_percent_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://[1:2:%33]:%32%39",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "1:2:3"));
    assert(!strcmp(port, "29"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* Percent encoded query. */
static void
percent_query_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://[1:2:3]?accepthostname=foo%20bar",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "1:2:3"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(!strcmp(accept, "foo bar"));
}

/* Percent encoded host. */
static void
percent_host_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://foo%20bar/",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "foo bar"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* Percent encoded host with port. */
static void
percent_host_port_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://foo%20bar:99/",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "foo bar"));
    assert(!strcmp(port, "99"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* Percent encoded username. */
static void
percent_username_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://my%20gosh@foo:99/",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "foo"));
    assert(!strcmp(port, "99"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(!strcmp(username, "my gosh"));
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* Percent encoded username and password. */
static void
percent_username_password_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://my%3agosh:pass%20word@foo:99/",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "foo"));
    assert(!strcmp(port, "99"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(!strcmp(username, "my:gosh"));
    assert(!strcmp(password, "pass word"));
    assert(lu == NULL);
    assert(accept == NULL);
}

/* Some edge cases. */
static void
path_edge_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://localhost/",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
fragment_edge_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri("telnets://localhost#",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "992"));
    assert(prefixes == ((1 << ANSI_HOST) | (1 << TLS_HOST)));
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

/* All percent encoded test. */
static void
all_percent_test(void)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    /* "tn3270s://user:pass@localhost:2023?lu=IBMXYZ?accepthostname=bob?waitoutput=false?verifyhostcert=false#" */
    success=parse_x3270_uri("%74%6e%33%32%37%30%73://%75%73%65%72:%70%61%73%73@%6c%6f%63%61%6c%68%6f%73%74:%32%30%32%33?%6c%75%3d%49%42%4d%58%59%5a%3f%61%63%63%65%70%74%68%6f%73%74%6e%61%6d%65%3d%62%6f%62%3f%77%61%69%74%6f%75%74%70%75%74%3d%66%61%6c%73%65%3f%76%65%72%69%66%79%68%6f%73%74%63%65%72%74%3d%66%61%6c%73%65#",
	    &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == true);
    assert(error == NULL);
    assert(!strcmp(host, "localhost"));
    assert(!strcmp(port, "2023"));
    assert(prefixes == ((1 << TLS_HOST) | (1 << NO_LOGIN_HOST) | (1 << NO_VERIFY_CERT_HOST)));
    assert(!strcmp(username, "user"));
    assert(!strcmp(password, "pass"));
    assert(!strcmp(lu, "IBMXYZ"));
    assert(!strcmp(accept, "bob"));
}

/* URI test. */
static void
is_uri_test(void)
{
    assert(is_x3270_uri("tn3270://foo") == true);
    assert(is_x3270_uri("foo") == false);
}

/* Errors. */
static void
common_fail(const char *uri)
{
    char *host, *port, *username, *password, *lu, *accept;
    unsigned prefixes;
    const char *error;
    bool success;

    success = parse_x3270_uri(uri, &host, &port, &prefixes, &username, &password, &lu, &accept, &error);
    assert(success == false);
    assert(error != NULL);
    assert(host == NULL);
    assert(port == NULL);
    assert(prefixes == 0);
    assert(username == NULL);
    assert(password == NULL);
    assert(lu == NULL);
    assert(accept == NULL);
}

static void
fail_test(void)
{
    common_fail("foo");
    common_fail("funky://foo");
    common_fail("funky://foo:baz");
    common_fail("tn3270://foo/bar/baz");
    common_fail("tn3270://foo#fred");
    common_fail("tn3270://[abc:");
    common_fail("tn3270://[abc]:");
    common_fail("tn3270://[abc]$");
    common_fail("tn3270://foo:");
    common_fail("tn3270://foo:65556");
    common_fail("tn3270://foo:6%xq55");
    common_fail("tn3270://[]");
    common_fail("tn3270://[farp]");
    common_fail("tn3270://?foo");
    common_fail("tn3270://fred/foo?bob");
    common_fail("tn3270://fred/foo#bob");
}
