/*
 * Copyright (c) 1996-2014, Paul Mattes.
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
 *	ft_guic.h
 *		Header file for file transfer dialogs.
 */

#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
extern void ft_gui_progress_popdown(void);
extern void ft_gui_errmsg_prepare(char *msg);
extern void ft_gui_clear_progress(void);
extern void ft_gui_complete_popup(const char *msg);
extern void ft_gui_update_length(unsigned long length);
extern void ft_gui_running(unsigned long length);
extern void ft_gui_aborting(void);
extern Boolean ft_gui_interact(char ***params, unsigned *num_params);
extern void ft_gui_awaiting(void);
# if defined(X3270_DISPLAY) /*[*/
extern void ft_gui_popup_ft(void);
# endif /*]*/
#else /*][*/
# define ft_gui_progress_popdown()
# define ft_gui_errmsg_prepare(msg)
# define ft_gui_clear_progress()
# define ft_gui_complete_popup(msg)
# define ft_gui_update_length(length)
# define ft_gui_running(length)
# define ft_gui_aborting()
# define ft_gui_interact(params, num_params)	False
# define ft_gui_awaiting()
#endif /*]*/
