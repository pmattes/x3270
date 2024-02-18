/*
 * Copyright (c) 1993-2023 Paul Mattes.
 * Copyright (c) 2004, Don Russell.
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
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC nor
 *       the names of their contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND GTRC
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, DON RUSSELL, JEFF
 * SPARKES OR GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	3270ds.h
 *
 *		Header file for the 3270 Data Stream Protocol.
 */

/* 3270 commands */
#define CMD_W		0x01	/* write */
#define CMD_RB		0x02	/* read buffer */
#define CMD_NOP		0x03	/* no-op */
#define CMD_EW		0x05	/* erase/write */
#define CMD_RM		0x06	/* read modified */
#define CMD_EWA		0x0d	/* erase/write alternate */
#define CMD_RMA		0x0e	/* read modified all */
#define CMD_EAU		0x0f	/* erase all unprotected */
#define CMD_WSF		0x11	/* write structured field */

/* SNA 3270 commands */
#define SNA_CMD_RMA	0x6e	/* read modified all */
#define SNA_CMD_EAU	0x6f	/* erase all unprotected */
#define SNA_CMD_EWA	0x7e	/* erase/write alternate */
#define SNA_CMD_W	0xf1	/* write */
#define SNA_CMD_RB	0xf2	/* read buffer */
#define SNA_CMD_WSF	0xf3	/* write structured field */
#define SNA_CMD_EW	0xf5	/* erase/write */
#define SNA_CMD_RM	0xf6	/* read modified */

/* 3270 orders */
#define ORDER_PT	0x05	/* program tab */
#define ORDER_GE	0x08	/* graphic escape */
#define ORDER_SBA	0x11	/* set buffer address */
#define ORDER_EUA	0x12	/* erase unprotected to address */
#define ORDER_IC	0x13	/* insert cursor */
#define ORDER_SF	0x1d	/* start field */
#define ORDER_SA	0x28	/* set attribute */
#define ORDER_SFE	0x29	/* start field extended */
#define ORDER_YALE	0x2b	/* Yale sub command */
#define ORDER_MF	0x2c	/* modify field */
#define ORDER_RA	0x3c	/* repeat to address */

#define FCORDER_NULL	0x00	/* format control: null */
#define FCORDER_FF	0x0c	/*		   form feed */
#define FCORDER_CR	0x0d	/*		   carriage return */
#define FCORDER_SO	0x0e	/*                 shift out (DBCS subfield) */
#define FCORDER_SI	0x0f	/*                 shift in (DBCS end) */
#define FCORDER_NL	0x15	/*		   new line */
#define FCORDER_EM	0x19	/*		   end of medium */
#define FCORDER_LF	0x25	/*                 line feed */
#define FCORDER_DUP	0x1c	/*		   duplicate */
#define FCORDER_FM	0x1e	/*		   field mark */
#define FCORDER_SUB	0x3f	/*		   substitute */
#define FCORDER_EO	0xff	/*		   eight ones */

/* SCS control code, some overlap orders */
#define SCS_BS      	0x16	/* Back Space  */
#define SCS_BEL		0x2f	/* Bell Function */
#define SCS_CR      	0x0d	/* Carriage Return */
#define SCS_ENP		0x14	/* Enable Presentation */
#define SCS_FF		0x0c	/* Forms Feed */
#define SCS_GE		0x08	/* Graphic Escape */
#define SCS_HT		0x05	/* Horizontal Tab */
#define SCS_INP		0x24	/* Inhibit Presentation */
#define SCS_IRS		0x1e	/* Interchange-Record Separator */
#define SCS_LF		0x25	/* Line Feed */
#define SCS_NL		0x15	/* New Line */
#define SCS_SA		0x28	/* Set Attribute: */
#define  SCS_SA_RESET	0x00	/*  Reset all */
#define  SCS_SA_HIGHLIGHT 0x41	/*  Highlighting */
#define  SCS_SA_CS	0x42	/*  Character set */
#define  SCS_SA_GRID	0xc2	/*  Grid */
#define SCS_SET		0x2b	/* Set: */
#define  SCS_SHF	0xc1	/*  Horizontal format */
#define  SCS_SLD	0xc6	/*  Line Density */
#define  SCS_SVF	0xc2	/*  Vertical Format */
#define SCS_SO		0x0e	/* Shift out (DBCS subfield start) */
#define SCS_SI		0x0f	/* Shift in (DBCS subfield end) */
#define SCS_TRN		0x35	/* Transparent */
#define SCS_VCS		0x04	/* Vertical Channel Select */
#define SCS_VT		0x0b	/* Vertical Tab */

