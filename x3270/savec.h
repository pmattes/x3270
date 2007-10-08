/*
 * Copyright 1995, 1999, 2001, 2005 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	savec.h
 *		Global declarations for save.c.
 */

extern char *command_string;
extern char *profile_name;

extern void charset_list_changed(char *charset);
extern void merge_profile(XrmDatabase *d, Boolean mono);
extern void save_args(int argc, char *argv[]);
extern void save_init(int argc, char *hostname, char *port);
extern int save_options(char *n);
extern void save_yourself(void);
