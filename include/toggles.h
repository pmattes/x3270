/*
 * Copyright (c) 1993-2023 Paul Mattes.
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
 *	toggles.h
 *		Global declarations for toggles.c.
 */

typedef struct {
    const char *name;
    toggle_index_t index;
    bool is_alias;
} toggle_name_t;
extern toggle_name_t toggle_names[];

enum toggle_type {
    TT_INITIAL,		/* at start-up */
    TT_INTERACTIVE,	/* at the prompt */
    TT_ACTION,		/* from a keymap, script or macro */
    TT_XMENU,		/* from a GUI menu */
    TT_FINAL		/* at shutdown */
};
typedef void toggle_upcall_t(toggle_index_t ix, enum toggle_type type);

void do_menu_toggle(int);
void do_toggle(int);
void initialize_toggles(void);
void toggles_register(void);
void toggle_toggle(toggle_index_t ix);
void set_toggle(toggle_index_t ix, bool value);
void set_toggle_initial(toggle_index_t ix, bool value);
bool toggle_changed(toggle_index_t ix);
bool toggle_supported(toggle_index_t ix);
void toggle_save_disconnect_set(const char *resource, const char *value, ia_t ia);

#define TOGGLE_NEED_INIT	0x1	/* needs start-up initialization */
#define TOGGLE_NEED_CLEANUP	0x2	/* needs shutdown clean-up */
typedef struct {
    toggle_index_t ix;
    toggle_upcall_t *upcall;
    unsigned flags;
} toggle_register_t;
void register_toggles(toggle_register_t toggles[], unsigned count);

#define XN_NONE			0x0
#define XN_DEFER		0x1	/* defer Set operation, if needed */
typedef enum {
    TU_SUCCESS,		/* succeeded */
    TU_FAILURE,		/* failed */
    TU_DEFERRED,	/* deferred */
} toggle_upcall_ret_t;
typedef toggle_upcall_ret_t toggle_extended_upcall_t(const char *name, const char *value, unsigned flags, ia_t ia);
typedef toggle_upcall_ret_t toggle_extended_done_t(bool success, unsigned flags, ia_t ia);
typedef const char *toggle_extended_canonicalize_t(const char *value);
void register_extended_toggle(const char *name,
	toggle_extended_upcall_t upcall, toggle_extended_done_t done,
	toggle_extended_canonicalize_t canonicalize, void **appres_address,
	enum resource_type resource_type);
typedef void toggle_extended_notify_t(const char *name, enum resource_type type,
	void **value, ia_t ia, unsigned flags);
void register_extended_toggle_notify(toggle_extended_notify_t notify);
void force_toggle_notify(const char *name, ia_t ia);
char **extended_toggle_names(int *countp, bool bool_only);
void *find_extended_toggle(const char *name, enum resource_type type);
int init_extended_toggle(const char *name, size_t nlen, bool bool_only,
	const char *value, const char **proper_namep);

bool Set_action(ia_t ia, unsigned argc, const char **argv);
bool Toggle_action(ia_t ia, unsigned argc, const char **argv);

typedef struct {
    const char *name;
    const char *value;
} tnv_t;
tnv_t *toggle_values(void);