/* Structured fields */
#define SF_READ_PART	0x01	/* read partition */
#define  SF_RP_QUERY	0x02	/*  query */
#define  SF_RP_QLIST	0x03	/*  query list */
#define   SF_RPQ_LIST	0x00	/*   QCODE list */
#define   SF_RPQ_EQUIV	0x40	/*   equivalent+ QCODE list */
#define   SF_RPQ_ALL	0x80	/*   all */
#define SF_ERASE_RESET	0x03	/* erase/reset */
#define  SF_ER_DEFAULT	0x00	/*  default */
#define  SF_ER_ALT	0x80	/*  alternate */
#define SF_SET_REPLY_MODE 0x09	/* set reply mode */
#define  SF_SRM_FIELD	0x00	/*  field */
#define  SF_SRM_XFIELD	0x01	/*  extended field */
#define  SF_SRM_CHAR	0x02	/*  character */
#define SF_CREATE_PART	0x0c	/* create partition */
#define  CPFLAG_PROT	0x40	/*  protected flag */
#define  CPFLAG_COPY_PS	0x20	/*  local copy to presentation space */
#define  CPFLAG_BASE	0x07	/*  base character set index */
#define SF_OUTBOUND_DS	0x40	/* outbound 3270 DS */
#define SF_TRANSFER_DATA 0xd0   /* file transfer open request */

/* Query replies */
#define QR_SUMMARY	0x80	/* summary */
#define QR_USABLE_AREA	0x81	/* usable area */
#define QR_IMAGE	0x82	/* image */
#define QR_TEXT_PART	0x83	/* text partitions */
#define QR_ALPHA_PART	0x84	/* alphanumeric partitions */
#define QR_CHARSETS	0x85	/* character sets */
#define QR_COLOR	0x86	/* color */
#define QR_HIGHLIGHTING	0x87	/* highlighting */
#define QR_REPLY_MODES	0x88	/* reply modes */
#define QR_FIELD_VAL	0x8a	/* field validation */
#define QR_MSR_CTL	0x8b	/* MSR control */
#define QR_OUTLINING	0x8c	/* field outlining */
#define QR_PART_CHAR	0x8e	/* partition characteristics */
#define QR_OEM_AUX	0x8f	/* OEM auxiliary device */
#define QR_FMT_PRES	0x90	/* format presentation */
#define QR_DBCS_ASIA	0x91	/* DBCS-Asia */
#define QR_SAVE_RESTORE	0x92	/* save/restore format */
#define QR_PC3270	0x93    /* PC3270 */
#define QR_FMT_SAD	0x94    /* format storage auxiliary device */
#define QR_DDM    	0x95    /* distributed data management */
#define QR_STG_POOLS   	0x96    /* storage pools */
#define QR_DIA   	0x97    /* document interchange architecture */
#define QR_DATA_CHAIN  	0x98    /* data chaining */
#define QR_AUX_DEVICE	0x99	/* auxiliary device */
#define QR_3270_IPDS	0x9a	/* 3270 IPDS */
#define QR_PDDS		0x9c	/* product defined data stream */
#define QR_IBM_AUX	0x9e	/* IBM auxiliary device */
#define QR_BEGIN_EOF	0x9f	/* begin/end of file */
#define QR_DEVICE_CHAR	0xa0	/* device characteristics */
#define QR_RPQNAMES	0xa1	/* RPQ names */
#define QR_DATA_STREAMS	0xa2	/* data streams */
#define QR_IMP_PART	0xa6	/* implicit partition */
#define QR_PAPER_FEED	0xa7	/* paper feed techniques */
#define QR_TRANSPARENCY	0xa8	/* transparency */
#define QR_SPC		0xa9	/* settable printer characteristics */
#define QR_IOCA_AD	0xaa	/* IOCA auxiliary device */
#define QR_CPR		0xab	/* cooperative proc. requestor */
#define QR_SEGMENT	0xb0	/* segment */
#define QR_PROCEDURE	0xb1	/* procedure */
#define QR_LINE_TYPE	0xb2	/* line type */
#define QR_PORT		0xb3	/* port */
#define QR_GCOLOR	0xb4	/* graphic color */
#define QR_XDR		0xb5	/* extended drawing routine */
#define QR_GSS		0xb6	/* graphic symbol sets */
#define QR_NULL		0xff	/* null */

