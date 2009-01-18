/*
 * Copyright (c) 2005-2009, Paul Mattes.
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
 *	ctlrc.h
 *		Global declarations for ctlr.c.
 */

enum pds {
	PDS_OKAY_NO_OUTPUT = 0,	/* command accepted, produced no output */
	PDS_OKAY_OUTPUT = 1,	/* command accepted, produced output */
	PDS_BAD_CMD = -1,	/* command rejected */
	PDS_BAD_ADDR = -2	/* command contained a bad address */
};

void ctlr_aclear(int baddr, int count, int clear_ea);
void ctlr_add(int baddr, unsigned char c, unsigned char cs);
void ctlr_add_bg(int baddr, unsigned char color);
void ctlr_add_cs(int baddr, unsigned char cs);
void ctlr_add_fa(int baddr, unsigned char fa, unsigned char cs);
void ctlr_add_fg(int baddr, unsigned char color);
void ctlr_add_gr(int baddr, unsigned char gr);
void ctlr_altbuffer(Boolean alt);
Boolean ctlr_any_data(void);
void ctlr_bcopy(int baddr_from, int baddr_to, int count, int move_ea);
void ctlr_changed(int bstart, int bend);
void ctlr_clear(Boolean can_snap);
void ctlr_erase(Boolean alt);
void ctlr_erase_all_unprotected(void);
void ctlr_init(unsigned cmask);
void ctlr_read_buffer(unsigned char aid_byte);
void ctlr_read_modified(unsigned char aid_byte, Boolean all);
void ctlr_reinit(unsigned cmask);
void ctlr_scroll(void);
void ctlr_shrink(void);
void ctlr_snap_buffer(void);
void ctlr_snap_buffer_sscp_lu(void);
Boolean ctlr_snap_modes(void);
void ctlr_wrapping_memmove(int baddr_to, int baddr_from, int count);
enum pds ctlr_write(unsigned char buf[], int buflen, Boolean erase);
void ctlr_write_sscp_lu(unsigned char buf[], int buflen);
struct ea *fa2ea(int baddr);
int find_field_attribute(int baddr);
unsigned char get_field_attribute(register int baddr);
Boolean get_bounded_field_attribute(register int baddr, register int bound,
    unsigned char *fa_out);
void mdt_clear(int baddr);
void mdt_set(int baddr);
int next_unprotected(int baddr0);
enum pds process_ds(unsigned char *buf, int buflen);
void ps_process(void);
void set_rows_cols(int mn, int ovc, int ovr);
void ticking_start(Boolean anyway);
void toggle_nop(struct toggle *t, enum toggle_type tt);
void toggle_showTiming(struct toggle *t, enum toggle_type tt);

enum dbcs_state {
	DBCS_NONE = 0,		/* position is not DBCS */
	DBCS_LEFT,		/* position is left half of DBCS character */
	DBCS_RIGHT,		/* position is right half of DBCS character */
	DBCS_SI,		/* position is SI terminating DBCS subfield */
	DBCS_SB,		/* position is SBCS character after the SI */
	DBCS_LEFT_WRAP,		/* position is left half of split DBCS */
	DBCS_RIGHT_WRAP,	/* position is right half of split DBCS */
	DBCS_DEAD		/* position is dead left-half DBCS */
};
#define IS_LEFT(d)	((d) == DBCS_LEFT || (d) == DBCS_LEFT_WRAP)
#define IS_RIGHT(d)	((d) == DBCS_RIGHT || (d) == DBCS_RIGHT_WRAP)
#define IS_DBCS(d)	(IS_LEFT(d) || IS_RIGHT(d))
#define MAKE_LEFT(b)	{ \
	if (((b) % COLS) == ((ROWS * COLS) - 1)) \
		ea_buf[(b)].db = DBCS_LEFT_WRAP; \
	else \
		ea_buf[(b)].db = DBCS_LEFT; \
}
#define MAKE_RIGHT(b)	{ \
	if (!((b) % COLS)) \
		ea_buf[(b)].db = DBCS_RIGHT_WRAP; \
	else \
		ea_buf[(b)].db = DBCS_RIGHT; \
}
#define SOSI(c)	(((c) == EBC_so)? EBC_si: EBC_so)

enum dbcs_why { DBCS_FIELD, DBCS_SUBFIELD, DBCS_ATTRIBUTE };

#if defined(X3270_DBCS) /*[*/
enum dbcs_state ctlr_dbcs_state(int baddr);
extern enum dbcs_state ctlr_lookleft_state(int baddr, enum dbcs_why *why);
int ctlr_dbcs_postprocess(void);
#else /*][*/
#define ctlr_dbcs_state(b)		DBCS_NONE
#define ctlr_lookleft_state(b, w)	DBCS_NONE
#define ctlr_dbcs_postprocess()		0
#endif /*]*/
