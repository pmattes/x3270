/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *	nvt.c
 *		NVT (ANSI X3.64 / DEC VT100 / xterm) terminal emulation.
 */

#include "globals.h"

#include "appres.h"
#include "ctlr.h"
#include "3270ds.h"
#include "toggles.h"

#include "codepage.h"
#include "ctlrc.h"
#include "host.h"
#include "nvt.h"
#include "nvt_gui.h"
#include "screen.h"
#include "scroll.h"
#include "tables.h"
#include "task.h"
#include "telnet.h"
#include "telnet_core.h"
#include "trace.h"
#include "screentrace.h"
#include "unicodec.h"
#include "utils.h"

#define MB_MAX	16

#define PE_MAX	1024

#define	SC	1	/* save cursor position */
#define RC	2	/* restore cursor position */
#define NL	3	/* new line */
#define UP	4	/* cursor up */
#define	E2	5	/* second level of ESC processing */
#define rS	6	/* reset */
#define IC	7	/* insert chars */
#define DN	8	/* cursor down */
#define RT	9	/* cursor right */
#define LT	10	/* cursor left */
#define CM	11	/* cursor motion */
#define ED	12	/* erase in display */
#define EL	13	/* erase in line */
#define IL	14	/* insert lines */
#define DL	15	/* delete lines */
#define DC	16	/* delete characters */
#define	SG	17	/* set graphic rendition */
#define BL	18	/* ring bell */
#define NP	19	/* new page */
#define BS	20	/* backspace */
#define CR	21	/* carriage return */
#define LF	22	/* line feed */
#define HT	23	/* horizontal tab */
#define E1	24	/* first level of ESC processing */
#define Xx	25	/* undefined control character (nop) */
#define Pc	26	/* printing character */
#define Sc	27	/* semicolon (after ESC [) */
#define Dg	28	/* digit (after ESC [ or ESC [ ?) */
#define RI	29	/* reverse index */
#define DA	30	/* send device attributes */
#define SM	31	/* set mode */
#define RM	32	/* reset mode */
#define DO	33	/* return terminal ID (obsolete) */
#define SR	34	/* device status report */
#define CS	35	/* character set designate */
#define E3	36	/* third level of ESC processing */
#define DS	37	/* DEC private set */
#define DR	38	/* DEC private reset */
#define DV	39	/* DEC private save */
#define DT	40	/* DEC private restore */
#define SS	41	/* set scrolling region */
#define TM	42	/* text mode (ESC ]) */
#define T2	43	/* semicolon (after ESC ]) */
#define TX	44	/* text parameter (after ESC ] n ;) */
#define TB	45	/* text parameter done (ESC ] n ; xxx BEL) */
#define TS	46	/* tab set */
#define TC	47	/* tab clear */
#define C2	48	/* character set designate (finish) */
#define G0	49	/* select G0 character set */
#define G1	50	/* select G1 character set */
#define G2	51	/* select G2 character set */
#define G3	52	/* select G3 character set */
#define S2	53	/* select G2 for next character */
#define S3	54	/* select G3 for next character */
#define MB	55	/* process multi-byte character */
#define CH	56	/* cursor horizontal absolute (CHA) */
#define VP	57	/* vertical position absolute (VPA) */
#define GT	58	/* > (after ESC [) */
#define D2	59	/* secondary device attributes */

static enum state {
    DATA = 0, ESC = 1, CSDES = 2,
    N1 = 3, DECP = 4, TEXT = 5, TEXT2 = 6,
    MBPEND = 7, ESCGT = 8, NUM_STATES = 9
} state = DATA;

/*
 * Terminal functions for ANSI X3.64 are called ansi_xxx.
 * DEC VT100-specific functions are called dec_xxx.
 * Xterm-specific functions are called xterm_xxx.
 */
static enum state ansi_data_mode(int, int);
static enum state dec_save_cursor(int, int);
static enum state dec_restore_cursor(int, int);
static enum state ansi_newline(int, int);
static enum state ansi_cursor_up(int, int);
static enum state ansi_esc2(int, int);
static enum state ansi_reset(int, int);
static enum state ansi_insert_chars(int, int);
static enum state ansi_cursor_down(int, int);
static enum state ansi_cursor_right(int, int);
static enum state ansi_cursor_left(int, int);
static enum state ansi_cursor_motion(int, int);
static enum state ansi_erase_in_display(int, int);
static enum state ansi_erase_in_line(int, int);
static enum state ansi_insert_lines(int, int);
static enum state ansi_delete_lines(int, int);
static enum state ansi_delete_chars(int, int);
static enum state ansi_sgr(int, int);
static enum state ansi_bell(int, int);
static enum state ansi_newpage(int, int);
static enum state ansi_backspace(int, int);
static enum state ansi_cr(int, int);
static enum state ansi_lf(int, int);
static enum state ansi_htab(int, int);
static enum state ansi_escape(int, int);
static enum state ansi_nop(int, int);
static enum state ansi_printing(int, int);
static enum state ansi_semicolon(int, int);
static enum state ansi_digit(int, int);
static enum state ansi_reverse_index(int, int);
static enum state ansi_send_attributes(int, int);
static enum state ansi_set_mode(int, int);
static enum state ansi_reset_mode(int, int);
static enum state dec_return_terminal_id(int, int);
static enum state ansi_status_report(int, int);
static enum state ansi_cs_designate(int, int);
static enum state ansi_esc3(int, int);
static enum state dec_set(int, int);
static enum state dec_reset(int, int);
static enum state dec_save(int, int);
static enum state dec_restore(int, int);
static enum state dec_scrolling_region(int, int);
static enum state xterm_text_mode(int, int);
static enum state xterm_text_semicolon(int, int);
static enum state xterm_text(int, int);
static enum state xterm_text_do(int, int);
static enum state ansi_htab_set(int, int);
static enum state ansi_htab_clear(int, int);
static enum state ansi_cs_designate2(int, int);
static enum state ansi_select_g0(int, int);
static enum state ansi_select_g1(int, int);
static enum state ansi_select_g2(int, int);
static enum state ansi_select_g3(int, int);
static enum state ansi_one_g2(int, int);
static enum state ansi_one_g3(int, int);
static enum state ansi_multibyte(int, int);
static enum state ansi_cursor_horizontal_absolute(int, int);
static enum state ansi_vertical_position_absolute(int, int);
static enum state ansi_gt(int, int);
static enum state dec_secondary_device_attributes(int, int);

typedef enum state (*afn_t)(int, int);
static afn_t nvt_fn[] = {
/* 0 */		&ansi_data_mode,
/* 1 */		&dec_save_cursor,
/* 2 */		&dec_restore_cursor,
/* 3 */		&ansi_newline,
/* 4 */		&ansi_cursor_up,
/* 5 */		&ansi_esc2,
/* 6 */		&ansi_reset,
/* 7 */		&ansi_insert_chars,
/* 8 */		&ansi_cursor_down,
/* 9 */		&ansi_cursor_right,
/* 10 */	&ansi_cursor_left,
/* 11 */	&ansi_cursor_motion,
/* 12 */	&ansi_erase_in_display,
/* 13 */	&ansi_erase_in_line,
/* 14 */	&ansi_insert_lines,
/* 15 */	&ansi_delete_lines,
/* 16 */	&ansi_delete_chars,
/* 17 */	&ansi_sgr,
/* 18 */	&ansi_bell,
/* 19 */	&ansi_newpage,
/* 20 */	&ansi_backspace,
/* 21 */	&ansi_cr,
/* 22 */	&ansi_lf,
/* 23 */	&ansi_htab,
/* 24 */	&ansi_escape,
/* 25 */	&ansi_nop,
/* 26 */	&ansi_printing,
/* 27 */	&ansi_semicolon,
/* 28 */	&ansi_digit,
/* 29 */	&ansi_reverse_index,
/* 30 */	&ansi_send_attributes,
/* 31 */	&ansi_set_mode,
/* 32 */	&ansi_reset_mode,
/* 33 */	&dec_return_terminal_id,
/* 34 */	&ansi_status_report,
/* 35 */	&ansi_cs_designate,
/* 36 */	&ansi_esc3,
/* 37 */	&dec_set,
/* 38 */	&dec_reset,
/* 39 */	&dec_save,
/* 40 */	&dec_restore,
/* 41 */	&dec_scrolling_region,
/* 42 */	&xterm_text_mode,
/* 43 */	&xterm_text_semicolon,
/* 44 */	&xterm_text,
/* 45 */	&xterm_text_do,
/* 46 */	&ansi_htab_set,
/* 47 */	&ansi_htab_clear,
/* 48 */	&ansi_cs_designate2,
/* 49 */	&ansi_select_g0,
/* 50 */	&ansi_select_g1,
/* 51 */	&ansi_select_g2,
/* 52 */	&ansi_select_g3,
/* 53 */	&ansi_one_g2,
/* 54 */	&ansi_one_g3,
/* 55 */	&ansi_multibyte,
/* 56 */	&ansi_cursor_horizontal_absolute,
/* 57 */	&ansi_vertical_position_absolute,
/* 58 */	&ansi_gt,
/* 59 */	&dec_secondary_device_attributes,
};

