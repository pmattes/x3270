/*
 * Copyright (c) 1996-2009, 2014 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ftc.h
 *		Global declarations for ft.c.
 */

extern Boolean ascii_flag;
extern Boolean cr_flag;
extern unsigned long ft_length;
extern FILE *ft_local_file;
extern char *ft_local_filename;
enum ft_state {
	FT_NONE,	/* No transfer in progress */
	FT_AWAIT_ACK,	/* IND$FILE sent, awaiting acknowledgement message */
	FT_RUNNING,	/* Ack received, data flowing */
	FT_ABORT_WAIT,	/* Awaiting chance to send an abort */
	FT_ABORT_SENT	/* Abort sent; awaiting response */
	};
extern Boolean ft_last_cr;
extern enum ft_state ft_state;
extern Boolean remap_flag;
extern unsigned char i_ft2asc[], i_asc2ft[];

#if defined(X3270_DBCS) /*[*/
enum ftd {
    FT_DBCS_NONE,
    FT_DBCS_SO,
    FT_DBCS_LEFT
};
extern enum ftd ft_dbcs_state;
extern unsigned char ft_dbcs_byte1;
extern Boolean ft_last_dbcs;
#endif /*]*/

extern void ft_aborting(void);
extern void ft_complete(const char *errmsg);
extern void ft_init(void);
extern void ft_running(Boolean is_cut);
extern void ft_update_length(void);
#if defined(X3270_DISPLAY) /*[*/
extern void popup_ft(Widget w, XtPointer call_parms, XtPointer call_data);
#endif /*]*/
extern action_t Transfer_action;
extern char *ft_local_fflag(void);

# if defined(_WIN32) /*[*/
extern int ft_ebcdic_to_multibyte(ebc_t ebc, char mb[], int mb_len);
extern int ft_unicode_to_multibyte(ucs4_t ucs4, char *mb, size_t mb_len);
extern ucs4_t ft_multibyte_to_unicode(const char *mb, size_t mb_len,
	int *consumedp, enum me_fail *errorp);
# else /*][*/
#  define ft_ebcdic_to_multibyte(ebc, mb, mb_len) \
	     ebcdic_to_multibyte(ebc, mb, mb_len)
#  define ft_unicode_to_multibyte(ucs4, mb, mb_len) \
	     unicode_to_multibyte(ucs4, mb, mb_len)
#  define ft_multibyte_to_unicode(mb, mb_len, consumedp, errorp) \
	     multibyte_to_unicode(mb, mb_len, consumedp, errorp)
# endif /*]*/
