#!/usr/bin/env python3
#
# Copyright (c) 2023 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# x3270 test target host, 3270 data stream definitions.

import enum

class aid(enum.IntEnum):
    NO = 0x60,
    QREPLY = 0x61,
    ENTER = 0x7d,
    PF1 = 0xf1,
    PF2 = 0xf2,
    PF3 = 0xf3,
    PF4 = 0xf4,
    PF5 = 0xf5,
    PF6 = 0xf6,
    PF7 = 0xf7,
    PF8 = 0xf8,
    PF9 = 0xf9,
    PF10 = 0x7a,
    PF11 = 0x7b,
    PF12 = 0x7c,
    PF13 = 0xc1,
    PF14 = 0xc2,
    PF15 = 0xc3,
    PF16 = 0xc4,
    PF17 = 0xc5,
    PF18 = 0xc6,
    PF19 = 0xc7,
    PF20 = 0xc8,
    PF21 = 0xc9,
    PF22 = 0x4a,
    PF23 = 0x4b,
    PF24 = 0x4c,
    OICR = 0xe6,
    MSR_MHS = 0xe7,
    SELECT = 0x7e,
    PA1 = 0x6c,
    PA2 = 0x6e,
    PA3 = 0x6b,
    CLEAR = 0x6d,
    SYSREQ = 0xf0,
    SF_QREPLY = 0x81,
    SF = 0x88

class order(enum.IntEnum):
    sba = 0x11,
    ic = 0x13,
    sf = 0x1d,
    sfe = 0x29,

class command(enum.IntEnum):
    erase_write_alternate = 0x7e,
    write = 0xf1
    write_structured_field = 0xf3,
    erase_write = 0xf5,

class wcc(enum.IntFlag):
    reset = 0x40,
    keyboard_restore = 0x02,
    sound_alarm = 0x04,

class fa(enum.IntFlag):
    printable = 0xc0,
    protect = 0x20,
    numeric = 0x10,
    normal_nonsel = 0x00,
    normal_sel = 0x04,
    high_sel = 0x08,
    zero_nonsel = 0x0c,
    modify = 0x01

class sf(enum.IntEnum):
    read_partition = 0x01

class sf_rp(enum.IntEnum):
    query = 0x02

class qr(enum.IntEnum):
    usable_area = 0x81
    rpq_names = 0xa1

class xa(enum.IntEnum):
    m3270 = 0xc0,
    input_control = 0xfe,

class binpresz(enum.IntEnum):
    '''Presentation size from https://www.ibm.com/docs/en/zos/2.1.0?topic=image-bind-area-format-dsect'''
    binpsz0 = 0x00,     # Undefined
    binpsz1 = 0x01,     # 12x40
    binpsz2 = 0x02,     # 24x80
    binpsz3 = 0x03,     # 24x80 default, alt from Query Reply
    binpsfx = 0x7e,     # Fixed size using defaults
    binpszrc = 0x7f     # Default and alt specified by preceding fields
