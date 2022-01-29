/*
 * Copyright (c) 2015, 2018, 2020, 2022 Paul Mattes.
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
 *	opts.h
 *		Command-line option processing declarations.
 */

/* Offset macros. */
#define aoffset(n) (void *)&appres.n
#define toggle_aoffset(index) aoffset(toggle[index])

/* Option defition. */
typedef struct {
    const char *name;
    enum {
	OPT_BOOLEAN,	/* set bool to True or False */
	OPT_STRING,	/* set a (char *) */
	OPT_XRM,	/* special for "-xrm" syntax */
	OPT_SKIP2,	/* skip the next token */
	OPT_NOP,	/* do nothing */
	OPT_INT,	/* set an integer */
	OPT_SET,	/* special for -set */
	OPT_CLEAR,	/* special for -clear */
	OPT_V,		/* special for "-v" */
	OPT_HELP,	/* special for "--help */
	OPT_DONE	/* option-list terminator */
    } type;
    bool flag;	/* value if OPT_BOOLEAN */
    const char *res_name; /* name of resource to set, or NULL */
    void *aoff;		/* appres offset */
    char *help_opts;	/* options to display for help, or NULL */
    char *help_text;	/* help text, or NULL */
} opt_t;

/* Register an array of options. */
void register_opts(opt_t *opts, unsigned num_opts);

/* Resource definition. */
typedef struct {
    const char *name;
    void *address;
    enum resource_type type;
} res_t;

/* Register an array of resources. */
void register_resources(res_t *res, unsigned num_res);

/* Explicit (non-appres) resource definition. */
typedef struct {
    const char *name;
    enum {
	V_FLAT,		/* match the full name: <name> */
	V_WILD,		/* name is the root, i.e., <name>.* */
	V_COLOR		/* match <name><host-color-name> or
			   <name><host-color-index> */
    } type;
} xres_t;

/* Register an array of explicit resources. */
void register_xresources(xres_t *xres, unsigned num_xres);
