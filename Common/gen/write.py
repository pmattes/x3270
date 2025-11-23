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
# 3270 screen generator; Write-related orders

from common import *

# Map for WCC flags
wcc_map = {
    'reset': 0x40,
    'startprinter': 0x08,
    'alarm': 0x04,
    'restore': 0x02,
    'resetmdt': 0x01
}

# Encodes WCC flags
def encode_wcc_flags(flags):
    x = 0
    for f in flags.split(','):
        if not f in wcc_map:
            raise GenValueError(f'Unknown WCC flag {f}')
        x |= wcc_map[f]
    return f'{x:02x}'

# Encodes a buffer address
# row and column are 1-origin
def baddr(gen: GenCommon, row, col):
    if row <= 0:
        raise GenValueError('row must be >0')
    if row > gen.rows:
        raise GenValueError(f'row must be <={gen.rows}')
    if col <= 0:
        raise GenValueError('column must be >0')
    if col > gen.columns:
        raise GenValueError(f'column mustr be <={gen.columns}')
    baddr = ((row - 1) * gen.columns) + (col - 1)
    if gen.rows * gen.columns > 0x1000:
        return f'{(baddr >> 8) & 0x3f:02x}{(baddr & 0xff):02x}'
    else:
        return code_table[(baddr >> 6) & 0x3f] + code_table[baddr & 0x3f]

# sba row col
def sba(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 2:
        raise GenSyntaxError('sb takes 2 arguments')
    return quote('11' + baddr(gen, int(args[0]), int(args[1])))

# pt
def pt(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 0:
        raise GenSyntaxError('pt takes 0 arguments')
    return '05'

# ic
def ic(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 0:
        raise GenSyntaxError('ic takes 0 arguments')
    return '13'

# ra
def ra(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 3:
        raise GenSyntaxError('ra takes 3 arguments')
    return quote('3c' + baddr(gen, int(args[0]), int(args[1])) + args[2])

# ew flags
def ew(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) > 1:
        raise GenSyntaxError('ew takes 0 or 1 arguments')
    gen.rows = 24
    gen.columns = 80
    if len(args) == 0:
        return quote('f500')
    return quote('f5' + encode_wcc_flags(args[0]))

# ewa flags
def ewa(gen: GenCommon, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) > 1:
        raise GenSyntaxError('ewa takes 0 or 1 arguments')
    gen.rows = gen.max_rows
    gen.columns = gen.max_columns
    if len(args) == 0:
        return quote('7e00')
    return quote('7e' + encode_wcc_flags(args[0]))