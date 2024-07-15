/*
 * Copyright (c) 2007-2023 Paul Mattes.
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
 *	wincmn.h
 * 		Common Windows definitions.
 */

#if defined(_WIN32) /*[*/

/*
 * Pull in the Windows header files needed for the function names redefined
 * below, and containing the typedefs for the substitute functions defined
 * below.
 */
# define WIN32_LEAN_AND_MEAN 1		/* Skip things we don't need */
# include <winsock2.h>                  /* Has to come before windows.h */
# include <windows.h>                   /* Common definitions for Windows */

# include <direct.h>
# include <io.h>
# include <process.h>
# include <shellapi.h>
# include <shlobj.h>
# include <stdarg.h>
# include <ws2tcpip.h>

/*
 * Windows has inet_ntop() only in Vista and up.  We define our own in
 * w3misc.c.
 */
# define inet_ntop my_inet_ntop
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);

/*
 * MinGW has no IsWindowsVersionOrGreater().
 */
# if !defined(__GNUC__) /*[*/
#  include <VersionHelpers.h>
# else /*][*/
#  define IsWindowsVersionOrGreater my_IsWindowsVersionOrGreater
extern BOOL IsWindowsVersionOrGreater(WORD major_version, WORD minor_version,
	        WORD service_pack_major);
# endif /*]*/

/*
 * Windows has no in_addr_t.
 */
typedef unsigned long in_addr_t;

/*
 * Prior to VS2013, Windows did not define va_copy().
 */
# if !defined(va_copy) /*[*/
#  define va_copy(to, from)	(to) = (from)
# endif /*]*/

/*
 * Windows snprintf/vsnprintf do not guarantee NUL termination, so we have our
 * own.
 */
int safe_vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int safe_snprintf(char *str, size_t size, const char *fmt, ...);
# if !defined(IS_SNPRINTF_C) /*[*/
#  define vsnprintf	safe_vsnprintf
#  define snprintf	safe_snprintf
# endif /*]*/

/*
 * We always use _vscprintf instead of vscprintf.
 */
# define vscprintf	_vscprintf

/* Windows has rand(), not random(). */
# define random		rand

# if defined(_MSC_VER) /*[*/

/* MSVC does not define the constants for access(). */
#  define F_OK   0
#  define X_OK   1
#  define W_OK   2
#  define R_OK   4

/* MSVC says these POSIX names are deprecated. */
#  define access	_access
#  define close		_close
#  define dup		_dup
#  define dup2		_dup2
#  define fdopen	_fdopen
#  define fileno	_fileno
#  define getcwd	_getcwd
#  define getpid	_getpid
#  define lseek		_lseek
#  define open		_open
#  define putenv	_putenv
#  define read		_read
#  define strdup	_strdup
#  define unlink	_unlink
#  define write		_write

/* Non-standard string function names. */
#  define strcasecmp    _stricmp
#  define strncasecmp   _strnicmp
#  define strtok_r      strtok_s

/* MSVC has no POSIX ssize_t. */
typedef SSIZE_T ssize_t;

/* MSVC has no gettimeofday(). We define it in w3misc.c. */
int gettimeofday(struct timeval *, void *);

/* MSVC has no getopt(). We define it in w3misc.c. */
int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind, opterr, optopt;

# endif /*]*/

#endif /*]*/
