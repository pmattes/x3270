/*
 * Copyright (c) 1995-2009, 2014-2016, 2018, 2020 Paul Mattes.
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
 *	actions.h
 *		Global declarations for actions.c.
 */

typedef struct {
    const char *name;
    action_t *action;
    unsigned flags;
    unsigned help_flags;
    const char *help_parms;
    const char *help_text;
    ia_t ia_restrict;
} action_table_t;
#define ACTION_KE	0x1	/* action is valid from key events */
#define ACTION_HIDDEN	0x2	/* action does not have help or tab expansion */

typedef struct action_elt {
    llist_t list;		/* linkage */
    action_table_t t;		/* payload */
} action_elt_t;

extern llist_t actions_list;
extern unsigned actions_list_count;

extern const char       *ia_name[];

extern const char *current_action_name;

void action_debug(const char *aname, ia_t ia, unsigned argc,
	const char **argv);
bool run_action(const char *name, enum iaction cause, const char *parm1,
	const char *parm2);
bool run_action_a(const char *name, enum iaction cause, unsigned count,
	const char **parms);
bool run_action_entry(action_elt_t *e, enum iaction cause, unsigned count,
	const char **parms);
int check_argc(const char *aname, unsigned nargs, unsigned nargs_min,
	unsigned nargs_max);
void register_actions(action_table_t *actions, unsigned count);
char *safe_param(const char *s);
void disable_keyboard(bool disable, bool explicit, const char *why);
#define DISABLE		true
#define ENABLE		false
#define EXPLICIT	true
#define IMPLICIT	false
void force_enable_keyboard(void);
bool keyboard_disabled(void);
const char *all_actions(void);
bool action_args_are(const char *name, ...);