static unsigned char st[NUM_STATES][256] = {
/*
 * State table for base processing (state == DATA)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,BL,BS,HT,LF,LF,NP,CR,G1,G0,
/* 10 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,E1,Xx,Xx,Xx,Xx,
/* 20 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 30 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 40 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 50 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 60 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 70 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Xx,
/* 80 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* 90 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* a0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* b0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* c0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* d0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* e0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* f0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc
},

/*
 * State table for ESC processing (state == ESC)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0,CS,CS,CS,CS, 0, 0, 0, 0,
/* 30 */	0, 0, 0, 0, 0, 0, 0,SC,RC, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0,NL, 0, 0,TS, 0, 0, 0, 0,RI,S2,S3,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,E2, 0,TM, 0, 0,
/* 60 */	0, 0, 0,rS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,G2,G3,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ()*+ C processing (state == CSDES)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       C2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0,C2,C2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ processing (state == N1)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,Sc, 0, 0,GT,E3,
/* 40 */       IC,UP,DN,RT,LT, 0, 0,CH,CM, 0,ED,EL,IL,DL, 0, 0,
/* 50 */       DC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0,DA,VP, 0,CM,TC,SM, 0, 0, 0,RM,SG,SR, 0,
/* 70 */	0, 0,SS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ ? processing (state == DECP)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0,DS, 0, 0, 0,DR, 0, 0, 0,
/* 70 */	0, 0,DT,DV, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] processing (state == TEXT)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,T2, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] n ; processing (state == TEXT2)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */        0, 0, 0, 0, 0, 0, 0,TB, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 30 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 40 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 50 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 60 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 70 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,Xx,
/* 80 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 90 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* a0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* b0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* c0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* d0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* e0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* f0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX
},
/*
 * State table for multi-byte characters (state == MBPEND)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 10 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 20 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 30 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 40 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 50 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 60 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 70 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 80 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 90 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* a0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* b0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* c0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* d0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* e0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* f0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB
},

/*
 * State table for ESC > processing (state == ESCGT)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0,D2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},
};

/* Character sets. */
#define CS_G0		0
#define CS_G1		1
#define CS_G2		2
#define CS_G3		3

/* Character set designations. */
#define CSD_LD		0
#define CSD_UK		1
#define CSD_US		2

static int      saved_cursor = 0;
#define NN	20
static int      n[NN], nx = 0;
#define NT	256
static char     text[NT + 1];
static int      tx = 0;
static int      nvt_ch;
static unsigned char gr = 0;
static unsigned char saved_gr = 0;
static unsigned char fg = 0;
static unsigned char saved_fg = 0;
static unsigned char bg = 0;
static unsigned char saved_bg = 0;
static int	cset = CS_G0;
static int	saved_cset = CS_G0;
static int	csd[4] = { CSD_US, CSD_US, CSD_US, CSD_US };
static int	saved_csd[4] = { CSD_US, CSD_US, CSD_US, CSD_US };
static int	once_cset = -1;
static int      insert_mode = 0;
static int      auto_newline_mode = 0;
static int      appl_cursor = 0;
static int      saved_appl_cursor = 0;
static int      wraparound_mode = 1;
static int      saved_wraparound_mode = 1;
static bool     rev_wraparound_mode = false;
static bool     saved_rev_wraparound_mode = false;
static int	allow_wide_mode = 0;
static int	saved_allow_wide_mode = 0;
static int	wide_mode = 0;
static int	saved_wide_mode = 0;
static bool  saved_altbuffer = false;
static int      scroll_top = -1;
static int      scroll_bottom = -1;
static unsigned char *tabs = (unsigned char *) NULL;
static char	gnnames[] = "()*+";
static char	csnames[] = "0AB";
static int	cs_to_change;
static int	pmi = 0;
static char	pending_mbs[MB_MAX];
static int	pe = 0;
static unsigned char ped[PE_MAX];
static bool	cursor_enabled = false;

static bool  held_wrap = false;

static void nvt_scroll(void);

static enum state
ansi_data_mode(int ig1 _is_unused, int ig2 _is_unused)
{
    return DATA;
}

static enum state
dec_save_cursor(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    saved_cursor = cursor_addr;
    saved_cset = cset;
    for (i = 0; i < 4; i++) {
	saved_csd[i] = csd[i];
    }
    saved_fg = fg;
    saved_bg = bg;
    saved_gr = gr;
    return DATA;
}

static enum state
dec_restore_cursor(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    cset = saved_cset;
    for (i = 0; i < 4; i++) {
	csd[i] = saved_csd[i];
    }
    fg = saved_fg;
    bg = saved_bg;
    gr = saved_gr;
    cursor_move(saved_cursor);
    held_wrap = false;
    return DATA;
}

static enum state
ansi_newline(int ig1 _is_unused, int ig2 _is_unused)
{
    int nc;

    cursor_move(cursor_addr - (cursor_addr % COLS));
    nc = cursor_addr + COLS;
    if (nc < scroll_bottom * COLS) {
	cursor_move(nc);
    } else {
	nvt_scroll();
    }
    held_wrap = false;
    return DATA;
}

static enum state
ansi_cursor_up(int nn, int ig2 _is_unused)
{
    int rr;

    if (nn < 1) {
	nn = 1;
    }
    rr = cursor_addr / COLS;
    if (rr - nn < 0) {
	cursor_move(cursor_addr % COLS);
    } else {
	cursor_move(cursor_addr - (nn * COLS));
    }
    held_wrap = false;
    return DATA;
}

static enum state
ansi_esc2(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i < NN; i++) {
	n[i] = 0;
    }
    nx = 0;
    return N1;
}

static enum state
ansi_reset(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;
    static bool first = true;

    gr = 0;
    saved_gr = 0;
    fg = 0;
    saved_fg = 0;
    bg = 0;
    saved_bg = 0;
    cset = CS_G0;
    saved_cset = CS_G0;
    csd[0] = csd[1] = csd[2] = csd[3] = CSD_US;
    saved_csd[0] = saved_csd[1] = saved_csd[2] = saved_csd[3] = CSD_US;
    once_cset = -1;
    saved_cursor = 0;
    cursor_enabled = true;
    insert_mode = 0;
    auto_newline_mode = 0;
    appl_cursor = 0;
    saved_appl_cursor = 0;
    wraparound_mode = 1;
    saved_wraparound_mode = 1;
    rev_wraparound_mode = false;
    saved_rev_wraparound_mode = false;
    allow_wide_mode = 0;
    saved_allow_wide_mode = 0;
    wide_mode = 0;
    allow_wide_mode = 0;
    saved_altbuffer = false;
    scroll_top = 1;
    scroll_bottom = ROWS;
    Replace(tabs, (unsigned char *)Malloc((COLS+7)/8));
    for (i = 0; i < (COLS+7)/8; i++) {
	tabs[i] = 0x01;
    }
    held_wrap = false;
    if (!first) {
	ctlr_altbuffer(true);
	ctlr_aclear(0, ROWS * COLS, 1);
	ctlr_altbuffer(false);
	ctlr_clear(false);
	screen_80();
	ctlr_enable_cursor(true, EC_NVT);
    }
    first = false;
    pmi = 0;
    return DATA;
}

static enum state
ansi_insert_chars(int nn, int ig2 _is_unused)
{
    int cc = cursor_addr % COLS;	/* current col */
    int mc = COLS - cc;			/* max chars that can be inserted */
    int ns;				/* chars that are shifting */

    if (nn < 1) {
	nn = 1;
    }
    if (nn > mc) {
	nn = mc;
    }

    /* Move the surviving chars right */
    ns = mc - nn;
    if (ns) {
	ctlr_bcopy(cursor_addr, cursor_addr + nn, ns, 1);
    }

    /* Clear the middle of the line */
    ctlr_aclear(cursor_addr, nn, 1);
    return DATA;
}

