/*
 * Copyright (c) 1993-2009, 2013-2015 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	togglesc.h
 *		Global declarations for toggles.c.
 */

typedef struct toggle {
    Boolean changed;    /* has the value changed since init */
    void *w[2];         /* the menu item widgets */
    const char *label[2]; /* labels */
    void (*upcall)(toggle_index_t ix, enum toggle_type); /* change value */
} toggle_t;

extern toggle_t toggle[];

#define set_toggle(ix, value)	appres.toggle[ix] = value
#define toggle_toggle(ix) { \
    set_toggle(ix, !toggled(ix)); \
    toggle[ix].changed = True; \
}
#define toggle_ix(t)		(toggle_index_t)((t) - toggle)
#define TOGGLE_BIT(ix)		(1 << (ix))
#define TOGGLE_SUPPORTED(ix)	(toggles_supported & TOGGLE_BIT(ix))

extern void do_menu_toggle(int);
extern void do_toggle(int);
extern void initialize_toggles(void);
extern void shutdown_toggles(void);
extern void toggles_init(void);
