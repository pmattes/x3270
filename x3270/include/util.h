/*
 * Copyright (c) 1995-2009, 2013-2015 Paul Mattes.
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
 *	util.h
 *		Global declarations for util.c.
 */

void add_resource(const char *name, const char *value);
char *ctl_see(int c);
char *do_subst(const char *s, unsigned flags);
#define DS_NONE		0x0
#define DS_VARS		0x1
#define DS_TILDE	0x2
#define DS_UNIQUE	0x4
void fcatv(FILE *f, char *s);
const char *get_message(const char *key);
char *get_fresource(const char *fmt, ...) printflike(1, 2);
char *get_resource(const char *name);
char *scatv(const char *s, char *buf, size_t len);
int split_dbcs_resource(const char *value, char sep, char **part1,
	char **part2);
int split_dresource(char **st, char **left, char **right);
int split_lresource(char **st, char **value);
char *strip_whitespace(const char *s);
char *xs_vbuffer(const char *fmt, va_list);
char *xs_buffer(const char *fmt, ...) printflike(1, 2);
void xs_error(const char *fmt, ...) printflike(1, 2);
void xs_warning(const char *fmt, ...) printflike(1, 2);

#if !defined(PR3287) /*[*/
typedef void (*iofn_t)(iosrc_t, ioid_t id);
typedef void (*tofn_t)(ioid_t id);
# define NULL_IOID	0L
ioid_t AddInput(iosrc_t fd, iofn_t fn);
ioid_t AddExcept(iosrc_t fd, iofn_t fn);
ioid_t AddOutput(iosrc_t fd, iofn_t fn);
void RemoveInput(ioid_t);
ioid_t AddTimeOut(unsigned long msec, tofn_t);
void RemoveTimeOut(ioid_t id);
#endif /*]*/

KeySym StringToKeysym(char *s);
char *KeysymToString(KeySym k);
Boolean read_resource_file(const char *filename, Boolean fatal);
Boolean split_hier(char *label, char **base, char ***parents);

const char *build_options(void);
void dump_version(void);
const char *display_scale(double d);

/* Doubly-linked lists. */
Boolean llist_isempty(llist_t *l);
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
