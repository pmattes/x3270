/*
 * Copyright (c) 2016, 2022 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ui_stream.h
 *		A UI back-end for a 3270 Terminal Emulator
 *		UI data stream I/O.
 */

#define JSON_MODE	(appres.b3270.json)
#define XML_MODE	(!appres.b3270.json)

/* Attribute types. */
typedef enum {
    AT_STRING,
    AT_INT,
    AT_SKIP_INT,	/* ignore an integer value */
    AT_DOUBLE,
    AT_BOOLEAN,
    AT_SKIP_BOOLEAN,	/* ignore a Boolean value */
    AT_NODE		/* JSON node */
} ui_attr_t;

/* Common functions. */
void ui_io_init(void);
void ui_leaf(const char *name, ...);
void ui_add_element(const char *name, ui_attr_t attr, ...);

/* XML-specific functions. */
void uix_pop(void);
void uix_push(const char *name, ...);
void uix_open_leaf(const char *name);
void uix_close_leaf(void);

/* JSON-specific functions. */
void uij_open_object(const char *name);
void uij_open_array(const char *name);
void uij_close_object(void);
void uij_close_array(void);