static enum state
ansi_cursor_down(int nn, int ig2 _is_unused)
{
    int rr;

    if (nn < 1) {
	nn = 1;
    }
    rr = cursor_addr / COLS;
    if (rr + nn >= ROWS) {
	cursor_move((ROWS-1)*COLS + (cursor_addr%COLS));
    } else {
	cursor_move(cursor_addr + (nn * COLS));
    }
    held_wrap = false;
    return DATA;
}

static enum state
ansi_cursor_right(int nn, int ig2 _is_unused)
{
    int cc;

    if (nn < 1) {
	nn = 1;
    }
    cc = cursor_addr % COLS;
    if (cc == COLS-1) {
	return DATA;
    }
    if (cc + nn >= COLS) {
	nn = COLS - 1 - cc;
    }
    cursor_move(cursor_addr + nn);
    held_wrap = false;
    return DATA;
}

static enum state
ansi_cursor_left(int nn, int ig2 _is_unused)
{
    int cc;

    if (held_wrap) {
	held_wrap = false;
	return DATA;
    }
    if (nn < 1) {
	nn = 1;
    }
    cc = cursor_addr % COLS;
    if (!cc) {
	return DATA;
    }
    if (nn > cc) {
	nn = cc;
    }
    cursor_move(cursor_addr - nn);
    return DATA;
}

static enum state
ansi_cursor_motion(int n1, int n2)
{
    if (n1 < 1) {
	n1 = 1;
    }
    if (n1 > ROWS) {
	n1 = ROWS;
    }
    if (n2 < 1) {
	n2 = 1;
    }
    if (n2 > COLS) {
	n2 = COLS;
    }
    cursor_move((n1 - 1) * COLS + (n2 - 1));
    held_wrap = false;
    return DATA;
}

static enum state
ansi_cursor_horizontal_absolute(int n1, int n2 _is_unused)
{
    if (n1 < 1) {
	n1 = 1;
    }
    if (n1 > COLS) {
	n1 = COLS;
    }
    cursor_move((cursor_addr / COLS) * COLS + (n1 - 1));
    held_wrap = false;
    return DATA;
}

static enum state
ansi_vertical_position_absolute(int n1, int n2 _is_unused)
{
    if (n1 < 1) {
	n1 = 1;
    }
    if (n1 > ROWS) {
	n1 = ROWS;
    }
    cursor_move(((n1 - 1) * COLS) + (cursor_addr % COLS));
    held_wrap = false;
    return DATA;
}

static enum state
ansi_erase_in_display(int nn, int ig2 _is_unused)
{
    switch (nn) {
    case 0:	/* below */
	ctlr_aclear(cursor_addr, (ROWS * COLS) - cursor_addr, 1);
	break;
    case 1:	/* above */
	ctlr_aclear(0, cursor_addr + 1, 1);
	break;
    case 2:	/* all (without moving cursor) */
	if (cursor_addr == 0 && !is_altbuffer) {
	    scroll_save(ROWS);
	}
	ctlr_aclear(0, ROWS * COLS, 1);
	break;
    }
    return DATA;
}

static enum state
ansi_erase_in_line(int nn, int ig2 _is_unused)
{
    int nc = cursor_addr % COLS;

    switch (nn) {
    case 0:	/* to right */
	ctlr_aclear(cursor_addr, COLS - nc, 1);
	break;
    case 1:	/* to left */
	ctlr_aclear(cursor_addr - nc, nc+1, 1);
	break;
    case 2:	/* all */
	ctlr_aclear(cursor_addr - nc, COLS, 1);
	break;
    }
    return DATA;
}

static enum state
ansi_insert_lines(int nn, int ig2 _is_unused)
{
    int rr = cursor_addr / COLS;	/* current row */
    int mr = scroll_bottom - rr;	/* rows left at and below this one */
    int ns;				/* rows that are shifting */

    /* If outside of the scrolling region, do nothing */
    if (rr < scroll_top - 1 || rr >= scroll_bottom) {
	return DATA;
    }

    if (nn < 1) {
	nn = 1;
    }
    if (nn > mr) {
	nn = mr;
    }

    /* Move the victims down */
    ns = mr - nn;
    if (ns) {
	ctlr_bcopy(rr * COLS, (rr + nn) * COLS, ns * COLS, 1);
    }

    /* Clear the middle of the screen */
    ctlr_aclear(rr * COLS, nn * COLS, 1);
    return DATA;
}

static enum state
ansi_delete_lines(int nn, int ig2 _is_unused)
{
    int rr = cursor_addr / COLS;	/* current row */
    int mr = scroll_bottom - rr;	/* max rows that can be deleted */
    int ns;				/* rows that are shifting */

    /* If outside of the scrolling region, do nothing */
    if (rr < scroll_top - 1 || rr >= scroll_bottom) {
	return DATA;
    }

    if (nn < 1) {
	nn = 1;
    }
    if (nn > mr) {
	nn = mr;
    }

    /* Move the surviving rows up */
    ns = mr - nn;
    if (ns) {
	ctlr_bcopy((rr + nn) * COLS, rr * COLS, ns * COLS, 1);
    }

    /* Clear the rest of the screen */
    ctlr_aclear((rr + ns) * COLS, nn * COLS, 1);
    return DATA;
}

static enum state
ansi_delete_chars(int nn, int ig2 _is_unused)
{
    int cc = cursor_addr % COLS;	/* current col */
    int mc = COLS - cc;			/* max chars that can be deleted */
    int ns;				/* chars that are shifting */

    if (nn < 1) {
	nn = 1;
    }
    if (nn > mc) {
	nn = mc;
    }

    /* Move the surviving chars left */
    ns = mc - nn;
    if (ns) {
	ctlr_bcopy(cursor_addr + nn, cursor_addr, ns, 1);
    }

    /* Clear the end of the line */
    ctlr_aclear(cursor_addr + ns, nn, 1);
    return DATA;
}

static enum state
ansi_sgr(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i <= nx && i < NN; i++) {
	switch (n[i]) {
	case 0:
	    gr = 0;
	    fg = 0;
	    bg = 0;
	    break;
	case 1:
	    gr |= GR_INTENSIFY;
	    break;
	case 4:
	    gr |= GR_UNDERLINE;
	    break;
	case 5:
	    gr |= GR_BLINK;
	    break;
	case 7:
	    gr |= GR_REVERSE;
	    break;
	case 30:
	    fg = 0xf0;	/* black -> neutral black */
	    break;
	case 31:
	    fg = 0xf2;	/* red -> red */
	    break;
	case 32:
	    fg = 0xf4;	/* green -> green */
	    break;
	case 33:
	    fg = 0xf6;	/* yellow -> yellow */
	    break;
	case 34:
	    fg = 0xf1;	/* blue -> blue */
	    break;
	case 35:
	    fg = 0xf3;	/* magenta -> pink */
	    break;
	case 36:
	    fg = 0xf5;	/* cyan -> turquiose */
	    break;
	case 37:
	    fg = 0xf7;	/* white -> neutral white */
	    break;
	case 39:
	    fg = 0;	/* default */
	    break;
	case 40:
	    bg = 0xf0;	/* black -> neutral black */
	    break;
	case 41:
	    bg = 0xf2;	/* red -> red */
	    break;
	case 42:
	    bg = 0xf4;	/* green -> green */
	    break;
	case 43:
	    bg = 0xf6;	/* yellow -> yellow */
	    break;
	case 44:
	    bg = 0xf1;	/* blue -> blue */
	    break;
	case 45:
	    bg = 0xf3;	/* magenta -> pink */
	    break;
	case 46:
	    bg = 0xf5;	/* cyan -> turquoise */
	    break;
	case 47:
	    bg = 0xf7;	/* white -> neutral white */
	    break;
	case 49:
	    bg = 0;	/* default */
	    break;
	}
    }

    return DATA;
}

static enum state
ansi_bell(int ig1 _is_unused, int ig2 _is_unused)
{
    ring_bell();
    return DATA;
}

static enum state
ansi_newpage(int ig1 _is_unused, int ig2 _is_unused)
{
    ctlr_clear(false);
    return DATA;
}

static enum state
ansi_backspace(int ig1 _is_unused, int ig2 _is_unused)
{
    if (held_wrap) {
	held_wrap = false;
	return DATA;
    }
    if (rev_wraparound_mode) {
	if (cursor_addr > (scroll_top - 1) * COLS) {
	    cursor_move(cursor_addr - 1);
	}
    } else {
	if (cursor_addr % COLS) {
	    cursor_move(cursor_addr - 1);
	}
    }
    return DATA;
}

static enum state
ansi_cr(int ig1 _is_unused, int ig2 _is_unused)
{
    if (cursor_addr % COLS) {
	cursor_move(cursor_addr - (cursor_addr % COLS));
    }
    if (auto_newline_mode) {
	ansi_lf(0, 0);
    }
    held_wrap = false;
    return DATA;
}

