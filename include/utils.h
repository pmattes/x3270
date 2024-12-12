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
 *	utils.h
 *		Global declarations for util.c.
 */

void add_resource(const char *name, const char *value);
char *ctl_see(int c);
char *do_subst(const char *s, unsigned flags);
#define DS_NONE		0x0
#define DS_VARS		0x1
#define DS_TILDE	0x2
#define DS_UNIQUE	0x4
const char *get_message(const char *key);
char *get_fresource(const char *fmt, ...) printflike(1, 2);
char *get_resource(const char *name);
int get_resource_int(const char *name);
bool get_resource_bool(const char *name);
char *get_underlying_resource(const char *name);
char *qscatv(const char *s);
char *scatv(const char *s);
int split_dbcs_resource(const char *value, char sep, char **part1,
	char **part2);
int s_split_dresource(const char *st, size_t *offset, char **left,
	char **right);
int split_dresource(char **st, char **left, char **right);
int split_lresource(char **st, char **value);
char *strip_whitespace(const char *s);
char *Vasprintf(const char *fmt, va_list);
char *Asprintf(const char *fmt, ...) printflike(1, 2);
void xs_error(const char *fmt, ...) printflike(1, 2);
void xs_warning(const char *fmt, ...) printflike(1, 2);

typedef void (*iofn_t)(iosrc_t, ioid_t id);
typedef void (*tofn_t)(ioid_t id);
typedef void (*childfn_t)(ioid_t id, int status);
ioid_t AddInput(iosrc_t fd, iofn_t fn);
ioid_t AddExcept(iosrc_t fd, iofn_t fn);
ioid_t AddOutput(iosrc_t fd, iofn_t fn);
#if !defined(_WIN32) /*[*/
ioid_t AddChild(pid_t pid, childfn_t fn);
#endif /*]*/
void RemoveInput(ioid_t);
ioid_t AddTimeOut(unsigned long msec, tofn_t);
void RemoveTimeOut(ioid_t id);

ks_t string_to_key(char *s);
char *key_to_string(ks_t k);
bool read_resource_file(const char *filename, bool fatal);
bool split_hier(const char *label, char **base, char ***parents);
void free_parents(char **parents);

const char *build_options(void);
void dump_version(void);
const char *display_scale(double d);
void array_add(const char ***s, int ix, const char *v);

/* Doubly-linked lists. */
bool llist_isempty(llist_t *l);
void llist_init(llist_t *l);
void llist_insert_before(llist_t *element, llist_t *before);
void llist_unlink(llist_t *element);

#define LLIST_INIT(head)	{ &head, &head }

#define FOREACH_LLIST(head, elt, type) { \
    llist_t *_elt; \
    llist_t *_next; \
    for (_elt = (head)->next; _elt != (head); _elt = _next) { \
	_next = _elt->next; \
	(elt) = (type)(void *)_elt;

#define FOREACH_LLIST_END(head, elt, type) \
    } \
}

#define LLIST_APPEND(elt, head)	llist_insert_before(elt, &head)
#define LLIST_PREPEND(elt, head) llist_insert_before(elt, head.next)

/* State changes. */
enum st {
    ST_NEGOTIATING,	/* protocol negotiation start */
    ST_CONNECT,		/* connection exists or is in progress */
    ST_3270_MODE,	/* into 3270 or NVT mode */
    ST_LINE_MODE,	/* in or out of NVT line mode */
    ST_REMODEL,		/* model changed */
    ST_PRINTER,		/* printer session state changed */
    ST_EXITING,		/* emulator exiting */
    ST_CODEPAGE,	/* code page changing */
    ST_SELECTING,	/* screen selection changing */
    ST_SECURE,		/* secure mode changing */
    ST_KBD_DISABLE,	/* keyboard disable changing */
    ST_TERMINAL_NAME,	/* terminal name changing */
    N_ST
};

#define ORDER_DONTCARE	0xfffe
#define ORDER_LAST	0xffff

typedef void schange_callback_t(bool);
void register_schange_ordered(enum st tx, schange_callback_t *func,
	unsigned short order);
void register_schange(enum st tx, schange_callback_t *func);
void st_changed(enum st tx, bool mode);
#if !defined(PR3287) /*[*/
void change_cstate(enum cstate cstate, const char *why);
#endif /*]*/
char *clean_termname(const char *tn);
void start_help(void);
const char *ut_getenv(const char *name);

enum ts { TS_AUTO, TS_ON, TS_OFF };
bool ts_value(const char *s, enum ts *tsp);
