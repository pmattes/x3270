/*
 * Copyright 1995, 1999, 2000, 2002, 2003, 2005 by Paul Mattes.
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