static enum state
ansi_lf(int ig1 _is_unused, int ig2 _is_unused)
{
    int nc = cursor_addr + COLS;

    held_wrap = false;

    /* If we're below the scrolling region, don't scroll. */
    if ((cursor_addr / COLS) >= scroll_bottom) {
	if (nc < ROWS * COLS) {
	    cursor_move(nc);
	}
	return DATA;
    }

    if (nc < scroll_bottom * COLS) {
	cursor_move(nc);
    } else {
	nvt_scroll();
    }
    return DATA;
}

static enum state
ansi_htab(int ig1 _is_unused, int ig2 _is_unused)
{
    int col = cursor_addr % COLS;
    int i;

    held_wrap = false;
    if (col == COLS - 1) {
	return DATA;
    }
    for (i = col + 1; i < COLS - 1; i++) {
	if (tabs[i / 8] & 1 << (i % 8)) {
	    break;
	}
    }
    cursor_move(cursor_addr - col + i);
    return DATA;
}

static enum state
ansi_escape(int ig1 _is_unused, int ig2 _is_unused)
{
    return ESC;
}

static enum state
ansi_nop(int ig1 _is_unused, int ig2 _is_unused)
{
    return DATA;
}

#define PWRAP { \
    if ((cursor_addr % COLS) == COLS - 1) { \
	ctlr_add_gr(cursor_addr, ea_buf[cursor_addr].gr | GR_WRAP); \
    } \
    nc = cursor_addr + 1; \
    if (nc < scroll_bottom * COLS) { \
	cursor_move(nc); \
    } else { \
	if (cursor_addr / COLS >= scroll_bottom) { \
	    cursor_move(cursor_addr / COLS * COLS); \
	} else { \
	    nvt_scroll(); \
	    cursor_move(nc - COLS); \
	} \
    } \
}

static enum state
ansi_printing(int ig1 _is_unused, int ig2 _is_unused)
{
    int nc;
    enum dbcs_state d;
    int xcset;

    if ((pmi == 0) && (nvt_ch & 0x80)) {
	char mbs[2];
	int consumed;
	enum me_fail fail;
	unsigned long ucs4;

	mbs[0] = (char)nvt_ch;
	mbs[1] = '\0';
	ucs4 = multibyte_to_unicode(mbs, 1, &consumed, &fail);
	if (ucs4 == 0) {
	    switch (fail) {
	    case ME_SHORT:
		/* Start munching multi-byte. */
		pmi = 0;
		pending_mbs[pmi++] = (char)nvt_ch;
		return MBPEND;
	    case ME_INVALID:
	    default:
		/* Invalid multi-byte -> '?' */
		nvt_ch = '?';
		break;
	    }
	} else {
	    nvt_ch = ucs4;
	}
    }
    pmi = 0;

    if (held_wrap) {
	PWRAP;
	held_wrap = false;
    }

    if (insert_mode) {
	ansi_insert_chars(1, 0);
    }
    d = ctlr_dbcs_state(cursor_addr);
    xcset = csd[(once_cset != -1) ? once_cset : cset];
    if (xcset == CSD_LD && nvt_ch >= 0x5f && nvt_ch <= 0x7e) {
	ctlr_add_nvt(cursor_addr, (unsigned char)(nvt_ch - 0x5f),
		CS_LINEDRAW);
    } else if (xcset == CSD_UK && nvt_ch == '#') {
	ctlr_add_nvt(cursor_addr, 0x1e, CS_LINEDRAW);
    } else {
	if (IS_UNICODE_DBCS(nvt_ch)) {
	    /* Get past the last column. */
	    if ((cursor_addr % COLS) == (COLS - 1)) {
		if (!wraparound_mode) {
		    return DATA;
		}
		ctlr_add_nvt(cursor_addr, ' ', CS_BASE);
		ctlr_add_gr(cursor_addr, gr);
		ctlr_add_fg(cursor_addr, fg);
		ctlr_add_bg(cursor_addr, bg);
		cursor_addr = cursor_addr + 1;
		d = ctlr_dbcs_state(cursor_addr);
	    }

	    /* Add the left half. */
	    ctlr_add_nvt(cursor_addr, nvt_ch, CS_DBCS);
	    ctlr_add_gr(cursor_addr, gr);
	    ctlr_add_fg(cursor_addr, fg);
	    ctlr_add_bg(cursor_addr, bg);

	    /* Handle unaligned DBCS overwrite. */
	    if (d == DBCS_RIGHT || d == DBCS_RIGHT_WRAP) {
		int xaddr;

		xaddr = cursor_addr;
		DEC_BA(xaddr);
		ctlr_add_nvt(xaddr, ' ', CS_BASE);
		ea_buf[xaddr].db = DBCS_NONE;
	    }

	    /* Add the right half. */
	    INC_BA(cursor_addr);
	    ctlr_add_nvt(cursor_addr, ' ', CS_DBCS);
	    ctlr_add_gr(cursor_addr, gr);
	    ctlr_add_fg(cursor_addr, fg);
	    ctlr_add_bg(cursor_addr, bg);

	    /* Handle cursor wrap. */
	    if (wraparound_mode) {
		if (!((cursor_addr + 1) % COLS)) {
		    held_wrap = true;
		} else {
		    PWRAP;
		}
	    } else {
		if ((cursor_addr % COLS) != (COLS - 1)) {
		    cursor_move(cursor_addr + 1);
		}
	    }
	    ctlr_dbcs_postprocess();
	    return DATA;
	} else {
	    /* Add an SBCS character to the buffer. */
	    ctlr_add_nvt(cursor_addr, nvt_ch, CS_BASE);
	}
    }

    /* Handle conflicts with existing DBCS characters. */
    if (d == DBCS_RIGHT || d == DBCS_RIGHT_WRAP) {
	int xaddr;

	xaddr = cursor_addr;
	DEC_BA(xaddr);
	ctlr_add_nvt(xaddr, ' ', CS_BASE);
	ea_buf[xaddr].db = DBCS_NONE;
	ea_buf[cursor_addr].db = DBCS_NONE;
	ctlr_dbcs_postprocess();
    }

    if (d == DBCS_LEFT || d == DBCS_LEFT_WRAP) {
	int xaddr;

	xaddr = cursor_addr;
	INC_BA(xaddr);
	ctlr_add_nvt(xaddr, ' ', CS_BASE);
	ea_buf[xaddr].db = DBCS_NONE;
	ea_buf[cursor_addr].db = DBCS_NONE;
	ctlr_dbcs_postprocess();
    }

    once_cset = -1;
    ctlr_add_gr(cursor_addr, gr);
    ctlr_add_fg(cursor_addr, fg);
    ctlr_add_bg(cursor_addr, bg);
    if (wraparound_mode) {
	/*
	 * There is a fascinating behavior of xterm which we will
	 * attempt to emulate here.  When a character is printed in the
	 * last column, the cursor sticks there, rather than wrapping
	 * to the next line.  Another printing character will put the
	 * cursor in column 2 of the next line.  One cursor-left
	 * sequence won't budge it; two will.  Saving and restoring
	 * the cursor won't move the cursor, but will cancel all of
	 * the above behaviors...
	 *
	 * In my opinion, very strange, but among other things, 'vi'
	 * depends on it!
	 */
	if (!((cursor_addr + 1) % COLS)) {
	    held_wrap = true;
	} else {
	    PWRAP;
	}
    } else {
	if ((cursor_addr % COLS) != (COLS - 1)) {
	    cursor_move(cursor_addr + 1);
	}
    }
    return DATA;
}

static enum state
ansi_multibyte(int ig1, int ig2)
{
    unsigned long ucs4;
    int consumed;
    enum me_fail fail;
    afn_t fn;

    if (pmi >= MB_MAX - 2) {
	/* String too long. */
	pmi = 0;
	nvt_ch = '?';
	return ansi_printing(ig1, ig2);
    }

    pending_mbs[pmi++] = (char)nvt_ch;
    pending_mbs[pmi] = '\0';
    ucs4 = multibyte_to_unicode(pending_mbs, pmi, &consumed, &fail);
    if (ucs4 != 0) {
	/* Success! */
	nvt_ch = ucs4;
	return ansi_printing(ig1, ig2);
    }
    if (fail == ME_SHORT) {
	/* Go get more. */
	return MBPEND;
    }

    /* Failure. */

    /* Replace the sequence with '?'. */
    ucs4 = nvt_ch; /* save for later */
    pmi = 0;
    nvt_ch = '?';
    ansi_printing(ig1, ig2);

    /*
     * Reprocess whatever we choked on (especially if it's a control
     * character).
     */
    nvt_ch = ucs4;
    state = DATA;
    fn = nvt_fn[st[(int)DATA][nvt_ch]];
    return (*fn)(n[0], n[1]);
}

