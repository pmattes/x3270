#!/usr/bin/env python3
#
# Copyright (c) 2022-2025 Paul Mattes.
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
# 3270 screen generator, common code

class GenSyntaxError(SyntaxError):
    # Constructor method
    def __init__(self, value):
        self.value = value
    # __str__ display function
    def __str__(self):
        return(repr(self.value))

class GenValueError(ValueError):
    # Constructor method
    def __init__(self, value):
        self.value = value
    # __str__ display function
    def __str__(self):
        return(repr(self.value))

class GenCommon():
    rows = 24
    columns = 80
    max_rows = 24
    max_columns = 80
    auto_seq = 1

# Maps an argument
def map_arg(arg, map, type):
    if not arg in map:
        raise GenValueError(f'Unkown {type} {arg}')
    return map[arg]

code_table = [
    '40', 'c1', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7',
    'c8', 'c9', '4a', '4b', '4c', '4d', '4e', '4f',
    '50', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7',
    'd8', 'd9', '5a', '5b', '5c', '5d', '5e', '5f',
    '60', '61', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7',
    'e8', 'e9', '6a', '6b', '6c', '6d', '6e', '6f',
    'f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7',
    'f8', 'f9', '7a', '7b', '7c', '7d', '7e', '7f'
]

# Makes non-TELNET data IAC safe.
def quote(hex):
    return ''.join([b+'ff' if b == 'ff' else b for b in [hex[i:i+2] for i in range(0, len(hex), 2)]])

# Expand a string into encoded text.
def atext_str(s: str):
    return ''.join([f'{c:2x}' for c in s.encode('utf8')])
