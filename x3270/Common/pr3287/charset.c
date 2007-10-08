/*
 * Copyright 2001, 2004 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	charset.c
 *		Limited character set support.
 */

#include "globals.h"
#include "tablesc.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include "widec.h"

typedef struct {
	const char *name;
	unsigned char *map;
} cs_t;

/* Built-in character sets. */
static unsigned char us_intl_map[] = { 0 };
static unsigned char us_map[] = { 0 };
static unsigned char apl_map[] = { 0 };
static unsigned char bracket_map[] = { 0xad, 0x5b, 0xba, 0xdd, 0xbd, 0x5d,
    0xbb, 0xa8, 0 };
static unsigned char oldibm_map[] = { 0xad, 0x5b, 0xba, 0xdd, 0xbd, 0x5d,
    0xbb, 0xa8, 0 };
static unsigned char german_map[] = { 0x43, 0x7b, 0x4a, 0xc4, 0x4f, 0x21,
    0x59, 0x7e, 0x5a, 0xdc, 0x5f, 0x5e, 0x63, 0x5b, 0x6a, 0xf6, 0x7c, 0xa7,
    0xa1, 0xdf, 0xb0, 0xa2, 0xb5, 0x40, 0xba, 0xac, 0xbb, 0x7c, 0xc0, 0xe4,
    0xcc, 0xa6, 0xd0, 0xfc, 0xdc, 0x7d, 0xe0, 0xd6, 0xec, 0x5c, 0xfc, 0x5d, 0 };
static unsigned char finnish_map[] = { 0x43, 0x7b, 0x47, 0x7d, 0x4a, 0xa7,
    0x4f, 0x21, 0x51, 0x60, 0x5a, 0xa4, 0x5b, 0xc5, 0x5f, 0x5e, 0x63, 0x23,
    0x67, 0x24, 0x6a, 0xf6, 0x71, 0x5c, 0x79, 0xe9, 0x7b, 0xc4, 0x7c, 0xd6,
    0x9f, 0x5d, 0xa1, 0xfc, 0xb1, 0xa2, 0xb5, 0x5b, 0xba, 0xac, 0xbb, 0x7c,
    0xc0, 0xe4, 0xcc, 0xa6, 0xd0, 0xe5, 0xdc, 0x7e, 0xe0, 0xc9, 0xec, 0x40, 0 };
static unsigned char uk_map[] = { 0x4a, 0x24, 0x5b, 0xa3, 0xa1, 0xaf, 0xb0,
    0xa2, 0xb1, 0x5b, 0xba, 0x5e, 0xbc, 0x7e, 0 };
static unsigned char norwegian_map[] = { 0x47, 0x7d, 0x4a, 0x23, 0x4f, 0x21,
    0x5a, 0xa4, 0x5b, 0xc5, 0x5f, 0x5e, 0x67, 0x24, 0x6a, 0xf8, 0x70, 0xa6,
    0x7b, 0xc6, 0x7c, 0xd8, 0x80, 0x40, 0x9c, 0x7b, 0x9e, 0x5b, 0x9f, 0x5d,
    0xa1, 0xfc, 0xb0, 0xa2, 0xba, 0xac, 0xbb, 0x7c, 0xc0, 0xe6, 0xd0, 0xe5,
    0xdc, 0x7e, 0 };
static unsigned char french_map[] = { 0x44, 0x40, 0x48, 0x5c, 0x4a, 0xb0,
    0x4f, 0x21, 0x51, 0x7b, 0x54, 0x7d, 0x5a, 0xa7, 0x5f, 0x5e, 0x6a, 0xf9,
    0x79, 0xb5, 0x7b, 0xa3, 0x7c, 0xe0, 0x90, 0x5b, 0xa0, 0x60, 0xa1, 0xa8,
    0xb0, 0xa2, 0xb1, 0x23, 0xb5, 0x5d, 0xba, 0xac, 0xbb, 0x7c, 0xbd, 0x7e,
    0xc0, 0xe9, 0xd0, 0xe8, 0xe0, 0xe7, 0 };
static unsigned char icelandic_map[] = { 0xa1, 0xf6, 0x5f, 0xd6, 0x79, 0xf0,
     0x7c, 0xd0, 0xc0, 0xfe, 0x4a, 0xde, 0xd0, 0xe6, 0x5a, 0xc6, 0xcc, 0x7e,
    0x4f, 0x21, 0x8e, 0x7b, 0x9c, 0x7d, 0xae, 0x5b, 0x9e, 0x5d, 0xac, 0x40,
    0xbe, 0x5c, 0x7d, 0x27, 0x8c, 0x60, 0x6a, 0x7c, 0 };
static unsigned char belgian_map[] = { 0x4a, 0x5b, 0x4f, 0x21, 0x5a, 0x5d,
    0x5f, 0x5e, 0xb0, 0xa2, 0xba, 0xac, 0xbb, 0x7c, 0 };

