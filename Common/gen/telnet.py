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
# 3270 screen generator: telnet

from common import *

# Map from TELNET option names to values.
telnet_opts = {
    'tn3270e': '28'
}

# do opt
def do(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 1:
        raise GenSyntaxError('do takes 1 argument')
    return 'fffd' + telnet_opts[args[0]]

# sb opt
def sb(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 1:
        raise GenSyntaxError('sb takes 1 argument')
    return 'fffa' + telnet_opts[args[0]]

# se
def se(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 0:
        raise GenSyntaxError('se takes 0 arguments')
    return 'fff0'

# eor
def eor(_, *argx):
    args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
    if len(args) != 0:
        raise GenSyntaxError('eor takes 0 arguments')
    return 'ffef'