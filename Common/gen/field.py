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
# 3270 screen generator: field-related orders

import functools

from common import *

# Map for SF flags.
sf_map = {
    'normal': 0x00,
    'protect': 0x20,
    'numeric': 0x10,
    'modify': 0x01,
    'sel': 0x04,
    'high': 0x08,
    'zero': 0x0c,
    'skip': 0x30
}

# Encodes SF flags.
def encode_sf_flags(flags):
    x = 0
    for f in flags.split(','):
        if not f in sf_map:
            raise GenValueError(f'Unknown SF flag {f}')
        x |= sf_map[f]
    return code_table[x & 0x3f]

# sf flags
def sf(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 1:
        raise GenSyntaxError('sf takes 1 argument')
    return quote('1d' + encode_sf_flags(args[0]))

# Map from highlighting attributes to their encoded values
highlighting_map = {
    'default': 0,
    'normal': 0xf0,
    'blink': 0xf1,
    'reverse': 0xf2,
    'underscore': 0xf4,
    'intensify': 0xf8
}

# highlighting x,y,z
def highlighting(h: str):
    return '{:#04x}'.format(functools.reduce(lambda x, y: x | y, [highlighting_map[hh] for hh in h.split(',')]))[2:]

# Map from color names to their encoded values
color_map = {
    'default': '00',
    'neutralBlack': 'f0',
    'blue': 'f1',
    'red': 'f2',
    'pink': 'f3',
    'green': 'f4',
    'turquoise': 'f5',
    'yellow': 'f6',
    'neutralWhite': 'f7',
    'black': 'f8',
    'deepBlue': 'f9',
    'orange': 'fa',
    'purple': 'fb',
    'paleGreen': 'fc',
    'paleTurquoise': 'fd',
    'grey': 'fe',
    'white': 'ff' 
}

# Map from character sets to their encoded values
charset_map = {
    'default': '00',
    'base': 'f0',
    'apl': 'f1',
    'dbcs': 'f8',
}

# sfe type value ...
def sfe_sa_mf(keyword, order, args, include_count=True):
    if len(args) == 0:
        raise GenSyntaxError(f'{keyword} takes at least 1 argument')
    ret = []
    while len(args) > 0:
        if args[0] == '3270':
            ret.append('c0' + encode_sf_flags(args[1]))
            args = args[2:]
        elif args[0] == 'highlighting':
            ret.append('41' + highlighting(args[1]))
            args = args[2:]
        elif args[0] == 'fg':
            ret.append('42' + color_map[args[1]])
            args = args[2:]
        elif args[0] == 'charset':
            ret.append('43' + charset_map[args[1]])
            args = args[2:]
        elif args[0] == 'bg':
            ret.append('45' + color_map[args[1]])
            args = args[2:]
        elif args[0] == 'all':
            ret.append('00' + args[1])
            args = args[2:]
        else:
            raise GenValueError(f'Unknown extended attribute {args[0]}')
    res = order
    if include_count:
        res += f'{len(ret):02x}'
    res += ''.join(ret)
    return quote(res)

# sfe type value...
def sfe(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    return sfe_sa_mf('sfe', '29', args)

# sa type value...
def sa(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 2:
        raise GenSyntaxError('sa takes 2 arguments')
    return sfe_sa_mf('sa', '28', args, include_count=False)

# mf type value...
def mf(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    return sfe_sa_mf('mf', '2c', args)
