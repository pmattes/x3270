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
} action_table_t;
#define ACTION_KE	0x1	/* action is valid from key events */

typedef struct action_elt {
    llist_t list;		/* linkage */
    action_table_t t;		/* payload */
} action_elt_t;

extern llist_t actions_list;
extern unsigned actions_list_count;

extern const char       *ia_name[];

Boolean action_suppressed(const char *name, const char *suppress);
void action_debug(const char *aname, ia_t ia, unsigned argc,
	const char **argv);
void run_action(const char *name, enum iaction cause, const char *parm1,
	const char *parm2);
int check_argc(const char *aname, unsigned nargs, unsigned nargs_min,
	unsigned nargs_max);
void register_actions(action_table_t *actions, unsigned count);
