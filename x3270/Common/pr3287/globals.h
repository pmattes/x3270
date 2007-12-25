/*              
 * Copyright 2000, 2004, 2007 by Paul Mattes.
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

#include <stdio.h>			/* Unix standard I/O library */
#include <stdlib.h>			/* Other Unix library functions */
#include <unistd.h>			/* Unix system calls */
#include <ctype.h>			/* Character classes */
#include <string.h>			/* String manipulations */
#include <sys/types.h>			/* Basic system data types */
#include <sys/time.h>			/* System time-related data types */
#include <time.h>			/* C library time functions */

#include "localdefs.h"

#define CN	(char *)NULL

extern unsigned long cgcsgid;
extern unsigned long cgcsgid_dbcs;
extern int dbcs;

#define Replace(var, value) { Free(var); var = (value); }
