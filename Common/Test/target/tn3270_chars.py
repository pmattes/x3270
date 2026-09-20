#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
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
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# x3270 test target host, EBCDIC character screen.

import logging

from ds import *
from ibm3270ds import *
import oopts
import socketwrapper
import tn3270
import tn3270e_proto

def _ebcdic(text: str) -> bytes:
    '''Encode screen labels in EBCDIC.'''
    return text.encode('cp037')

def build_screen(graphic_escape: bool, numeric_order: bool = False) -> bytes:
    '''Build the EBCDIC character screen.'''
    mode = 'Graphic Escape' if graphic_escape else 'Normal'
    ret = bytes([command.erase_write, wcc.keyboard_restore | wcc.reset])
    ret += sba_bytes(1, 1, 80) + _ebcdic(f'EBCDIC characters - {mode}')
    ret += sba_bytes(3, 1, 80)
    ret += bytes([order.sf, fa.protect | fa.high_sel])
    if numeric_order:
        ret += _ebcdic('    -0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -A -B -C -D -E -F')
        row_digits = range(4, 16)
        column_digits = range(16)
    else:
        ret += _ebcdic('    4- 5- 6- 7- 8- 9- A- B- C- D- E- F-')
        row_digits = range(16)
        column_digits = range(4, 16)

    for row_digit in row_digits:
        row = row_digit - row_digits.start + 4
        ret += sba_bytes(row, 1, 80)
        ret += bytes([order.sf, fa.protect | fa.high_sel])
        ret += _ebcdic(f'{"-" if not numeric_order else ""}{row_digit:X}{"-" if numeric_order else ""}')
        ret += sba_bytes(row, 5, 80) + bytes([order.sf, fa.protect])
        for column_digit in column_digits:
            high = row_digit if numeric_order else column_digit
            low = column_digit if numeric_order else row_digit
            character = bytes([(high << 4) | low])
            if graphic_escape:
                ret += bytes([0x08])
            ret += character
            ret += bytes([0x40, 0x40])

    ret += sba_bytes(24, 1, 80) + bytes([order.sf, fa.protect])
    ret += _ebcdic('F3=END     F8=GE/NORMAL F9=IBM/SEQUENTIAL')
    ret += sba_bytes(24, 80, 80) + bytes([order.ic])
    return ret

class chars(tn3270.tn3270_server):
    '''TN3270 protocol server for EBCDIC character testing.'''

    graphic_escape = False
    numeric_order = False

    def __init__(self, conn: socketwrapper.socketwrapper,
                 logger: logging.Logger, peername: str, tls: bool,
                 switch, opts: oopts.oopts):
        '''Initialize the character screen.'''
        super().__init__(conn, logger, peername, tls, switch, opts)

    def rcv_data_cooked(self, data: bytes,
                        mode=tn3270e_proto.data_type.d3270_data):
        '''Consume terminal input.'''
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270:
            return
        if len(data) == 0:
            self.homescreen()
            return
        match data[0]:
            case aid.PF8.value:
                self.graphic_escape = not self.graphic_escape
                self.homescreen()
            case aid.PF9.value:
                self.numeric_order = not self.numeric_order
                self.homescreen()
            case aid.PF3.value:
                self.hangup()
            case _:
                self.homescreen()

    def start3270(self):
        '''Start 3270 mode.'''
        self.homescreen()

    def homescreen(self):
        '''Display the character screen.'''
        self.send_host(build_screen(self.graphic_escape, self.numeric_order))
