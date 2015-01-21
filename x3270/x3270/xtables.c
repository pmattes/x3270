/*
 * Copyright (c) 1993-2009, 2015 Paul Mattes.
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
 *	tables.c
 *		Translation tables between ASCII (actually ISO 8859-1), EBCDIC
 *		code page 37 and the special 3270 Character Generator character
 *		set.
 */

#include "globals.h"
#include "xtables.h"

const unsigned char asc2cg0[256] = {
/*00*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*08*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*10*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*18*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*20*/	0x10, 0x19, 0x13, 0x2c, 0x1a, 0x2e, 0x30, 0x12,
/*28*/	0x0d, 0x0c, 0xbf, 0x35, 0x33, 0x31, 0x32, 0x14,
/*30*/	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
/*38*/	0x28, 0x29, 0x34, 0xbe, 0x09, 0x11, 0x08, 0x18,
/*40*/	0x2d, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
/*48*/	0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
/*50*/	0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
/*58*/	0xb7, 0xb8, 0xb9, 0x0a, 0x15, 0x0b, 0x3a, 0x2f,
/*60*/	0x3d, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
/*68*/	0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
/*70*/	0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
/*78*/	0x97, 0x98, 0x99, 0x0f, 0x16, 0x0e, 0x3b, 0x00,
/*80*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*88*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*90*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*98*/	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*a0*/	0x01, 0x6e, 0x1b, 0x1c, 0x1f, 0x1d, 0x17, 0x2b,
/*a8*/	0x3c, 0xd0, 0x6a, 0x6c, 0x36, 0x07, 0xd1, 0x37,
/*b0*/	0x38, 0xd6, 0x68, 0x69, 0x3e, 0x54, 0x1e, 0x39,
/*b8*/	0x3f, 0x67, 0x6b, 0x6d, 0x4b, 0x4c, 0x4d, 0x6f,
/*c0*/	0x60, 0x7a, 0x75, 0x65, 0x70, 0xbc, 0xba, 0xbd,
/*c8*/	0x61, 0x7b, 0x76, 0x71, 0x62, 0x7c, 0x77, 0x72,
/*d0*/	0xd7, 0x7f, 0x63, 0x7d, 0x78, 0x66, 0x73, 0x5b,
/*d8*/	0xbb, 0x64, 0x7e, 0x79, 0x74, 0x48, 0xd9, 0x2a,
/*e0*/	0x40, 0x5a, 0x55, 0x45, 0x50, 0x9c, 0x9a, 0x4f,
/*e8*/	0x41, 0x4a, 0x56, 0x51, 0x42, 0x5c, 0x57, 0x52,
/*f0*/	0xf7, 0x5f, 0x43, 0x5d, 0x58, 0x46, 0x53, 0x9d,
/*f8*/	0x9b, 0x44, 0x5e, 0x59, 0x4e, 0x49, 0xf9, 0x47
};
const unsigned char ebc2cg0[256] = {
/*00*/	0x00, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
/*08*/	0xdf, 0xdf, 0xdf, 0xdf, 0x02, 0x03, 0x00, 0x00,
/*10*/	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0x04, 0xdf, 0xdf,
/*18*/	0xdf, 0x05, 0xdf, 0xdf, 0x9f, 0xdf, 0x9e, 0xdf,
/*20*/	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
/*28*/	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
/*30*/	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
/*38*/	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
/*40*/	0x10, 0x01, 0x55, 0x50, 0x40, 0x5a, 0x45, 0x9c,
/*48*/	0x4f, 0x5f, 0x1b, 0x32, 0x09, 0x0d, 0x35, 0x16,
/*50*/	0x30, 0x4a, 0x56, 0x51, 0x41, 0x5c, 0x57, 0x52,
/*58*/	0x42, 0x2a, 0x19, 0x1a, 0xbf, 0x0c, 0xbe, 0x36,
/*60*/	0x31, 0x14, 0x75, 0x70, 0x60, 0x7a, 0x65, 0xbc,
/*68*/	0xbd, 0x7f, 0x17, 0x33, 0x2e, 0x2f, 0x08, 0x18,
/*70*/	0x9b, 0x7b, 0x76, 0x71, 0x61, 0x7c, 0x77, 0x72,
/*78*/	0x62, 0x3d, 0x34, 0x2c, 0x2d, 0x12, 0x11, 0x13,
/*80*/	0xbb, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
/*88*/	0x87, 0x88, 0x6c, 0x6d, 0xf7, 0x49, 0xf9, 0xd6,
/*90*/	0x38, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
/*98*/	0x90, 0x91, 0x6a, 0x6b, 0x9a, 0x3f, 0xba, 0x1f,
/*a0*/	0x54, 0x3b, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
/*a8*/	0x98, 0x99, 0x6e, 0x6f, 0xd7, 0x48, 0xd9, 0xd1,
/*b0*/	0x3a, 0x1c, 0x1d, 0x39, 0xd0, 0x2b, 0x1e, 0x4b,
/*b8*/	0x4c, 0x4d, 0x0a, 0x0b, 0x37, 0x3c, 0x3e, 0x5b,
/*c0*/	0x0f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
/*c8*/	0xa7, 0xa8, 0x07, 0x58, 0x53, 0x43, 0x5d, 0x46,
/*d0*/	0x0e, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
/*d8*/	0xb0, 0xb1, 0x67, 0x59, 0x4e, 0x44, 0x5e, 0x47,
/*e0*/	0x15, 0x9d, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
/*e8*/	0xb8, 0xb9, 0x68, 0x78, 0x73, 0x63, 0x7d, 0x66,
/*f0*/	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
/*f8*/	0x28, 0x29, 0x69, 0x79, 0x74, 0x64, 0x7e, 0x06 };