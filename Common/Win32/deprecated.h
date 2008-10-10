/*
 * Copyright 2008 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 * Redefinitions of POSIX functions that MSC doesn't like the names of.
 */

#define close		_close
#define fdopen		_fdopen
#define fileno		_fileno
#define getcwd		_getcwd
#define open		_open
#define putenv		_putenv
#define snprintf	_snprintf
#define unlink		_unlink
