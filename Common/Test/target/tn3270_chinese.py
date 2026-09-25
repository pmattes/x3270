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
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# x3270 test target host, Chinese code-page test page.

import re

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

required_cgcsgid = 0x04380345
poem = bytes.fromhex('''
    0e 4f 4d 4a 67 5c 73 0f 6b 40 0e 56 75 65 99 0f
    40 4d 0e 44 62 0f e2 96 95 87 40 96 86 40 d9 89
    a5 85 99 40 c3 89 a3 a8 0e 44 72 0f 6b 40 82 a8
    40 e2 a4 40 e2 88 89 5d 25 25 0e 55 ad 52 e9 55
    9b 56 60 51 5d 52 43 52 43 42 6b 49 ba 56 5c 51
    5f 42 6b 5c 74 52 d0 57 9d 43 41 0f 25 0e 54 47
    50 ee 4d 62 4c 78 42 6b 57 dd 4a a5 4e 50 53 df
    51 59 43 41 0f 25 0e 5c 7d 55 b8 58 81 4c 8b 5a
    46 49 ba 55 b5 42 6b 4a 5e 51 f9 52 87 42 6b 6c
    7e 54 e6 56 4a 43 41 0f 25 0e 59 b8 50 b3 5a 64
    52 6e 4d f5 4e 59 58 88 42 6b 58 a0 58 f8 4a af
    42 6b 5b 9e 55 e0 5c 51 43 41 0f 25 0e 58 81 4d
    6b 57 dd 59 74 42 6b 57 a8 5a 70 50 e0 54 47 58
    cf 43 41 0f 25 0e 51 6f 4b 63 52 e9 52 e9 4a 46
    4b ce 4a a5 42 6b 52 98 5a c1 59 b8 42 6b 4b cb
    56 69 4c d3 43 41 0f 25
''')
linebreak = re.compile(rb'\x0d\x25|\r\n|[\x0a\x0d\x15\x25]')

# Split the EBCDIC text into physical screen lines.
def poem_lines(text: bytes) -> list[bytes]:
    lines = linebreak.split(text)
    if text and linebreak.search(text[-2:]):
        lines.pop()
    return lines

# Build the protected screen containing the pre-encoded poem.
def build_screen(text: bytes, rows: int, columns: int) -> bytes:
    lines = poem_lines(text)
    if rows < 2 or columns < 8:
        raise ValueError('terminal screen is too small')
    if len(lines) > rows - 1:
        raise ValueError('poem has more lines than the terminal can display')
    if any(len(line) > columns - 1 for line in lines):
        raise ValueError('a poem line is wider than the terminal screen')

    ret = bytes([command.erase_write_alternate, wcc.keyboard_restore | wcc.reset])
    for row in range(1, rows):
        ret += sba_bytes(row, 1, columns)
        attribute = fa.protect | fa.high_sel if row == 1 else fa.protect
        ret += bytes([order.sf, attribute])
        if row <= len(lines):
            ret += lines[row - 1]

    ret += sba_bytes(rows, 1, columns) + 'F3=END'.encode('cp037')
    ret += sba_bytes(rows, columns, columns) + bytes([order.ic])
    return ret

# Build an explanatory screen before returning to the caller.
def build_error_screen(message: str, rows: int, columns: int,
                       alternate: bool = False) -> bytes:
    write_command = command.erase_write_alternate if alternate else command.erase_write
    ret = bytes([write_command, wcc.keyboard_restore | wcc.reset])
    ret += sba_bytes(1, 1, columns) + bytes([order.sf, fa.protect | fa.high_sel])
    ret += message[:columns - 1].encode('cp037')
    ret += sba_bytes(rows, columns, columns) + bytes([order.ic])
    return ret

class chinese(tn3270.tn3270_server):
    '''TN3270 protocol server for displaying a CP935 Chinese poem.'''

    # Consume terminal input and validate the QueryReply charset information.
    def rcv_data_cooked(self, data: bytes,
                        mode=tn3270e_proto.data_type.d3270_data):
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270 or not data:
            return
        match data[0]:
            case aid.PF3.value:
                self.hangup()
            case aid.SF.value:
                result = self.dinfo.parse_query_reply(data)
                if not result[0]:
                    self.fail(f'Invalid terminal QueryReply: {result[1]}')
                elif self.dinfo.cgcsgid_dbcs != required_cgcsgid:
                    reported = self.dinfo.cgcsgid_dbcs
                    self.fail(f'Requires DBCS CGCSGID 0x{required_cgcsgid:08x}; '
                              f'terminal reported {reported!r}.')
                else:
                    self.display_poem()

    # Start charset negotiation or reject terminals that cannot report it.
    def start3270(self):
        if not self.dinfo.extended:
            self.fail('This page requires an extended terminal with DBCS '
                      'CGCSGID 0x04380345.')
            return
        self.query()

    # Display the embedded poem bytes without transcoding the CP935 content.
    def display_poem(self):
        try:
            screen = build_screen(poem, self.dinfo.alt_rows, self.dinfo.alt_columns)
        except ValueError as exc:
            self.error('chinese', str(exc))
            self.fail(str(exc))
            return
        self.send_host(screen)

    # Display a failure message and return to the calling menu.
    def fail(self, message: str):
        self.warning('chinese', message)
        rows = max(self.dinfo.alt_rows, 2)
        columns = max(self.dinfo.alt_columns, 8)
        self.send_host(build_error_screen(message, rows, columns, self.dinfo.extended))
        self.hangup()
