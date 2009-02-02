/*
 * Copyright (c) 1995-2009, Paul Mattes.
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
 *	hostc.h
 *		Global declarations for host.c.
 */

struct host {
	char *name;
	char **parents;
	char *hostname;
	enum { PRIMARY, ALIAS, RECENT } entry_type;
	char *loginstring;
	time_t connect_time;
	struct host *prev, *next;
};
extern struct host *hosts;

extern void Connect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Disconnect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Reconnect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);

/* Host connect/disconnect and state change. */
extern void hostfile_init(void);
extern void host_cancel_reconnect(void);
extern int host_connect(const char *n);
extern void host_connected(void);
extern void host_disconnect(Boolean disable);
extern void host_in3270(enum cstate);
extern void register_schange(int tx, void (*func)(Boolean));
extern void st_changed(int tx, Boolean mode);