static cs_t cs[] = {
	{ "us-intl", us_intl_map },
	{ "us", us_map },
	{ "apl", apl_map },
	{ "bracket", bracket_map },
	{ "oldibm", oldibm_map },
	{ "german", german_map },
	{ "finnish", finnish_map },
	{ "uk", uk_map },
	{ "norwegian", norwegian_map },
	{ "french", french_map },
	{ "icelandic", icelandic_map },
	{ "belgian", belgian_map },
	{ NULL, NULL }
};

unsigned long cgcsgid = 0x02b90025;
unsigned long cgcsgid_dbcs = 0x02b90025;
int dbcs = 0;

char *encoding = CN;
char *converters = CN;

/*
 * Parse a remapping string (<ebc>=<iso>), and remap the specific EBCDIC code.
 * Also understand the syntax "cgcsgid=<n>" for assigning a new CGCSGID.
 * Returns 0 for success, -1 for a parsing problem.
 */
static int
remap_pair(const char *s)
{
	unsigned long ebc, iso;
	char *ptr;

	if (!strncmp(s, "cgcsgid=", 8)) {
		unsigned long c;

		c = strtoul(s + 8, &ptr, 0);
		if (c == 0 || *ptr != '\0')
			return -1;
		cgcsgid = c;
		return 0;
	}
#if defined(X3270_DBCS) /*[*/
	if (!strncmp(s, "cgcsgid_dbcs=", 13)) {
		unsigned long c;

		c = strtoul(s + 13, &ptr, 0);
		if (c == 0 || *ptr != '\0')
			return -1;
		cgcsgid_dbcs = c;
		return 0;
	}
	if (!strncmp(s, "encoding=", 9)) {
		Replace(encoding, NewString((char *)s + 9));
		return 0;
	}
	if (!strncmp(s, "converters=", 11)) {
		Replace(converters, NewString((char *)s + 11));
		return 0;
	}
#endif /*]*/
	ebc = strtoul(s, &ptr, 0);
	if (ptr == s || (ebc & ~0xff) || *ptr != '=')
		return -1;
	s = ptr + 1;
	iso = strtoul(s, &ptr, 0);
	if (ptr == s || (iso & ~0xff) || *ptr)
		return -1;
	ebc2asc[ebc] = iso;
	return 0;
}

/*
 * Change character sets.
 * Returns 0 if the new character set was found, -1 otherwise.
 */
int
charset_init(const char *csname)
{
	cs_t *c;
	int i;

	/* Set up ebc2asc. */
	(void) memcpy(ebc2asc, ebc2asc0, 256);
	if (csname == NULL)
		return 0;

	/* If the name begins with an '@', the balance is a file name. */
	if (csname[0] == '@') {
		FILE *f;
		char buf[1024];
		int lno = 0;

		if (!*(csname + 1)) {
			errmsg("Empty charset file name");
			return -1;
		}
		f = fopen(csname + 1, "r");
		if (f == NULL) {
			errmsg("Charset file '%s': %s", csname + 1,
			    strerror(errno));
			return -1;
		}
		while (fgets(buf, sizeof(buf), f) != NULL) {
			char *s = buf;
			char *token;

			lno++;
			while (isspace(*s))
				s++;
			if (*s == '#')
				continue;
			while ((token = strtok(s, " \t\r\n")) != NULL) {
				s = NULL;
				if (remap_pair(token) < 0) {
					errmsg("%s, line %d: invalid value "
					    "'%s'", csname + 1, lno, token);
					fclose(f);
					return -1;
				}
			}
		}
		fclose(f);
#if defined(X3270_DBCS) /*[*/
		if (converters != CN) {
			if (wide_init(converters, encoding) < 0)
				return -1;
			dbcs = 1;
		}
#endif /*]*/
		return 0;
	}

	/* If the name begins with an '=', the balance is a literal string. */
	if (csname[0] == '=') {
		char *s0, *s;
		char *token;

		if (!*(csname + 1))
			return 0;
		s0 = malloc(strlen(csname + 1) + 1);
		if (s0 == NULL) {
			errmsg("Insufficient memory to parse charset");
			return -1;
		}
		s = strcpy(s0, csname + 1);
		while ((token = strtok(s, " \t\r\n")) != NULL) {
			s = NULL;
			if (remap_pair(token) < 0) {
				errmsg("charset invalid value '%s'", token);
				free(s0);
				return -1;
			}
		}
		free(s0);
		return 0;
	}

	/* Otherwise, it's a built-in name. */
	for (c = cs; c->name != NULL; c++) {
		if (!strcmp(csname, c->name))
			break;
	}
	if (c->name == NULL) {
		errmsg("No such charset: %s", csname);
		return -1;
	}

	/* Remap 'em. */
	for (i = 0; c->map[i]; i += 2) {
		ebc2asc[c->map[i]] = c->map[i+1];
	}
	return 0;
}
