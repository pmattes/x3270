/*
 * Copyright (c) 1993-2015 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
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
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES,
 * DON RUSSELL, JEFF SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ssl_passwd_gui.c
 *		SSL certificate password dialog for c3270.
 */

#include "globals.h"

#if defined(HAVE_LIBSSL) /*[*/

#include "appres.h"
# if defined(WC3270) /*[*/
#  include "cscreen.h"
# endif /*]*/
# include "ssl_passwd_gui.h"

/* Statics. */
static bool ssl_password_prompted;

/* Prompt for a password on the console. */
static char *
gets_noecho(char *buf, int size)
{
# if !defined(_WIN32) /*[*/
    char *s;
    size_t sl;

    (void) system("stty -echo");
    s = fgets(buf, size - 1, stdin);
    (void) system("stty echo");
    if (s != NULL) {
	sl = strlen(buf);
	if (sl && buf[sl - 1] == '\n') {
	    buf[sl - 1] = '\0';
	}
    }
    return s;
# else /*][*/
    int cc = 0;

    while (true) {
	char c;

	(void) screen_wait_for_key(&c);
	if (c == '\r') {
	    buf[cc] = '\0';
	    return buf;
	} else if (c == '\b' || c == 0x7f) {
	    if (cc) {
		    cc--;
	    }
	} else if (c == 0x1b) {
	    cc = 0;
	} else if ((unsigned char)c >= ' ' && cc < size - 1) {
	    buf[cc++] = c;
	}
    }
# endif /*]*/
}

/* Password callback. */
int
ssl_passwd_gui_callback(char *buf, int size)
{
    char *s;

   fprintf(stdout, "\nEnter password for Private Key: ");
   fflush(stdout);
   s = gets_noecho(buf, size);
   fprintf(stdout, "\n");
   fflush(stdout);
   ssl_password_prompted = true;
   return s? (int)strlen(s): 0;
}

/* Password GUI reset. */
void
ssl_passwd_gui_reset(void)
{
    ssl_password_prompted = false;
}

/*
 * Password GUI retry.
 * Returns true if we should try prompting for the password again.
 */
bool
ssl_passwd_gui_retry(void)
{
    return ssl_password_prompted;
}

#endif /*]*/
