/*
 * Copyright (c) 1995-2009, 2014-2015 Paul Mattes.
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
 *	macros.h
 *		Global declarations for macros.c.
 */

/* macro definition */
struct macro_def {
    char		*name;
    char		**parents;
    char		*action;
    struct macro_def	*next;
};
extern struct macro_def *macro_defs;
extern bool macro_output;

void abort_script(void);
void cancel_if_idle_command(void);
void login_macro(char *s);
void macros_init(void);
void macro_command(struct macro_def *m);
void peer_script_init(void);
void ps_set(char *s, bool is_hex);
void push_command(char *);
void push_idle(char *);
void push_keymap_action(char *);
void push_macro(char *, bool);
void sms_accumulate_time(struct timeval *, struct timeval *);
bool sms_active(void);
void sms_connect_wait(void);
void sms_continue(void);
void sms_error(const char *msg);
void sms_host_output(void);
void sms_info(const char *fmt, ...) printflike(1, 2);
bool sms_in_macro(void);
bool sms_redirect(void);
void sms_store(unsigned char c);

typedef void *sms_cbh;
typedef void (*sms_data_cb)(sms_cbh handle, const char *buf, size_t len);
typedef void (*sms_done_cb)(sms_cbh handle, bool success,
	const char *status_buf, size_t status_len);
typedef struct {
    const char *shortname;
    enum iaction ia;
    sms_data_cb data;
    sms_done_cb done;
} sms_cb_t;
void push_cb(const char *buf, size_t len, const sms_cb_t *cb,
	sms_cbh handle);
void macros_register(void);
