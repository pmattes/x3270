/*
 * Copyright (c) 1995-2009, 2013-2014 Paul Mattes.
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
 *	utilc.h
 *		Global declarations for util.c.
 */

extern void add_resource(const char *name, const char *value);
extern char *ctl_see(int c);
extern char *do_subst(const char *s, unsigned flags);
#define DS_NONE		0x0
#define DS_VARS		0x1
#define DS_TILDE	0x2
#define DS_UNIQUE	0x4
extern void fcatv(FILE *f, char *s);
extern const char *get_message(const char *key);
extern char *get_fresource(const char *fmt, ...) printflike(1, 2);
extern char *get_resource(const char *name);
extern char *scatv(const char *s, char *buf, size_t len);
extern int split_dbcs_resource(const char *value, char sep, char **part1,
    char **part2);
extern int split_dresource(char **st, char **left, char **right);
extern int split_lresource(char **st, char **value);
extern char *strip_whitespace(const char *s);
extern char *xs_vbuffer(const char *fmt, va_list);
extern char *xs_buffer(const char *fmt, ...) printflike(1, 2);
extern void xs_error(const char *fmt, ...) printflike(1, 2);
extern void xs_warning(const char *fmt, ...) printflike(1, 2);

typedef void (*iofn_t)(unsigned long fd, ioid_t id);
typedef void (*tofn_t)(ioid_t id);
#define NULL_IOID	0L
extern ioid_t AddInput(unsigned long, iofn_t);
extern ioid_t AddExcept(unsigned long, iofn_t);
extern ioid_t AddOutput(unsigned long, iofn_t);
extern void RemoveInput(ioid_t);
extern ioid_t AddTimeOut(unsigned long msec, tofn_t);
extern void RemoveTimeOut(ioid_t id);

extern KeySym StringToKeysym(char *s);
extern char *KeysymToString(KeySym k);
extern int read_resource_file(const char *filename, Boolean fatal);
extern Boolean split_hier(char *label, char **base, char ***parents);

typedef struct {
	char *buf;
	int alloc_len;
	int cur_len;
} rpf_t;

extern void rpf_init(rpf_t *r);
extern void rpf_reset(rpf_t *r);
extern void rpf(rpf_t *r, char *fmt, ...) printflike(2, 3);
extern void rpf_free(rpf_t *r);
extern const char *build_options(void);
extern void dump_version(void);
extern const char *display_scale(double d, char *buf, size_t buflen);
#if defined(WC3270) /*[*/
extern void start_html_help(void);
#endif /*]*/

/* Doubly-linked lists. */
typedef struct llist {
    struct llist *next;
    struct llist *prev;
} llist_t;

extern Boolean llist_isempty(llist_t *l);
extern void llist_init(llist_t *l);
extern void llist_insert_before(llist_t *element, llist_t *before);
extern void llist_unlink(llist_t *element);

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
