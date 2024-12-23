#!/usr/bin/env python3
#
# Copyright (c) 2022-2024 Paul Mattes.
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
# x3270 test target host, data stream helper.

import sys
from typing import List, Tuple

from ibm3270ds import *

# 12-bit address encoding table
code_table = [
    0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
]

def sba_bytes(row1: int, col1: int, columns: int) -> bytes:
    '''Compute an SBA order'''
    b = ((row1 - 1) * columns) + (col1 - 1)
    if (b >= 0x1000):
        return bytes([order.sba.value, (b >> 8) & 0x3f, b & 0xff])
    else:
        return bytes([order.sba.value, code_table[b >> 6], code_table[b & 0x3f]])

def sba(row1: int, col1: int, columns: int) -> str:
    '''Compute an SBA order'''
    return sba_bytes(row1, col1, columns).hex()

def isblank(field: str) -> bool:
    '''Check for all blanks, nulls or underscores'''
    for ch in field:
        if not ch in [' ', '\0', '_']:
            return False
    return True

def decode_address(addr: bytes) -> int:
    '''Decode an address'''
    return (code_table.index(addr[0]) << 6) | code_table.index(addr[1])

def baddr(row1: int, col1: int, columns: int) -> int:
    '''Translate 1-origin row and column to a buffer address'''
    return ((row1 - 1) * columns) + (col1 - 1)

def get_field(buffer: List[int], row1: int, col1: int, columns: int, length: int, underscore=True, pad=True, upper=True) -> str:
    '''Extract a field from a buffer'''
    b = baddr(row1, col1, columns)
    field = buffer[b : b + length]
    # Remove NUL characters.
    # Remove leading spaces and underscores.
    # Truncate after the first space or underscore.
    # Pad with '_' on the right to recover the length.
    while 0 in field:
        field.remove(0)
    if underscore:
        field = [0x40 if ch == 0x6d else ch for ch in field] # translate '_' to ' '
    while len(field) > 0 and field[0] == 0x40:
        field.remove(0x40)
    if 0x40 in field:
        field = field[0 : field.index(0x40)]
    while len(field) > 0 and field[-1] == 0x40:
        field = field[0 : len(field) - 1]
    if underscore:
        while len(field) < length:
            field.append(0x6d)
    elif pad:
        while len(field) < length:
            field.append(0x00)
    ret = bytes(field).decode('cp037')
    if upper:
        ret = ret.upper()
    return ret

rows = {
    '2': 24,
    '3': 32,
    '4': 43,
    '5': 27
}
columns = {
    '2': 80,
    '3': 80,
    '4': 80,
    '5': 132
}

class dinfo():
    '''Display information'''
    def __init__(self, ttype: str):
        self.ttype = ttype
        self.dynamic = self.ttype == 'IBM-DYNAMIC'
        self.extended = self.ttype.endswith('-E') or self.dynamic
        self.rpqnames = None
        if not self.dynamic:
            self.model = self.ttype[9]
            self.alt_rows = rows[self.model]
            self.alt_columns = columns[self.model]
        else:
            # Temporary until QueryReply can be processed.
            self.model = '?'
            self.alt_rows = 24
            self.alt_columns = 80

    def parse_query_reply(self, b: bytes) -> Tuple[bool, str]:
        '''Parse a Query Reply'''
        if len(b) < 1 or b[0] != aid.SF.value:
            return (False, 'overall len or AID is wrong')
        b = b[1:]
        while len(b) > 0:
            # Get the length.
            if len(b) < 2:
                return (False, f'not enough buffer for subfield len ({len(b)})')
            field_len = b[0] << 8 | b[1]
            if field_len < 2 or len(b) < field_len:
                return (False, 'subfield len too small')
            if b[2] != aid.SF_QREPLY.value:
                return (False, 'subfield isn\'t QREPLY')
            if b[3] == qr.usable_area.value:
                # Usable area:
                #  +1 12/14-bit
                #  +2 special character features
                #  +3/+4 width
                #  +5/+6 height
                self.alt_columns = b[6] << 8 | b[7]
                self.alt_rows = b[8] << 8 | b[9]
            elif b[3] == qr.rpq_names.value:
                self.rpqnames = b[4:field_len]

            # Get the next field.
            b = b[field_len:]

        return (True, '')