#define BA_TO_ROW(ba)		((ba) / COLS)
#define BA_TO_COL(ba)		((ba) % COLS)
#define ROWCOL_TO_BA(r,c)	(((r) * COLS) + c)
#define INC_BA(ba)		{ (ba) = ((ba) + 1) % (COLS * ROWS); }
#define DEC_BA(ba)		{ (ba) = (ba) ? (ba - 1) : ((COLS*ROWS) - 1); }

/* Field attributes. */
#define FA_PRINTABLE	0xc0	/* these make the character "printable" */
#define FA_PROTECT	0x20	/* unprotected (0) / protected (1) */
#define FA_NUMERIC	0x10	/* alphanumeric (0) /numeric (1) */
#define FA_INTENSITY	0x0c	/* display/selector pen detectable: */
#define FA_INT_NORM_NSEL 0x00	/*  00 normal, non-detect */
#define FA_INT_NORM_SEL	 0x04	/*  01 normal, detectable */
#define FA_INT_HIGH_SEL	 0x08	/*  10 intensified, detectable */
#define FA_INT_ZERO_NSEL 0x0c	/*  11 nondisplay, non-detect */
#define FA_RESERVED	0x02	/* must be 0 */
#define FA_MODIFY	0x01	/* modified (1) */

/* Bits in the field attribute that are stored. */
#define FA_MASK		(FA_PROTECT | FA_NUMERIC | FA_INTENSITY | FA_MODIFY)

/* Tests for various attribute properties. */
#define FA_IS_MODIFIED(c)	((c) & FA_MODIFY)
#define FA_IS_NUMERIC(c)	((c) & FA_NUMERIC)
#define FA_IS_PROTECTED(c)	((c) & FA_PROTECT)
#define FA_IS_SKIP(c)		(((c) & FA_PROTECT) && ((c) & FA_NUMERIC))

#define FA_IS_ZERO(c)					\
	(((c) & FA_INTENSITY) == FA_INT_ZERO_NSEL)
#define FA_IS_HIGH(c)					\
	(((c) & FA_INTENSITY) == FA_INT_HIGH_SEL)
#define FA_IS_NORMAL(c)					\
    (							\
	((c) & FA_INTENSITY) == FA_INT_NORM_NSEL	\
	||						\
	((c) & FA_INTENSITY) == FA_INT_NORM_SEL		\
    )
#define FA_IS_SELECTABLE(c)				\
    (							\
	((c) & FA_INTENSITY) == FA_INT_NORM_SEL		\
	||						\
	((c) & FA_INTENSITY) == FA_INT_HIGH_SEL		\
    )
#define FA_IS_INTENSE(c)				\
	((c & FA_INT_HIGH_SEL) == FA_INT_HIGH_SEL)

/* Extended attributes */
#define XA_ALL		0x00
#define XA_3270		0xc0
#define XA_VALIDATION	0xc1
#define  XAV_FILL	0x04
#define  XAV_ENTRY	0x02
#define  XAV_TRIGGER	0x01
#define XA_OUTLINING	0xc2
#define  XAO_UNDERLINE	0x01
#define  XAO_RIGHT	0x02
#define  XAO_OVERLINE	0x04
#define  XAO_LEFT	0x08
#define XA_HIGHLIGHTING	0x41
#define  XAH_DEFAULT	0x00
#define  XAH_NORMAL	0xf0
#define  XAH_BLINK	0xf1
#define  XAH_REVERSE	0xf2
#define  XAH_UNDERSCORE	0xf4
#define  XAH_INTENSIFY	0xf8
#define XA_FOREGROUND	0x42
#define  XAC_DEFAULT	0x00
#define XA_CHARSET	0x43
#define XA_BACKGROUND	0x45
#define XA_TRANSPARENCY	0x46
#define  XAT_DEFAULT	0x00
#define  XAT_OR		0xf0
#define  XAT_XOR	0xf1
#define  XAT_OPAQUE	0xff
#define XA_INPUT_CONTROL 0xfe
#define  XAI_DISABLED	0x00
#define  XAI_ENABLED	0x01