static enum state
ansi_semicolon(int ig1 _is_unused, int ig2 _is_unused)
{
    if (nx >= NN) {
	return DATA;
    }
    nx++;
    return state;
}

static enum state
ansi_digit(int ig1 _is_unused, int ig2 _is_unused)
{
    n[nx] = (n[nx] * 10) + (nvt_ch - '0');
    return state;
}

static enum state
ansi_reverse_index(int ig1 _is_unused, int ig2 _is_unused)
{
    int rr = cursor_addr / COLS;	/* current row */
    int np = (scroll_top - 1) - rr;	/* number of rows in the scrolling
				           region, above this line */
    int ns;				/* number of rows to scroll */
    int nn = 1;				/* number of rows to index */

    held_wrap = false;

    /* If the cursor is above the scrolling region, do a simple margined
       cursor up.  */
    if (np < 0) {
	ansi_cursor_up(nn, 0);
	return DATA;
    }

    /* Split the number of lines to scroll into ns */
    if (nn > np) {
	ns = nn - np;
	nn = np;
    } else {
	ns = 0;
    }

    /* Move the cursor up without scrolling */
    if (nn) {
	ansi_cursor_up(nn, 0);
    }

    /* Insert lines at the top for backward scroll */
    if (ns) {
	ansi_insert_lines(ns, 0);
    }

    return DATA;
}

static enum state
ansi_send_attributes(int nn, int ig2 _is_unused)
{
    if (!nn) {
	net_sends("\033[?1;2c");
    }
    return DATA;
}

static enum state
dec_return_terminal_id(int ig1 _is_unused, int ig2 _is_unused)
{
    return ansi_send_attributes(0, 0);
}

static enum state
dec_secondary_device_attributes(int ig1 _is_unused, int ig2 _is_unused)
{
    /* Don't respond. It can trigger all sorts of additional chatter. */
    /* net_sends("\033[>0;3270;0c"); */
    return DATA;
}

static enum state
ansi_set_mode(int nn, int ig2 _is_unused)
{
    switch (nn) {
    case 4:
	insert_mode = 1;
	break;
    case 20:
	auto_newline_mode = 1;
	break;
    }
    return DATA;
}

static enum state
ansi_reset_mode(int nn, int ig2 _is_unused)
{
    switch (nn) {
    case 4:
	insert_mode = 0;
	break;
    case 20:
	auto_newline_mode = 0;
	break;
    }
    return DATA;
}

static enum state
ansi_status_report(int nn, int ig2 _is_unused)
{
    char *s;

    switch (nn) {
    case 5:
	net_sends("\033[0n");
	break;
    case 6:
	s = Asprintf("\033[%d;%dR",
		(cursor_addr/COLS) + 1, (cursor_addr%COLS) + 1);
	net_sends(s);
	Free(s);
	break;
    }
    return DATA;
}

static enum state
ansi_cs_designate(int ig1 _is_unused, int ig2 _is_unused)
{
    cs_to_change = (int)(strchr(gnnames, nvt_ch) - gnnames);
    return CSDES;
}

static enum state
ansi_cs_designate2(int ig1 _is_unused, int ig2 _is_unused)
{
    csd[cs_to_change] = (int)(strchr(csnames, nvt_ch) - csnames);
    return DATA;
}

static enum state
ansi_select_g0(int ig1 _is_unused, int ig2 _is_unused)
{
    cset = CS_G0;
    return DATA;
}

static enum state
ansi_select_g1(int ig1 _is_unused, int ig2 _is_unused)
{
    cset = CS_G1;
    return DATA;
}

static enum state
ansi_select_g2(int ig1 _is_unused, int ig2 _is_unused)
{
    cset = CS_G2;
    return DATA;
}

static enum state
ansi_select_g3(int ig1 _is_unused, int ig2 _is_unused)
{
    cset = CS_G3;
    return DATA;
}

static enum state
ansi_one_g2(int ig1 _is_unused, int ig2 _is_unused)
{
    once_cset = CS_G2;
    return DATA;
}

static enum state
ansi_one_g3(int ig1 _is_unused, int ig2 _is_unused)
{
    once_cset = CS_G3;
    return DATA;
}

static enum state
ansi_esc3(int ig1 _is_unused, int ig2 _is_unused)
{
    return DECP;
}

static enum state
dec_set(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i <= nx && i < NN; i++)
	switch (n[i]) {
	case 1:	/* application cursor keys */
	    appl_cursor = 1;
	    break;
	case 2:	/* set G0-G3 */
	    csd[0] = csd[1] = csd[2] = csd[3] = CSD_US;
	    break;
	case 3:	/* 132-column mode */
	    if (allow_wide_mode) {
		wide_mode = 1;
		screen_132();
	    }
	    break;
	case 7:	/* wraparound mode */
	    wraparound_mode = 1;
	    break;
	case 25:	/* cursor */
	    cursor_enabled = true;
	    ctlr_enable_cursor(true, EC_NVT);
	    break;
	case 40:	/* allow 80/132 switching */
	    allow_wide_mode = 1;
	    break;
	case 45:	/* reverse-wraparound mode */
	    rev_wraparound_mode = true;
	    break;
	case 47:	/* alt buffer */
	case 1049:
	    dec_save_cursor(0, 0);
	    ctlr_altbuffer(true);
	    is_altbuffer = true;
	    ctlr_aclear(0, ROWS * COLS, 1);
	    break;
	}
    return DATA;
}

static enum state
dec_reset(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i <= nx && i < NN; i++) {
	switch (n[i]) {
	case 1:	/* normal cursor keys */
	    appl_cursor = 0;
	    break;
	case 3:	/* 132-column mode */
	    if (allow_wide_mode) {
		wide_mode = 0;
		screen_80();
	    }
	    break;
	case 7:	/* no wraparound mode */
	    wraparound_mode = 0;
	    break;
	case 25:	/* cursor */
	    cursor_enabled = false;
	    ctlr_enable_cursor(false, EC_NVT);
	    break;
	case 40:	/* allow 80/132 switching */
	    allow_wide_mode = 0;
	    break;
	case 45:	/* no reverse-wraparound mode */
	    rev_wraparound_mode = false;
	    break;
	case 47:	/* alt buffer */
	case 1049:
	    ctlr_altbuffer(false);
	    is_altbuffer = false;
	    dec_restore_cursor(0, 0);
	    break;
	}
    }
    return DATA;
}

static enum state
dec_save(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i <= nx && i < NN; i++) {
	switch (n[i]) {
	case 1:	/* application cursor keys */
	    saved_appl_cursor = appl_cursor;
	    break;
	case 3:	/* 132-column mode */
	    saved_wide_mode = wide_mode;
	    break;
	case 7:	/* wraparound mode */
	    saved_wraparound_mode = wraparound_mode;
	    break;
	case 40:	/* allow 80/132 switching */
	    saved_allow_wide_mode = allow_wide_mode;
	    break;
	case 45:	/* reverse-wraparound mode */
	    saved_rev_wraparound_mode = rev_wraparound_mode;
	    break;
	case 47:	/* alt buffer */
	case 1049:
	    saved_altbuffer = is_altbuffer;
	    dec_save_cursor(0, 0);
	    break;
	}
    }
    return DATA;
}

static enum state
dec_restore(int ig1 _is_unused, int ig2 _is_unused)
{
    int i;

    for (i = 0; i <= nx && i < NN; i++) {
	switch (n[i]) {
	case 1:	/* application cursor keys */
	    appl_cursor = saved_appl_cursor;
	    break;
	case 3:	/* 132-column mode */
	    if (allow_wide_mode) {
		wide_mode = saved_wide_mode;
		if (wide_mode) {
		    screen_132();
		} else {
		    screen_80();
		}
	    }
	    break;
	case 7:	/* wraparound mode */
	    wraparound_mode = saved_wraparound_mode;
	    break;
	case 40:	/* allow 80/132 switching */
	    allow_wide_mode = saved_allow_wide_mode;
	    break;
	case 45:	/* reverse-wraparound mode */
	    rev_wraparound_mode = saved_rev_wraparound_mode;
	    break;
	case 47:	/* alt buffer */
	case 1049:	/* alt buffer */
	    ctlr_altbuffer(saved_altbuffer);
	    is_altbuffer = saved_altbuffer;
	    dec_restore_cursor(0, 0);
	    break;
	}
    }
    return DATA;
}

