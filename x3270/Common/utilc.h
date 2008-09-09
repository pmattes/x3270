/*
 * Copyright 1995-2008 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
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
