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
# x3270 test target host, color test.

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

class colortest(tn3270.tn3270_server):
    '''TN3270 protocol server for color testing (plain model 4 required)'''

    inv = False
    ea = False

    def rcv_data_cooked(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Consume data'''
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270:
            return
        match data[0]:
            case aid.PF1.value:
                self.inv = not self.inv
                self.homescreen(self.inv, self.ea)
            case aid.PF2.value:
                self.ea = not self.ea
                self.homescreen(self.inv, self.ea)
            case aid.PF3.value:
                self.hangup()
            case aid.SF.value:
                # Parse the QueryReply, assuming that's what it is
                self.debug('color', 'got SF')
                qr = self.dinfo.parse_query_reply(data)
                if (qr[0]):
                    self.debug('color', f'alt rows {self.dinfo.alt_rows} columns {self.dinfo.alt_columns}')
                    if self.dinfo.rpqnames != None:
                        self.debug('color', 'RPQ Names: ' + self.dinfo.rpqnames.hex().replace('ffff', 'ff'))
                    self.rows = self.dinfo.alt_rows
                    self.columns = self.dinfo.alt_columns
                    if self.rows < 43 or self.columns < 80:
                        self.send_host(bytes([command.erase_write, 0xc7]) + 'Need 43 rows x 80 columns at minimum.'.encode('cp037') + bytes([order.sf, fa.protect, order.ic]))
                        self.hangup()
                        return
                    self.query_done()
                else:
                    self.error('color', 'Query Reply: ' + qr[1])
            case _:
                self.homescreen(self.inv, self.ea)

    def tnError(self, ttype: str) -> str:
        '''Check for valid terminal type, retuyrn error message'''
        if ttype == 'IBM-DYNAMIC':
            return None
        if ttype.startswith('IBM-327') and ttype.endswith('-E'):
            return None
        return 'Need an extended (-E) model of 3270 or IBM-DYNAMIC terminal type.'
    
    def start3270(self):
        '''Start 3270 mode'''
        self.debug('color', f'ttype is {self.dinfo.ttype}')
        err = self.tnError(self.dinfo.ttype)
        if err != None:
            self.send_host(bytes([command.erase_write, 0xc7]) + err.encode('cp037') + bytes([order.sf, fa.protect, order.ic]))
            self.hangup()
            return
        if self.dinfo.extended:
            self.query()
        else:
            self.query_done()

    def query_done(self):
        '''QueryReply has been processed (or will not be sent)'''
        self.homescreen(self.inv, self.ea)

    def homescreen(self, inv: bool, ea: bool):
        '''Dump out the home screen'''
        # cmd = self.get_dump('colortest')
        # pf = sba_bytes(self.rows, 1, self.columns) + 'F3=END'.encode('cp037')
        # self.send_raw(cmd[0:len(cmd)-2] + pf + cmd[-2:])
        self.send_host(self.build_screen(inv, ea))

    def build_screen(self, inv: bool, ea: bool) -> bytes:
        '''Build up the screen'''
        # Erase/write.
        ret = bytes([command.erase_write_alternate, 0xc7])

        # Display each color in normal and reverse video.
        row = 1
        for i in color:
            ret += sba_bytes(row, 1, self.columns) + bytes([order.sfe, 1, xa.m3270, fa.protect]) + f'{i:02x} {i.name}'.encode('cp037')
            ret += sba_bytes(row, 20, self.columns) + bytes([order.sfe, 2, xa.m3270, fa.protect, xa.fg, i.value]) + 'normal'.encode('cp037')
            ret += bytes([order.sfe, 3, xa.m3270, fa.protect, xa.fg, i.value, xa.highlighting, highlight.reverse]) + 'reverse'.encode('cp037') + bytes([order.sfe, 2, xa.m3270, fa.protect, xa.all, 0])
            row += 1

        # Display single, adjacent characters in each color.
        ret += sba_bytes(18, 1, self.columns)
        ret += bytes([order.sf, fa.protect])
        ret += sba_bytes(19, 1, self.columns)
        ret += bytes([order.sf, fa.protect])
        for i in color:
            ret += bytes([order.sa, xa.fg, i.value]) + 'X'.encode('cp037')
        ret += bytes([order.sa, xa.all, 0])

        # Display the four kinds of fields that have distinguishable colors.
        ic = False
        row = 20
        for i in [(fa.normal_nonsel, 'Normal input'), (fa.high_sel, 'Highlighted input'), (fa.protect, 'Protected'), (fa.protect|fa.high_sel, 'Protected, highlighted')]:
            ret += sba_bytes(row, 1, self.columns) + bytes([order.sf, i[0]])
            row += 1
            if not ic:
                ret += bytes([order.ic])
                ic = True
            ret += f'{i[1]} field'.encode('cp037')

        # Display each combination of foreground and background colors.
        ret += sba_bytes(25, 1, self.columns) + bytes([order.sf, fa.protect]) + '   \\  bg'.encode('cp037')
        ret += sba_bytes(26, 2, self.columns) + '    \\ 00 f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff'.encode('cp037')
        row = 27
        yLabel = 'fg'
        for fg in color:
            ret += sba_bytes(row, 1, self.columns) + bytes([order.sfe, 2, xa.m3270, fa.protect, xa.all, 0]) + f'{yLabel} {fg.value:02x}'.encode('cp037')
            row += 1
            yLabel = '  '
            for bg in color:
                if ea:
                    ret += bytes([order.sfe, 1, xa.m3270, fa.protect, order.sa, xa.fg, fg.value, order.sa, xa.bg, bg.value])
                else:
                    ret += bytes([order.sfe, 3, xa.m3270, fa.protect, xa.fg, fg.value, xa.bg, bg.value])
                if inv:
                    ret += bytes([order.sa, xa.highlighting, highlight.reverse])
                ret += 'XX'.encode('cp037')
            ret += bytes([order.sfe, 2, xa.m3270, fa.protect, xa.all, 0, order.sa, xa.highlighting, 0, order.sa, xa.fg, 0, order.sa, xa.bg, 0])

        # Add the PF3 message at the lower right.
        ret += sba_bytes(self.rows - 3, self.columns - 21, self.columns) + f'INV: {inv}'.encode('cp037')
        ret += sba_bytes(self.rows - 2, self.columns - 21, self.columns) + f'EA:  {ea}'.encode('cp037')
        ret += sba_bytes(self.rows, self.columns - 21, self.columns) + 'F1=INV  F2=EA  F3=END'.encode('cp037')
        return ret