static enum state
dec_scrolling_region(int top, int bottom)
{
    if (top < 1) {
	top = 1;
    }
    if (bottom > ROWS) {
	bottom = ROWS;
    }
    if (top <= bottom && (top > 1 || bottom < ROWS)) {
	scroll_top = top;
	scroll_bottom = bottom;
	cursor_move(0);
    } else {
	scroll_top = 1;
	scroll_bottom = ROWS;
    }
    return DATA;
}

static enum state
xterm_text_mode(int ig1 _is_unused, int ig2 _is_unused)
{
    nx = 0;
    n[0] = 0;
    return TEXT;
}

static enum state
xterm_text_semicolon(int ig1 _is_unused, int ig2 _is_unused)
{
    tx = 0;
    return TEXT2;
}

static enum state
xterm_text(int ig1 _is_unused, int ig2 _is_unused)
{
    if (tx < NT) {
	text[tx++] = nvt_ch;
    }
    return state;
}

static enum state
xterm_text_do(int ig1 _is_unused, int ig2 _is_unused)
{
    net_nvt_break();
    text[tx] = '\0';
    xterm_text_gui(n[0], text);
    return DATA;
}

static enum state
ansi_htab_set(int ig1 _is_unused, int ig2 _is_unused)
{
    int col = cursor_addr % COLS;

    tabs[col / 8] |= 1 << (col % 8);
    return DATA;
}

static enum state
ansi_htab_clear(int nn, int ig2 _is_unused)
{
    int col, i;

    switch (nn) {
    case 0:
	col = cursor_addr % COLS;
	tabs[col / 8] &= ~(1 << (col % 8));
	break;
    case 3:
	for (i = 0; i < (COLS + 7) / 8; i++) {
	    tabs[i] = 0;
	}
	break;
    }
    return DATA;
}

static enum state
ansi_gt(int ig1 _is_unused, int ig2 _is_unused)
{
    return ESCGT;
}

/*
 * Scroll the screen or the scrolling region.
 */
static void
nvt_scroll(void)
{
    held_wrap = false;

    /* Save the top line */
    if (scroll_top == 1 && scroll_bottom == ROWS) {
	if (!is_altbuffer) {
	    scroll_save(1);
	}
	ctlr_scroll(fg, bg);
	return;
    }

    /* Scroll all but the last line up */
    if (scroll_bottom > scroll_top) {
	ctlr_bcopy(scroll_top * COLS, (scroll_top - 1) * COLS,
		(scroll_bottom - scroll_top) * COLS, 1);
    }

    /* Clear the last line */
    ctlr_aclear((scroll_bottom - 1) * COLS, COLS, 1);
}

/* Callback for when we enter NVT mode. */
static void
nvt_in3270(bool in3270)
{
    if (in3270) {
	/*
	 * When switching to 3270 mode, clean up our external effects:
	 * cursor disable and alternate buffer.
	 */
	if (!cursor_enabled) {
	    cursor_enabled = true;
	    ctlr_enable_cursor(true, EC_NVT);
	}
	ctlr_altbuffer(false);
	is_altbuffer = false;
    } else {
	ansi_reset(0, 0);
    }
}

/* Callback for when we change connection state. */
static void
nvt_connect(bool connected)
{
    if (cursor_enabled != CONNECTED) {
	cursor_enabled = CONNECTED;
	ctlr_enable_cursor(cursor_enabled, EC_NVT);
    }
}

/*
 * External entry points
 */

void
nvt_process(unsigned int c)
{
    afn_t fn;

    c &= 0xff;
    nvt_ch = c;

    scroll_to_bottom();

    if (toggled(SCREEN_TRACE)) {
	trace_char((char)c);
    }

    fn = nvt_fn[st[(int)state][c]];
    state = (*fn)(n[0], n[1]);

    /* Saving pending escape data. */
    if (state == DATA) {
	pe = 0;
    } else if (pe < PE_MAX) {
	ped[pe++] = c;
    }

    /* Let a blocked task go. */
    task_store(c);
    task_host_output();
}

void
nvt_send_up(void)
{
    if (appl_cursor) {
	net_sends("\033OA");
    } else {
	net_sends("\033[A");
    }
}

void
nvt_send_down(void)
{
    if (appl_cursor) {
	net_sends("\033OB");
    } else {
	net_sends("\033[B");
    }
}

void
nvt_send_right(void)
{
    if (appl_cursor) {
	net_sends("\033OC");
    } else {
	net_sends("\033[C");
    }
}

void
nvt_send_left(void)
{
    if (appl_cursor) {
	net_sends("\033OD");
    } else {
	net_sends("\033[D");
    }
}

void
nvt_send_home(void)
{
    net_sends("\033[H");
}

void
nvt_send_clear(void)
{
    net_sends("\033[2K");
}

void
nvt_send_pf(int nn)
{
    char *s;
    static int code[] = {
	/*
	 * F1 through F12 are VT220 codes. (Note the discontinuity --
	 * \E[16~ is missing)
	 */
	11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24,
	/*
	 * F13 through F20 are defined for xterm.
	 */
	25, 26, 28, 29, 31, 32, 33, 34,
	/*
	 * F21 through F24 are x3270 extensions.
	 */
	35, 36, 37, 38
    };

    if (nn < 1 || (unsigned)nn > sizeof(code)/sizeof(code[0])) {
	return;
    }
    if (nn <= 4) {
	/* xterm sends PF codes instead of F codes for F1..F4. */
	nvt_send_pa(nn);
	return;
    }
    s = Asprintf("\033[%d~", code[nn-1]);
    net_sends(s);
    Free(s);
}

void
nvt_send_pa(int nn)
{
    char *s;
    static char code[4] = { 'P', 'Q', 'R', 'S' };

    if (nn < 1 || nn > 4) {
	return;
    }
    s = Asprintf("\033O%c", code[nn-1]);
    net_sends(s);
    Free(s);
}

static void
toggle_lineWrap(toggle_index_t ix _is_unused, enum toggle_type type _is_unused)
{
    if (toggled(LINE_WRAP)) {
	wraparound_mode = 1;
    } else {
	wraparound_mode = 0;
    }
}

/* Emit an SGR command. */
static void
emit_sgr(int mode)
{
    space3270out((mode < 10)? 4: 5);
    *obptr++ = 0x1b;
    *obptr++ = '[';
    if (mode > 9) {
	*obptr++ = '0' + (mode / 10);
    }
    *obptr++ = '0' + (mode % 10);
    *obptr++ = 'm';
}

/* Emit a DEC Private Mode command. */
static void
emit_decpriv(int mode, char op)
{
    space3270out((mode < 10)? 5: 6);
    *obptr++ = 0x1b;
    *obptr++ = '[';
    *obptr++ = '?';
    if (mode > 9) {
	*obptr++ = '0' + (mode / 10);
    }
    *obptr++ = '0' + (mode % 10);
    *obptr++ = op;
}

/* Emit a CUP (cursor position) command. */
static void
emit_cup(int baddr)
{
    if (baddr) {
	char *s;
	size_t sl;

	s = Asprintf("\033[%d;%dH", (baddr / COLS) + 1, (baddr % COLS) + 1);
	sl = strlen(s);
	space3270out(sl);
	strcpy((char *)obptr, s);
	Free(s);
	obptr += sl;
    } else {
	space3270out(3);
	*obptr++ = 0x1b;
	*obptr++ = '[';
	*obptr++ = 'H';
    }
}

/* Emit <n> spaces or a CUP, whichever is shorter. */
static int
ansi_dump_spaces(size_t spaces, int baddr)
{
    char *s;
    size_t sl;

    if (!spaces) {
	return 0;
    }

    /*
     * Move the cursor, if it takes less space than
     * expanding the spaces.
     * It is possible to optimize this further with clever
     * CU[UDFB] sequences, but not (yet) worth the effort.
     */
    s = Asprintf("\033[%d;%dH", (baddr / COLS) + 1, (baddr % COLS) + 1);
    sl = strlen(s);
    if (sl < spaces) {
	space3270out(sl);
	strcpy((char *)obptr, s);
	obptr += sl;
    } else {
	space3270out(spaces);
	while (spaces--) {
	    *obptr++ = ' ';
	}
    }

    Free(s);
    return 0;
}

/*
 * Snap the provided screen buffer (primary or alternate).
 * This is (mostly) optimized to draw the minimum necessary, assuming a
 * blank screen.
 */