/* WCC definitions */
#define WCC_RESET_BIT		0x40
#define WCC_START_PRINTER_BIT	0x08
#define WCC_SOUND_ALARM_BIT	0x04
#define WCC_KEYBOARD_RESTORE_BIT 0x02
#define WCC_RESET_MDT_BIT	0x01
#define WCC_RESET(c)		((c) & WCC_RESET_BIT)
#define WCC_START_PRINTER(c)	((c) & WCC_START_PRINTER_BIT)
#define WCC_SOUND_ALARM(c)	((c) & WCC_SOUND_ALARM_BIT)
#define WCC_KEYBOARD_RESTORE(c)	((c) & WCC_KEYBOARD_RESTORE_BIT)
#define WCC_RESET_MDT(c)	((c) & WCC_RESET_MDT_BIT)

/* AIDs */
#define AID_NO		0x60	/* no AID generated */
#define AID_QREPLY	0x61
#define AID_ENTER	0x7d
#define AID_PF1		0xf1
#define AID_PF2		0xf2
#define AID_PF3		0xf3
#define AID_PF4		0xf4
#define AID_PF5		0xf5
#define AID_PF6		0xf6
#define AID_PF7		0xf7
#define AID_PF8		0xf8
#define AID_PF9		0xf9
#define AID_PF10	0x7a
#define AID_PF11	0x7b
#define AID_PF12	0x7c
#define AID_PF13	0xc1
#define AID_PF14	0xc2
#define AID_PF15	0xc3
#define AID_PF16	0xc4
#define AID_PF17	0xc5
#define AID_PF18	0xc6
#define AID_PF19	0xc7
#define AID_PF20	0xc8
#define AID_PF21	0xc9
#define AID_PF22	0x4a
#define AID_PF23	0x4b
#define AID_PF24	0x4c
#define AID_OICR	0xe6
#define AID_MSR_MHS	0xe7
#define AID_SELECT	0x7e
#define AID_PA1		0x6c
#define AID_PA2		0x6e
#define AID_PA3		0x6b
#define AID_CLEAR	0x6d
#define AID_SYSREQ	0xf0

#define AID_SF		0x88
#define SFID_QREPLY	0x81

/* Colors */
#define HOST_COLOR_NEUTRAL_BLACK	0
#define HOST_COLOR_BLUE			1
#define HOST_COLOR_RED			2
#define HOST_COLOR_PINK			3
#define HOST_COLOR_GREEN		4
#define HOST_COLOR_TURQUOISE		5
#define HOST_COLOR_YELLOW		6
#define HOST_COLOR_NEUTRAL_WHITE	7
#define HOST_COLOR_BLACK		8
#define HOST_COLOR_DEEP_BLUE		9
#define HOST_COLOR_ORANGE		10
#define HOST_COLOR_PURPLE		11
#define HOST_COLOR_PALE_GREEN		12
#define HOST_COLOR_PALE_TURQUOISE	13
#define HOST_COLOR_GREY			14
#define HOST_COLOR_WHITE		15

/* Data stream manipulation macros. */
#define MASK32	0xff000000U
#define MASK24	0x00ff0000U
#define MASK16	0x0000ff00U
#define MASK08	0x000000ffU
#define MINUS1	0xffffffffU

#define SET16(ptr, val) { \
	*((ptr)++) = (unsigned char)(((val) & MASK16) >> 8); \
	*((ptr)++) = (unsigned char)(((val) & MASK08)); \
}
#define GET16(val, ptr) { \
	(val) = *((ptr)+1); \
	(val) += *(ptr) << 8; \
}
#define SET32(ptr, val) { \
	*((ptr)++) = (unsigned char)(((val) & MASK32) >> 24); \
	*((ptr)++) = (unsigned char)(((val) & MASK24) >> 16); \
	*((ptr)++) = (unsigned char)(((val) & MASK16) >> 8); \
	*((ptr)++) = (unsigned char)(((val) & MASK08)); \
}
#define HIGH8(s)        (((s) >> 8) & 0xff)
#define LOW8(s)         ((s) & 0xff)

