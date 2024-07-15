/*
 * Copyright (c) 1995-2024 Paul Mattes.
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
 *	task.h
 *		Global declarations for task.c.
 */

/* macro definition */
struct macro_def {
    char		*name;
    char		**parents;
    char		*action;
    struct macro_def	*next;
};
extern struct macro_def *macro_defs;
typedef void *task_cbh;

void abort_script(void);
void abort_script_by_cb(const char *cb_name);
void abort_queue(const char *unique_name);
void macro_command(struct macro_def *m);
void peer_script_init(void);
void connect_error(const char *fmt, ...) printflike(1, 2);
void connect_errno(int e, const char *fmt, ...) printflike(2, 3);
void macros_init(void);
void ps_set(char *s, bool is_hex, bool force_utf8);
void push_keymap_action(char *);
void push_keypad_action(char *);
void push_macro(char *);
void push_stack_macro(char *);
bool task_active(void);
bool task_can_kbwait(void);
void task_connect_wait(void);
bool run_tasks(void);
void task_error(const char *msg);
void task_host_output(void);
void task_info(const char *fmt, ...) printflike(1, 2);
bool task_ifield_can_proceed(void);
void task_ft_complete(const char *msg, bool is_error);
void task_kbwait(void);
void task_passthru_done(const char *tag, bool success, const char *result);
bool task_redirect(void);
const char *task_set_passthru(task_cbh **ret_cbh);
void task_store(unsigned char c);
void task_abort_input_request_irhandle(void *irhandle);
void task_abort_input_request(void);
bool task_is_interactive(void);
bool task_nonblocking_connect(void);

/* Input request vectors. */
typedef void (*ir_state_abort_cb)(void *state);
typedef void (*task_setir_cb)(task_cbh handle, void *irhandle);
typedef void *(*task_getir_cb)(task_cbh handle);
typedef void (*task_setir_state_cb)(task_cbh handle, const char *name,
	void *state, ir_state_abort_cb abort_cb);
typedef void *(*task_getir_state_cb)(task_cbh handle, const char *name);
typedef struct {
    task_setir_cb setir;
    task_getir_cb getir;
    task_setir_state_cb setir_state;
    task_getir_state_cb getir_state;
} irv_t;

typedef void (*task_data_cb)(task_cbh handle, const char *buf, size_t len,
	bool success);
typedef bool (*task_done_cb)(task_cbh handle, bool success, bool abort);
typedef bool (*task_run_cb)(task_cbh handle, bool *success);
typedef void (*task_closescript_cb)(task_cbh handle);
typedef void (*task_setflags_cb)(task_cbh handle, unsigned flags);
typedef unsigned (*task_getflags_cb)(task_cbh handle);
typedef bool (*task_need_delay_cb)(task_cbh handle);
typedef const char *(*task_command_cb)(task_cbh handle);
typedef void (*task_reqinput_cb)(task_cbh handle, const char *buf, size_t len,
	bool echo);
typedef struct {
    const char *shortname;
    enum iaction ia;
    unsigned flags;
    task_data_cb data;
    task_done_cb done;
    task_run_cb run;
    task_closescript_cb closescript;
    task_setflags_cb setflags;
    task_getflags_cb getflags;
    irv_t *irv;
    task_command_cb command;
    task_reqinput_cb reqinput;
    task_setflags_cb setxflags;
    task_getflags_cb getxflags;
} tcb_t;
#define CB_UI		0x1	/* came from the UI */
#define CB_NEEDS_RUN	0x2	/* needs its run method called */
#define CB_NEW_TASKQ	0x4	/* creates a new task queue */
#define CB_PEER		0x8	/* peer script (don't abort) */
#define CB_NEEDCOOKIE	0x10	/* needs a valid cookie */

#define CBF_INTERACTIVE	0x1	/* settable: interactive (e.g., c3270 prompt) */
#define CBF_CONNECT_FT_NONBLOCK 0x2 /* do not block Connect()/Open()/Transfer() */
#define CBF_PWINPUT	0x4	/* can do password (no echo) input */
#define CBF_ERRD	0x8	/* understands 'errd:' error output */

#define XF_HAVECOOKIE	0x1	/* has a valid cookie */
char *push_cb(const char *buf, size_t len, const tcb_t *cb,
	task_cbh handle);
struct cmd {
    const char *action;	/* action to execute */
    const char **args;	/* arguments */
};
char *push_cb_split(cmd_t **cmds, const tcb_t *cb, task_cbh handle);
void task_activate(task_cbh handle);
void task_register(void);
char *task_cb_prompt(task_cbh handle);
unsigned long task_cb_msec(task_cbh handle);
const char *task_cb_name(void *);

typedef bool continue_fn(void *, const char *);
typedef void abort_fn(void *);
bool task_request_input(const char *action, const char *prompt,
	continue_fn *continue_fn, abort_fn *abort_fn, void *handle,
	bool no_echo);
bool task_can_request_input(const char *action, bool no_echo);

typedef llist_t task_cb_ir_state_t;
void task_cb_init_ir_state(task_cb_ir_state_t *ir_state);
void task_cb_set_ir_state(task_cb_ir_state_t *ir_state, const char *name,
	void *state, ir_state_abort_cb abort);
void *task_cb_get_ir_state(task_cb_ir_state_t *ir_state, const char *name);
void task_cb_abort_ir_state(task_cb_ir_state_t *ir_state);

void task_set_ir_state(const char *name, void *state, ir_state_abort_cb abort);
void *task_get_ir_state(const char *name);

void task_resume_xwait(void *context, bool cancel, const char *why);
typedef void xcontinue_fn(void *context, bool cancel);
void task_xwait(void *context, xcontinue_fn *continue_fn, const char *why);

char *task_get_tasks(void);
bool validate_command(const char *command, int offset, char **error);

bool task_running_cb_contains(tcb_t *cb);
char *task_status_string(void);

bool task_kbwait_state(void);