static void
nvt_snap_one(struct ea *buf)
{
    int baddr;
    int cur_gr = 0;
    int cur_fg = 0;
    int cur_bg = 0;
    int spaces = 0;
    static int uncolor_table[16] = {
	/* 0xf0 */ 0,	/* neutral black -> black */
	/* 0xf1 */ 4,	/*          blue -> blue */
	/* 0xf2 */ 1,	/*           red -> red */
	/* 0xf3 */ 5,	/*          pink -> magenta */
	/* 0xf4 */ 2,	/*         green -> green */
	/* 0xf5 */ 6,	/*     turquoise -> cyan */
	/* 0xf6 */ 3,	/*        yellow -> yellow */
	/* 0xf7 */ 7,	/* neutral white -> white */
	/* 0xf8 */ 0,	/* (shouldn't happen) */
	/* 0xf9 */ 0,	/* (shouldn't happen) */
	/* 0xfa */ 0,	/* (shouldn't happen) */
	/* 0xfb */ 0,	/* (shouldn't happen) */
	/* 0xfc */ 0,	/* (shouldn't happen) */
	/* 0xfd */ 0,	/* (shouldn't happen) */
	/* 0xfe */ 0,	/* (shouldn't happen) */
	/* 0xff */ 0	/* (shouldn't happen) */
    };
    char mb[16];
    size_t len;
    int xlen;
    size_t i;
    enum dbcs_state d;
    ucs4_t u;
    int c;
    int last_sgr = 0;
#   define	EMIT_SGR(n)	{ emit_sgr(n); last_sgr = (n); }

    /* Draw what's on the screen. */
    baddr = 0;
    do {
	int xgr = buf[baddr].gr;

	/* Set the attributes. */
	if (xgr != cur_gr) {
	    spaces = ansi_dump_spaces(spaces, baddr);
	    if ((xgr ^ cur_gr) & cur_gr) {
		/*
		 * Something turned off. Turn everything off,
		 * then turn the remaining modes on below.
		 */
		EMIT_SGR(0);
		xgr = 0;
	    } else {
		/*
		 * Clear the bits in xgr that are already set
		 * in cur_gr.  Turn on the new modes.
		 */
		xgr &= ~cur_gr;
	    }
	    /* Turn on the attributes remaining in xgr. */
	    if (xgr & GR_INTENSIFY) {
		EMIT_SGR(1);
	    }
	    if (xgr & GR_UNDERLINE) {
		EMIT_SGR(4);
	    }
	    if (xgr & GR_BLINK) {
		EMIT_SGR(5);
	    }
	    if (xgr & GR_REVERSE) {
		EMIT_SGR(7);
	    }
	    cur_gr = buf[baddr].gr;
	}

	/* Set the colors. */
	if (buf[baddr].fg != cur_fg) {
	    spaces = ansi_dump_spaces(spaces, baddr);
	    if (buf[baddr].fg) {
		c = uncolor_table[buf[baddr].fg & 0x0f];
	    } else {
		c = 9;
	    }
	    EMIT_SGR(30 + c);
	    cur_fg = buf[baddr].fg;
	}
	if (buf[baddr].bg != cur_bg) {
	    spaces = ansi_dump_spaces(spaces, baddr);
	    if (buf[baddr].bg) {
		c = uncolor_table[buf[baddr].bg & 0x0f];
	    } else {
		c = 9;
	    }
	    EMIT_SGR(40 + c);
	    cur_bg = buf[baddr].bg;
	}

	/* Expand the current character to multibyte. */
	d = ctlr_dbcs_state(baddr);
	if (is_nvt(&buf[baddr], false, &u)) {
	    if (!IS_RIGHT(d)) {
		len = unicode_to_multibyte(u, mb, sizeof(mb));
	    } else {
		len = 0;
	    }
	} else {
	    if (IS_LEFT(d)) {
		int xaddr = baddr;
		INC_BA(xaddr);
		len = ebcdic_to_multibyte(buf[baddr].ec << 8 | buf[xaddr].ec,
			mb, sizeof(mb));
	    } else if (IS_RIGHT(d)) {
		len = 0;
	    } else {
		len = ebcdic_to_multibyte(buf[baddr].ec, mb, sizeof(mb));
	    }
	}

	if (len > 0) {
	    len--; /* terminating NUL */
	}
	xlen = 0;
	for (i = 0; i < len; i++) {
	    if ((mb[i] & 0xff) == 0xff) {
		xlen++;
	    }
	}

	/* Optimize for white space. */
	if (!cur_fg && !cur_bg && !cur_gr &&
	    ((len + xlen) == 1) && (mb[0] == ' ')) {
	    spaces++;
	} else {
	    if (spaces) {
		spaces = ansi_dump_spaces(spaces, baddr);
	    }

	    /* Emit the current character. */
	    space3270out(len + xlen);
	    for (i = 0; i < len; i++) {
		if ((mb[i] & 0xff) == 0xff) {
		    *obptr++ = 0xff;
		}
		*obptr++ = mb[i];
	    }
	}

	INC_BA(baddr);
    } while (baddr != 0);

    /* Remove any attributes we set above. */
    if (last_sgr != 0) {
	emit_sgr(0);
    }
}

/* Snap the contents of the screen buffers in NVT mode. */
void
nvt_snap(void)
{
    /*
     * Note that ea_buf is the live buffer, and aea_buf is the other
     * buffer.  So the task here is to draw the other buffer first,
     * then switch modes and draw the live one.
     */
    if (is_altbuffer) {
	/* Draw the primary screen first. */
	nvt_snap_one(aea_buf);
	emit_cup(0);

	/* Switch to the alternate. */
	emit_decpriv(47, 'h');

	/* Draw the secondary, and stay in alternate mode. */
	nvt_snap_one(ea_buf);
    } else {
	int i;
	int any = 0;
	static struct ea zea = { 0, 0, 0, 0, 0, 0, 0, 0 };

	/* See if aea_buf has anything in it. */
	for (i = 0; i < ROWS * COLS; i++) {
	    if (memcmp(&aea_buf[i], &zea, sizeof(struct ea))) {
		any = 1;
		break;
	    }
	}

	if (any) {
	    /* Switch to the alternate. */
	    emit_decpriv(47, 'h');

	    /* Draw the alternate screen. */
	    nvt_snap_one(aea_buf);
	    emit_cup(0);

	    /* Switch to the primary. */
	    emit_decpriv(47, 'l');
	}

	/* Draw the primary, and stay in primary mode. */
	nvt_snap_one(ea_buf);
    }
}

/*
 * Snap the non-default terminal modes.
 * This is a subtle piece of logic, and may harbor a few bugs yet.
 */
