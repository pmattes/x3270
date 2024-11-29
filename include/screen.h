/*
 * Copyright (c) 1999-2024 Paul Mattes.
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
 * 	screen.h
 * 		Declarations for screen.c.
 */
extern int *char_width, *char_height;

void cursor_move(int baddr);
void blink_start(void);
unsigned display_heightMM(void);
unsigned display_height(void);
unsigned display_widthMM(void);
unsigned display_width(void);
void mcursor_locked(void);
void mcursor_normal(void);
void mcursor_waiting(void);
bool screen_obscured(void);
void screen_scroll(unsigned char fg, unsigned char bg);
unsigned long screen_window_number(void);
bool screen_has_bg_color(void);
void ring_bell(void);
void screen_disp(bool erasing);
void screen_80(void);
void screen_132(void);
void screen_flip(void);
bool screen_flipped(void);
bool screen_selected(int baddr);
bool screen_new_display_charsets(const char *realname);
void screen_system_fixup(void);
bool screen_suspend(void);
void screen_set_thumb(float top, float shown, int saved, int screen, int back);
void enable_cursor(bool on);
void screen_init(void);
void screen_change_model(int mn, int ovc, int ovr);
