/*
 * Copyright (c) 1996-2023 Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft_private.h
 *		Private definitions for ft.c.
 */

typedef enum {
    HT_TSO,
    HT_VM,
    HT_CICS
} host_type_t;
bool ft_encode_host_type(const char *s, host_type_t *ht);
const char *ft_decode_host_type(host_type_t ht);

typedef enum {
    DEFAULT_RECFM,
    RECFM_FIXED,
    RECFM_VARIABLE,
    RECFM_UNDEFINED
} recfm_t;
bool ft_encode_recfm(const char *s, recfm_t *recfm);
const char *ft_decode_recfm(recfm_t recfm);

typedef enum {
    DEFAULT_UNITS,
    TRACKS,
    CYLINDERS,
    AVBLOCK
} units_t;
bool ft_encode_units(const char *s, units_t *units);
const char *ft_decode_units(units_t units);

typedef struct {
    /* User-specified parameters. */
    char *host_filename;
    char *local_filename;
    bool receive_flag;
    bool append_flag;
    host_type_t host_type;
    bool ascii_flag;
    bool cr_flag;
    bool remap_flag;
    recfm_t recfm;
    units_t units;
    bool allow_overwrite;
    int lrecl;
    int blksize;
    int primary_space;
    int secondary_space;
    int avblock;
    int dft_buffersize;
#if defined(_WIN32) /*[*/
    int windows_codepage;
#endif /*]*/
    char *other_options;

    /* Invocation state. */
    bool is_action;
} ft_conf_t;
extern ft_conf_t *ftc;

char *ft_resolve_dir(ft_conf_t *p);
FILE *ft_go(ft_conf_t *p, enum iaction cause);
void ft_init_conf(ft_conf_t *p);
bool ft_start_backend(ft_conf_t *p, enum iaction cause);

/* Transient state. */
typedef struct {
    char *resolved_local_filename;
    FILE *local_file;
    size_t length;
    bool is_cut;
    bool last_dbcs;
    bool last_cr;
    enum ftd {
	FT_DBCS_NONE,
	FT_DBCS_SO,
	FT_DBCS_LEFT
    } dbcs_state;
    unsigned char dbcs_byte1;
} ft_tstate_t;
extern ft_tstate_t fts;

#define __FT_PRIVATE_H