void
nvt_snap_modes(void)
{
    int i;
    static char csdsel[4] = "()*+";

    /* Set up the saved cursor (cursor, fg, bg, gr, cset, csd). */
    if (saved_cursor != 0 ||
	saved_fg != 0 ||
	saved_bg != 0 ||
	saved_gr != 0 ||
	saved_cset != CS_G0 ||
	saved_csd[0] != CSD_US ||
	saved_csd[1] != CSD_US ||
	saved_csd[2] != CSD_US ||
	saved_csd[3] != CSD_US ||
	!cursor_enabled) {

	if (saved_cursor != 0) {
	    emit_cup(saved_cursor);
	}
	if (saved_fg != 0) {
	    emit_sgr(30 + saved_fg);
	}
	if (saved_bg != 0) {
	    emit_sgr(40 + saved_bg);
	}
	if (saved_gr != 0) {
	    if (saved_gr & GR_INTENSIFY) {
		emit_sgr(1);
	    }
	    if (saved_gr & GR_UNDERLINE) {
		emit_sgr(4);
	    }
	    if (saved_gr & GR_BLINK) {
		emit_sgr(5);
	    }
	    if (saved_gr & GR_REVERSE) {
		emit_sgr(7);
	    }
	}
	if (saved_cset != CS_G0) {
	    switch (saved_cset) {
	    case CS_G1:
		space3270out(1);
		*obptr++ = 0x0e;
		break;
	    case CS_G2:
		space3270out(2);
		*obptr++ = 0x1b;
		*obptr++ = 'N';
		break;
	    case CS_G3:
		space3270out(2);
		*obptr++ = 0x1b;
		*obptr++ = 'O';
		break;
	    default:
		break;
	    }
	}
	for (i = 0; i < 4; i++) {
	    if (saved_csd[i] != CSD_US) {
		space3270out(3);
		*obptr++ = 0x1b;
		*obptr++ = csdsel[i];
		*obptr++ = gnnames[saved_csd[i]];
	    }
	}
	if (!cursor_enabled) {
	    space3270out(6);
	    *obptr++ = 0x1b;
	    *obptr++ = '[';
	    *obptr++ = '?';
	    *obptr++ = '2';
	    *obptr++ = '5';
	    *obptr++ = 'l';
	}

	/* Emit a SAVE CURSOR to stash these away. */
	space3270out(2);
	*obptr++ = 0x1b;
	*obptr++ = '7';
    }

    /* Now set the above to their current values, except for the cursor. */
    if (fg != saved_fg) {
	emit_sgr(30 + fg);
    }
    if (bg != saved_bg) {
	emit_sgr(40 + bg);
    }
    if (gr != saved_gr) {
	emit_sgr(0);
	if (gr & GR_INTENSIFY) {
	    emit_sgr(1);
	}
	if (gr & GR_UNDERLINE) {
	    emit_sgr(4);
	}
	if (gr & GR_BLINK) {
	    emit_sgr(5);
	}
	if (gr & GR_REVERSE) {
	    emit_sgr(7);
	}
    }
    if (cset != saved_cset) {
	switch (cset) {
	case CS_G0:
	    space3270out(1);
	    *obptr++ = 0x0f;
	    break;
	case CS_G1:
	    space3270out(1);
	    *obptr++ = 0x0e;
	    break;
	case CS_G2:
	    space3270out(2);
	    *obptr++ = 0x1b;
	    *obptr++ = 'n';
	    break;
	case CS_G3:
	    space3270out(2);
	    *obptr++ = 0x1b;
	    *obptr++ = 'o';
	    break;
	default:
	    break;
	}
    }
    for (i = 0; i < 4; i++) {
	if (csd[i] != saved_csd[i]) {
	    space3270out(3);
	    *obptr++ = 0x1b;
	    *obptr++ = csdsel[i];
	    *obptr++ = gnnames[csd[i]];
	}
    }

    /*
     * Handle appl_cursor, wrapaparound_mode, rev_wraparound_mode,
     * allow_wide_mode, wide_mode and altbuffer, both the saved values and
     * the current ones.
     */
    if (saved_appl_cursor) {
	emit_decpriv(1, 'h');		/* set */
	emit_decpriv(1, 's');		/* save */
	if (!appl_cursor) {
	    emit_decpriv(1, 'l');	/* reset */
	}
    } else if (appl_cursor) {
	emit_decpriv(1, 'h');		/* set */
    }
    if (saved_wide_mode) {
	emit_decpriv(3, 'h');		/* set */
	emit_decpriv(3, 's');		/* save */
	if (!wide_mode) {
	    emit_decpriv(3, 'l');	/* reset */
	}
    } else if (wide_mode) {
	emit_decpriv(3, 'h');		/* set */
    }
    if (saved_wraparound_mode == 0) {
	emit_decpriv(7, 'h');		/* set (no-wraparound mode) */
	emit_decpriv(7, 's');		/* save */
	if (wraparound_mode) {
	    emit_decpriv(7, 'l');	/* reset */
	}
    } else if (!wraparound_mode) {
	emit_decpriv(7, 'h');		/* set (no-wraparound mode) */
    }
    if (saved_allow_wide_mode) {
	emit_decpriv(40, 'h');		/* set */
	emit_decpriv(40, 's');		/* save */
	if (!allow_wide_mode) {
	    emit_decpriv(40, 'l');	/* reset */
	}
    } else if (allow_wide_mode) {
	emit_decpriv(40, 'h');		/* set */
    }
    if (saved_rev_wraparound_mode) {
	emit_decpriv(45, 'h');		/* set (rev--wraparound mode) */
	emit_decpriv(45, 's');		/* save */
	if (!rev_wraparound_mode) {
	    emit_decpriv(45, 'l');	/* reset */
	}
    } else if (rev_wraparound_mode) {
	emit_decpriv(45, 'h');		/* set (rev-wraparound mode) */
    }
    if (saved_altbuffer) {
	emit_decpriv(47, 'h');		/* set */
	emit_decpriv(47, 's');		/* save */
	if (!is_altbuffer) {
	    emit_decpriv(47, 'l');	/* reset */
	}
    } /* else not necessary to set it now -- it was already set when the
	 screen was drawn */

    /*
     * Now take care of auto_newline, insert mode, the scroll region
     * and tabs.
     */
    if (auto_newline_mode) {
	space3270out(4);
	*obptr++ = 0x1b;
	*obptr++ = '[';
	*obptr++ = '4';
	*obptr++ = 'h';
    }
    if (insert_mode) {
	space3270out(5);
	*obptr++ = 0x1b;
	*obptr++ = '[';
	*obptr++ = '2';
	*obptr++ = '0';
	*obptr++ = 'h';
    }
    if (scroll_top != 1 || scroll_bottom != ROWS) {
	space3270out(10);
	obptr += sprintf((char *)obptr, "\033[%d;%dr", scroll_top,
		scroll_bottom);
    }
    if (tabs) {
	unsigned char *deftabs;

	deftabs = (unsigned char *)Malloc((COLS + 7) / 8);
	for (i = 0; i < (COLS + 7) / 8; i++) {
	    deftabs[i] = 0x01;
	}
	for (i = 0; i < COLS; i++) {
	    if (tabs[i / 8] & 1 << (i % 8)) {
		    if (!(deftabs[i / 8] & 1 << (i % 8))) {
			/* Tab was cleared. */
			space3270out(15);
			obptr += sprintf((char *)obptr,
				"\033[%d;%dH",
				(cursor_addr / COLS) + 1,
				((cursor_addr + i) % COLS) + 1);
			*obptr++ = 0x1b;
			*obptr++ = '[';
			*obptr++ = '0';
			*obptr++ = 'g';
		    }
	    } else {
		if (deftabs[i / 8] & 1 << (i % 8)) {
		    /* Tab was set. */
		    space3270out(13);
		    obptr += sprintf((char *)obptr,
			    "\033[%d;%dH",
			    (cursor_addr / COLS) + 1,
			    ((cursor_addr + i) % COLS) + 1);
		    *obptr++ = 0x1b;
		    *obptr++ = 'H';
		}
	    }
	}
    }

    /*
     * We're done moving the cursor for other purposes (saving it,
     * messing with tabs).  Put it where it should be now.
     */
    emit_cup(cursor_addr);

    /* Now add any pending single-character CS change. */
    switch (once_cset) {
    case CS_G2:
	space3270out(2);
	*obptr++ = 0x1b;
	*obptr++ = 'N';
	break;
    case CS_G3:
	space3270out(2);
	*obptr++ = 0x1b;
	*obptr++ = 'O';
	break;
    default:
	break;
    }

    /* Now add any incomplete escape sequence. */
    if (pe) {
	int xlen = 0;

	for (i = 0; i < pe; i++) {
	    if (ped[i] == 0xff) {
		xlen++;
	    }
	}
	space3270out(pe + xlen);
	for (i = 0; i < pe; i++) {
	    if (ped[i] == 0xff) {
		*obptr++ = 0xff;
	    }
	    *obptr++ = ped[i];
	}
    }

    /* Last, emit any incomplete multi-byte data. */
    if (pmi) {
	space3270out(pmi);
	for (i = 0; i < pmi; i++) {
	    *obptr++ = pending_mbs[i];
	}
    }
}

/**
 * NVT-mode module registration.
 */
void
nvt_register(void)
{
    static toggle_register_t toggles[] = {
	{ LINE_WRAP, toggle_lineWrap, 0 }
    };

    /* Register our toggles. */
    register_toggles(toggles, array_count(toggles));

    /* Register for state changes. */
    register_schange(ST_3270_MODE, nvt_in3270);
    register_schange(ST_CONNECT, nvt_connect);
}

/**
 * Test a buffer position for NVT mode text.
 * Translates line-drawing characters to Unicode.
 *
 * @param[in] ea	Buffer position
 * @param[in] ascii_box_draw True to do ASCII-art box drawing
 * @param[out] u	Returned Unicode value
 *
 * @return true if NVT text present
 */
bool
is_nvt(struct ea *ea, bool ascii_box_draw, ucs4_t *u)
{
    if (ea->cs == CS_LINEDRAW) {
	*u = linedraw_to_unicode(ea->ucs4, ascii_box_draw);
	return true;
    }
    if ((*u = ea->ucs4) != 0) {
	return true;
    }
    return false;
}

/* Do a backspace with wraparound. */
void
nvt_wrapping_backspace(void)
{
    bool prev = rev_wraparound_mode;

    rev_wraparound_mode = true;
    nvt_process((unsigned int)'\b');
    rev_wraparound_mode = prev;
}
