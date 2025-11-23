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
# 3270 screen generator: tn3270e

from common import *

# tn3270e header types
tn3270e_type = {
    '3270-data': '00',
    'scs-data': '01',
    'response': '02',
    'bind-image': '03',
    'unbind': '04',
    'nvt-data': '05',
    'request': '06',
    'sscp-lu-data': '07',
    'print-eoj': '08',
    'bid': '09'
}

# tn3270e request flags
tn3270e_req = {
    'none': '00',
    'err-cond-cleared': '00',
    'send-data': '01',
    'keyboard-restore': '02',
    'signal': '04'
}

# tn3270e response flags
tn3270e_rsp = {
    'no-response': '00',
    'error-response': '01',
    'always-response': '02',
    'positive-response': '00',
    'negatve-response': '01',
    'sna-sense': '04'
}

# tn3270e type req rsp seq
def tn3270e(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) < 1 or len(args) > 4:
        raise GenSyntaxError('tn3270e takes 1 to 4 arguments')
    args = args.copy()
    if len(args) < 2:
        args.append('none')
    if len(args) < 3:
        args.append('error-response')
    if len(args) < 4:
        args.append(str(gen.auto_seq))
        gen.auto_seq += 1
    ret = quote(map_arg(args[0], tn3270e_type, 'type') + \
            map_arg(args[1], tn3270e_req, 'request flag') + \
            map_arg(args[2], tn3270e_rsp, 'response flag') + \
            f'{int(args[3]):04x}')
    return quote(ret)

# tn3270e objects
e_obj = {
    'device-type': '02'
}

# tn3270e.send
def send(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 1:
        raise GenSyntaxError('tn3270e.send takes 1 argument')
    return '08' + e_obj[args[0]]

e_verb = {
    'request': '07',
    'is': '04'
}

# tn3270e.device-type verb text
def device_type(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 2:
        raise GenSyntaxError('tn3270e.device-type takes 2 arguments')
    return '02' + e_verb[args[0]] + atext_str(args[1])

# tn3270e.connect text
def connect(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 1:
        raise GenSyntaxError('tn3270e.connect takes 1 argument')
    return '01' + atext_str(args[0])

# Map for functions
functions_map = {
    'responses': '02',
    'sysreq': '04',
}

# Encodes functions
def encode_functions(flags):
    x = []
    for f in flags.split(','):
        if not f in functions_map:
            raise GenValueError(f'Unknown function {f}')
        x += functions_map[f]
    return ''.join(x)

# tn3270e.functions verb name[,name...]
def functions(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 2:
        raise GenSyntaxError('tn3270e.functions takes 2 arguments')
    return '03' + e_verb[args[0]] + encode_functions(args[1])