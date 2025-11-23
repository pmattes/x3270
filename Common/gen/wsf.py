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
# 3270 screen generator: structured fields

from common import *

reply_modes = {
    'field': ('00', 2),
    'extended-field': ('01', 2),
    'character': ('02', 3),
}

attributes = {
    'highlighting': '41',
    'fg': '42',
    'charset': '43',
    'bg': '45',
}

# wsf.set-reply-mode <partition> <mode> [<attribute>[,<attribute>...]]
def set_reply_mode(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) < 2:
        raise GenSyntaxError('write-structured-field takes at least 2 arguments')
    partition = args[0]
    mode = args[1]
    if not mode in reply_modes:
        raise GenValueError(f"unknown reply mode '{mode}'")
    rm = reply_modes[mode]
    if len(args) != rm[1]:
        raise GenSyntaxError(f'write-structured-field {mode} takes {rm[1]} arguments')
    if rm[1] == 3:
        attrs = ''.join([attributes[i] for i in args[2].split(',')])
    else:
        attrs = ''
    payload = '09' + partition + rm[0] + attrs
    payload_len = int((4 + len(payload)) / 2)
    return '11' + f'{payload_len:04x}' + payload