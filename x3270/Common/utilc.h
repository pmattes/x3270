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
 *	utilc.h
 *		Global declarations for util.c.
 */

extern void add_resource(const char *name, const char *value);
extern char *ctl_see(int c);
extern char *do_subst(const char *s, Boolean do_vars, Boolean do_tilde);
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
extern char *xs_buffer(const char *fmt, ...) printflike(1, 2);
extern void xs_error(const char *fmt, ...) printflike(1, 2);
extern void xs_warning(const char *fmt, ...) printflike(1, 2);

extern unsigned long AddInput(int, void (*)(void));
extern unsigned long AddExcept(int, void (*)(void));
extern unsigned long AddOutput(int, void (*)(void));
extern void RemoveInput(unsigned long);
extern unsigned long AddTimeOut(unsigned long msec, void (*fn)(void));
extern void RemoveTimeOut(unsigned long cookie);
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
