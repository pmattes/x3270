/*      
 * Copyright 2000, 2002, 2007 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */     

/*     
 *	printerc.h
 *		Printer session support
 */             

extern void printer_init(void);
extern void printer_lu_dialog(void);
extern void printer_start(const char *lu);
extern void printer_stop(void);
extern Boolean printer_running(void);
#if defined(_WIN32) /*[*/
extern void printer_check(void);
#endif /*]*/
