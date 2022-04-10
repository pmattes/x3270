/*
 * Copyright (c) 1995-2022 Paul Mattes.
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
 *	host.h
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
extern enum iaction connect_ia;
extern char *host_prefix;
extern char *host_suffix;
extern bool host_retry_mode;

/* Host connect/disconnect and state change. */
void hostfile_init(void);
bool host_connect(const char *n, enum iaction ia);
void host_connected(void);
void host_continue_connect(iosrc_t iosrc, net_connect_t nc);
void host_new_connection(bool pending);
void host_disconnect(bool disable);
void host_in3270(enum cstate);
void host_newfd(iosrc_t s);
bool host_reconnecting(void);
void host_register(void);
void host_set_flag(int flag);
