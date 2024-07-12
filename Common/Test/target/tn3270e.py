#!/usr/bin/env python3
#
# Copyright (c) 2022-2023 Paul Mattes.
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
# x3270 test target host, TN3270E support.

import re
from typing import List

from atn3270 import atn3270
import ds
from ftie import ftie
import oopts
from tn3270e_proto import *

def str_expand(text: str) -> List[int]:
    '''Expand a string into integers'''
    return [b for b in bytes(text.encode('iso8859-1'))]

# BIND image boilerplate borrowed from a real host
bind1 = '31010303b1903080008787f88700028000000000' # before dimensions
bind2 = '0000'                                     # between dimensions and PLU name length
bind3 = '0005007eec0b1008c9c2d4f0e3c5e2d8'         # after PLU name

class tn3270e():
    '''TN3270E protocol support'''

    def __init__(self, atn3270: atn3270, opts: oopts.oopts):
        self.in3270 = False
        self.atn3270 = atn3270
        self.opts = opts
        self.got_terminal = False
        self.got_functions = False
        self.sequence = 0
        self.functions = []
        self.do_bind = opts.get('bind', 'True') == 'True'
        self.dinfo = ds.dinfo('IBM-3278-2-E')

    def negotiate(self):
        '''Start negotiating'''

        # Start negotiating, TERMINAL-TYPE first.
        self.atn3270.e_sb(bytes([int(op.send), int(op.device_type)]))
        return

    def stop(self):
        '''Stop TN3270E'''
        return

    def bind_image(self) -> bytes:
        '''Construct a BIND-IMAGE string'''
        ret = bytes.fromhex(bind1) + bytes([24, 80, self.dinfo.alt_rows, self.dinfo.alt_columns])
        if self.dinfo.dynamic:
            ret += bytes([ds.binpresz.binpsz3.value])
        elif self.dinfo.model == '2':
            ret += bytes([ds.binpresz.binpsfx.value])
        else:
            ret += bytes([ds.binpresz.binpszrc.value])
        ret += bytes.fromhex(bind2)
        system = self.atn3270.e_get_system()
        ret += bytes([len(system)])
        ret += system.encode('cp037')
        ret += bytes.fromhex(bind3)
        return ret

    def rcv_sb(self, buffer: bytes):
        '''Process a TN3270E sub-negotiation'''
        match ftie(buffer[0], op):
            case op.device_type:
                if buffer[1] != int(op.request):
                    self.atn3270.e_warning(f'unknown DEVICE-TYPE verb {buffer[1]}')
                    return
                # The next line is inadequate: It ignores the CONNECT opcode (1) that might follow the terminal type,
                # followed by the requested Logical Unit
                if int(op.connect) in buffer:
                    text = buffer[2:].decode('iso8859-1').split(chr(int(op.connect)))
                    ttype = text[0]
                    connect = text[1]
                    self.atn3270.e_debug(f'got SB DEVICE-TYPE REQUEST {ttype} CONNECT {connect}')
                else:
                    ttype = buffer[2:].decode('iso8859-1')
                    self.atn3270.e_debug(f'got SB DEVICE-TYPE REQUEST {ttype}')
                if not re.match(r'IBM-3278(-E)?', ttype) and ttype != 'IBM-DYNAMIC':
                    self.atn3270.e_warning(f'rejecting DEVICE-TYPE {ttype}')
                    self.atn3270.e_sb(bytes([int(op.device_type), int(op.reject),
                                             int(op.reason), int(reason.invalid_device_type)]))
                else:
                    self.got_terminal = True
                    self.terminal = ttype
                    # Return TERMID for now.
                    # Return SYSTEM (e_get_system()) in the BIND indication later.
                    self.atn3270.e_sb(bytes([int(op.device_type), int(op.op_is)] +
                                            str_expand(self.terminal) +
                                            [int(op.connect)] +
                                            str_expand(self.atn3270.e_get_termid())))
                    self.atn3270.e_debug(f'sent SB DEVICE-TYPE IS {ttype} CONNECT {self.atn3270.e_get_termid()}')
                    self.dinfo = self.atn3270.e_set_ttype(ttype)
                    if self.got_functions:
                        self.in3270 = True
                        self.atn3270.e_in3270(True)
            case op.functions:
                if buffer[1] == int(op.op_is):
                    self.got_functions = True
                    self.functions = [b for b in buffer[2:]]
                    self.atn3270.e_debug(f'got SB FUNCTIONS IS {[str(ftie(b, func)) for b in self.functions]}')
                elif buffer[1] == int(op.request):
                    self.atn3270.e_debug(f'got SB FUNCTIONS REQUEST {[str(ftie(b, func)) for b in buffer[2:]]}')
                    if len(buffer) == 2 or (len(buffer) == 3 and self.do_bind and buffer[2] == int(func.bind_image)):
                        # They want what we want. Cool.
                        self.atn3270.e_sb(bytes([int(op.functions), int(op.op_is)]) + buffer[2:])
                        self.got_functions = True
                        self.functions = [b for b in buffer[2:]]
                        self.atn3270.e_debug(f'sent SB FUNCTIONS IS {[str(ftie(b, func)) for b in self.functions]}')
                    else:
                        # Tell them what we want.
                        sb = [int(op.functions), int(op.request)]
                        if int(func.bind_image) in buffer[2:] and self.do_bind:
                            sb.append(int(func.bind_image))
                        self.atn3270.e_debug(f'sent SB FUNCTIONS REQUEST {[str(ftie(b, func)) for b in sb[2:]]}')
                        self.atn3270.e_sb(bytes(sb))
                else:
                    # No idea what they want.
                    self.atn3270.e_warning(f'unknown FUNCTIONS verb {buffer[1]}')
                    self.atn3270.e_in3270(False)
                if self.got_terminal and self.got_functions:
                    self.in3270 = True
                    if int(func.bind_image) in self.functions:
                        self.atn3270.e_to_terminal(self.header(data_type.bind_image) + self.bind_image())
                    self.atn3270.e_in3270(True)

    def from_terminal(self, buffer: bytes):
        '''Process a TN3270E protocol message from the terminal'''
        if not self.in3270:
            return
        if buffer[0] in [data_type.d3270_data.value, data_type.nvt_data.value, data_type.sscp_lu_data.value]:
            self.atn3270.e_to_host(buffer[5:], data_type(buffer[0]))
        # else:
            # self.warning ...
        return

    def header(self, mode=data_type.d3270_data):
        '''Generate a 3270 data header'''
        b = bytes([int(mode), 0, 0, self.sequence >> 8, self.sequence & 0xff])
        self.sequence = self.sequence % 65536
        return b
