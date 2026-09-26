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
import subprocess

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

required_cgcsgid = 0x04380345
poem = '''\
江城子, 苏轼 (“Song of River City”, by Su Shi)

十年生死两茫茫，不思量，自难忘。
千里孤坟，无处话凄凉。
纵使相逢应不识，尘满面，鬓如霜。
夜来幽梦忽还乡，小轩窗，正梳妆。
相顾无言，惟有泪千行。
料得年年肠断处，明月夜，短松冈。
'''
linebreak = re.compile(rb'\x0d\x25|\r\n|[\x0a\x0d\x15\x25]')

# Convert editable UTF-8 text to the terminal's CP935 byte stream.
def encode_cp935(text: str) -> bytes:
    result = subprocess.run(
        ['iconv', '-f', 'UTF-8', '-t', 'IBM-935'],
        input=text.encode('utf-8'),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return result.stdout

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

    # Convert and display the UTF-8 poem using CP935.
    def display_poem(self):
        try:
            screen = build_screen(encode_cp935(poem), self.dinfo.alt_rows,
                                  self.dinfo.alt_columns)
        except (OSError, subprocess.CalledProcessError) as exc:
            detail = getattr(exc, 'stderr', b'')
            if detail:
                detail = detail.decode('utf-8', errors='replace').strip()
            self.error('chinese', f'Cannot convert poem to CP935: {detail or exc}')
            self.fail('Unable to convert the UTF-8 poem to EBCDIC 935.')
            return
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