/* Other EBCDIC control codes. */
#define EBC_null	0x00
#define EBC_soh		0x01
#define EBC_stx		0x02
#define EBC_ff		0x0c
#define EBC_cr		0x0d
#define EBC_so		0x0e
#define EBC_si		0x0f
#define EBC_nl		0x15
#define EBC_em		0x19
#define EBC_dup		0x1c
#define EBC_fm		0x1e
#define EBC_sub		0x3f
#define EBC_space	0x40
#define EBC_plus	0x4e
#define EBC_nobreakspace 0x41   
#define EBC_period	0x4b    
#define EBC_ampersand	0x50    
#define EBC_minus	0x60
#define EBC_slash	0x61
#define EBC_comma	0x6b
#define EBC_percent	0x6c
#define EBC_underscore	0x6d
#define EBC_greater	0x6e    
#define EBC_question	0x6f    
#define EBC_Yacute	0xad
#define EBC_diaeresis	0xbd
#define EBC_0		0xf0
#define EBC_1		0xf1
#define EBC_2		0xf2
#define EBC_3		0xf3
#define EBC_4		0xf4
#define EBC_5		0xf5
#define EBC_6		0xf6
#define EBC_7		0xf7
#define EBC_8		0xf8
#define EBC_9		0xf9
#define EBC_A		0xc1
#define EBC_B		0xc2
#define EBC_C		0xc3
#define EBC_D		0xc4
#define EBC_E		0xc5
#define EBC_F		0xc6
#define EBC_G		0xc7
#define EBC_H		0xc8
#define EBC_I		0xc9
#define EBC_J		0xd1
#define EBC_K		0xd2
#define EBC_L		0xd3
#define EBC_M		0xd4
#define EBC_N		0xd5
#define EBC_O		0xd6
#define EBC_P		0xd7
#define EBC_Q		0xd8
#define EBC_R		0xd9
#define EBC_S		0xe2
#define EBC_T		0xe3
#define EBC_U		0xe4
#define EBC_V		0xe5
#define EBC_eo		0xff
#define EBC_less	0x4c
#define EBC_greaer	0x6e

/* Unicode private-use definitions. */
#define UPRIV_GE_00	0xf700	/* first GE */
#define UPRIV_GE_ff	0xf7ff	/* last GE */
#define UPRIV_sub	0xf8fc
#define UPRIV_eo	0xf8fd
#define UPRIV_fm	0xf8fe
#define UPRIV_dup	0xf8ff

/* Second set of PUA definitions. */
#define UPRIV2		0xe000
#define UPRIV2_dup	(UPRIV2 + '*')
#define UPRIV2_fm	(UPRIV2 + ';')
#define UPRIV2_Aunderbar	(UPRIV2 + 'A')
#define UPRIV2_Zunderbar	(UPRIV2 + 'Z')

/* BIND definitions. */
#define BIND_RU			0x31
#define BIND_OFF_MAXRU_SEC	10
#define BIND_OFF_MAXRU_PRI	11
#define BIND_OFF_RD		20
#define BIND_OFF_CD		21
#define BIND_OFF_RA		22
#define BIND_OFF_CA		23
#define BIND_OFF_SSIZE		24
#define BIND_OFF_PLU_NAME_LEN	27
#define BIND_PLU_NAME_MAX	8
#define BIND_OFF_PLU_NAME	28

/* Screen sizes. */
#define MODEL_2_ROWS		24
#define MODEL_2_COLS		80
#define MODEL_3_ROWS		32
#define MODEL_3_COLS		80
#define MODEL_4_ROWS		43
#define MODEL_4_COLS		80
#define MODEL_5_ROWS		27
#define MODEL_5_COLS		132

/* Largest combination of rows x columns. */
#define MAX_ROWS_COLS		0x3fff
