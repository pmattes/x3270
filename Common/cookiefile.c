/*
 * Copyright (c) 2024 Paul Mattes.
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
 *      cookiefile.c
 *              Cookie file operations.
 */

#include "globals.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "appres.h"
#include "utils.h"
#include "wincmn.h"

#if defined(_WIN32) /*[*/
# include "aclapi.h"
# include "accctrl.h"
#endif /*]*/

#include "cookiefile.h"

#define GEN_LENGTH	64

static char *cookie_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.";

static char *
gen_cookie(void)
{
    static char buf[GEN_LENGTH + 1];
    int i;

    for (i = 0; i < GEN_LENGTH; i++) {
	buf[i] = cookie_chars[random() % strlen(cookie_chars)];
    }
    buf[GEN_LENGTH] = '\0';
    return buf;
}

/**
 * Cookie file initialization.
 * @return true if initialization was successful
 */
bool
cookiefile_init(void)
{
    static char bad_chars[] = { '=', ';', ' ', '"', '\\', '(', ')', ',', '#', '@', ':', '?', 0 };
    int i;
    int fd = -1;
    char *cookie;
#if defined(_WIN32) /*[*/
    EXPLICIT_ACCESS eas[1];
    PACL pacl = 0;
    DWORD rc;
#endif /*]*/

    if (appres.cookie_file == NULL) {
	return true;
    }

    if (access(appres.cookie_file, R_OK) == 0) {
	char read_buf[1024];
	ssize_t nr;

	/* The file exists. Read the cookie from it. */
	fd = open(appres.cookie_file, O_RDONLY);
	if (fd < 0) {
	    perror(appres.cookie_file);
	    goto fail;
	}
	nr = read(fd, read_buf, sizeof(read_buf) - 1);
	if (nr < 0) {
	    perror(appres.cookie_file);
	    goto fail;
	}
	while (nr > 0 && isspace((int)read_buf[nr - 1])) {
	    nr--;
	}
	read_buf[nr] = '\0';
	if (!nr) {
	    char *gen = gen_cookie();
	    ssize_t nw;

	    /* Re-open the file in write mode. */
	    close(fd);
	    fd = open(appres.cookie_file, O_WRONLY | O_TRUNC);
	    if (fd < 0) {
		perror(appres.cookie_file);
		goto fail;
	    }

	    /* Generate a new cookie and write it to the file. */
	    lseek(fd, 0, SEEK_SET);
	    nw = write(fd, gen, (unsigned int)strlen(gen));
	    if (nw < 0) {
		perror(appres.cookie_file);
		goto fail;
	    }
	    cookie = gen;
	} else {
	    /* Check for invalid characters. */
	    for (i = 0; read_buf[i]; i++) {
		if (isspace((int)read_buf[i])) {
		    fprintf(stderr, "%s containts an invalid cookie, contains whitespace\n", appres.cookie_file);
		    goto fail;
		}
	    }
	    for (i = 0; bad_chars[i]; i++) {
		if (strchr(read_buf, bad_chars[i]) != NULL) {
		    fprintf(stderr, "%s contains an invalid cookie, contains '%c'\n", appres.cookie_file, bad_chars[i]);
		    goto fail;
		}
	    }
	    cookie = read_buf;
	}
    } else {
	char *gen = gen_cookie();

	/* Create the file. */
#if !defined(_WIN32) /*[*/
	fd = open(appres.cookie_file, O_WRONLY | O_CREAT | O_EXCL, 0400);
#else /*][*/
	fd = open(appres.cookie_file, O_WRONLY | O_CREAT | O_EXCL, S_IREAD);
#endif /*]*/
	if (fd < 0) {
	    perror(appres.cookie_file);
	    goto fail;
	}
	if (write(fd, gen, (unsigned int)strlen(gen)) < 0) {
	    perror("cookiefile write");
	    goto fail;
	}
	cookie = gen;
    }

    /* Make the file reasonably secure. */
#if !defined(_WIN32) /*[*/
    chmod(appres.cookie_file, 0400);
#else /*][*/
    eas[0].grfAccessPermissions = GENERIC_ALL;
    eas[0].grfAccessMode = GRANT_ACCESS;
    eas[0].grfInheritance = NO_INHERITANCE;
    eas[0].Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    eas[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    eas[0].Trustee.ptstrName = "CURRENT_USER";

    rc = SetEntriesInAcl(1, &eas[0], NULL, &pacl);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "SetEntriesInAcl(%s) failed: 0x%x\n", appres.cookie_file, (unsigned)rc);
	goto fail;
    }

    rc = SetNamedSecurityInfoA(appres.cookie_file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
	    NULL, NULL, pacl, NULL);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "SetNamedSecurityInfo(%s) failed: 0x%x\n", appres.cookie_file, (unsigned)rc);
	goto fail;
    }
#endif

    if (fd >= 0) {
	close(fd);
    }
    security_cookie = NewString(cookie);
    return true;

fail:
    fflush(stderr);
    if (fd >= 0) {
	close(fd);
    }
    return false;
}
