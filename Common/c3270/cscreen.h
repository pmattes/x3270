/*
 * Copyright (c) 1999-2022 Paul Mattes.
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
 * 	cscreen.h
 *		c3270/wc3270 screen declarations.
 */

extern bool screen_initted;
extern bool escaped;
#if defined(_WIN32) /*[*/
extern int windows_cp;
extern HWND console_window;
#endif /*]*/

void screen_resume(void);
FILE *start_pager(void);
void screen_register(void);
void screen_final(void);
void screen_system_fixup(void);
#if defined(_WIN32) /*[*/
typedef enum {
    PC_DEFAULT,
    PC_PROMPT,
    PC_ERROR,
    PC_NORMAL
} pc_t;
void screen_color(pc_t sc);
bool screen_wait_for_key(char *c);
void screen_title(const char *text);
typedef void (*ctrlc_fn_t)(void);
void screen_set_ctrlc_fn(ctrlc_fn_t fn);
void get_console_size(int *rows, int *cols);
void screen_send_esc(void);
void screen_echo_mode(bool echo);
#endif /*]*/
#if !defined(_WIN32) /*[*/
bool screen_has_ansi_color(void);
const char *screen_op(void);
typedef enum {
    ACOLOR_BLUE,	/* 34 */
    ACOLOR_RED,		/* 31 */
    ACOLOR_YELLOW	/* 33 */
} acolor_t;
const char *screen_setaf(acolor_t color);
#endif /*]*/
