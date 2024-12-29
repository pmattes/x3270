#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# RPQNAMES utilities

from enum import IntEnum

def add_len(body: str) -> str:
    '''Prepend the length byte to a field'''
    lenf = (len(body) // 2) + 1
    return f'{lenf:02x}' + body

def iac_quote(body: str) -> str:
    '''Double TELNET IAC bytes'''
    return ''.join([b + b if b == 'ff' else b for b in [body[i:i+2] for i in range(0, len(body), 2)]])
    
def make_rpq(body: str) -> str:
    '''Construct the RPQ Names reply, getting the fixed fields and length right'''
    return iac_quote('0000000000000000' + add_len(ebcdic('x3270') + body))

def ebcdic(text: str) -> str:
    '''Convert text to hex EBCDIC'''
    return ''.join([f'{c:02x}' for c in text.encode('cp037')])

class RpqName(IntEnum):
    Address = 0,
    Timestamp = 1,
    Timezone = 2,
    User = 3,
    Version = 4
    def encode(self) -> str:
        return f'{self.value:02x}'