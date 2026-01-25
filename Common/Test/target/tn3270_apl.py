#!/usr/bin/env python3
#
# Copyright (c) 2022-2026 Paul Mattes.
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
# x3270 test target host, special-character server.

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

class rm(enum.IntEnum):
    field = 0x00
    xfield = 0x01
    char = 0x02

class apl(tn3270.tn3270_server):
    '''TN3270 protocol server for special characters'''

    reply_mode = rm.field

    def rcv_data_cooked(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Consume data'''
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270:
            return
        match data[0]:
            case aid.PF3.value:
                self.hangup()
            case aid.PF8.value:
                self.reply_mode = rm.field
                self.homescreen()
            case aid.PF9.value:
                self.reply_mode = rm.xfield
                self.homescreen()
            case aid.PF10.value:
                self.reply_mode = rm.char
                self.homescreen()
            case aid.SF.value:
                # Parse the QueryReply, assuming that's what it is
                self.debug('apl', 'got SF')
                qr = self.dinfo.parse_query_reply(data)
                if (qr[0]):
                    self.debug('apl', f'alt rows {self.dinfo.alt_rows} columns {self.dinfo.alt_columns}')
                    if self.dinfo.rpqnames != None:
                        self.debug('apl', 'RPQ Names: ' + self.dinfo.rpqnames.hex().replace('ffff', 'ff'))
                    self.debug('apl', f'GE: {self.dinfo.ge}, DBCS: {self.dinfo.dbcs}')
                    self.query_done()
                else:
                    self.error('apl', 'Query Reply: ' + qr[1])
            case _:
                self.homescreen()

    def start3270(self):
        '''Start 3270 mode'''
        self.debug('apl', f'ttype is {self.dinfo.ttype}')
        #if not self.dinfo.ttype.startswith('IBM-327') or not self.dinfo.ttype[9] == '4':
        #    self.send_host(bytes([command.erase_write, 0xc7]) + 'Plain model 4 required. '.encode('cp037') + bytes([order.sf, fa.protect, order.ic]))
        #    self.hangup()
        #    return
        if self.dinfo.extended:
            self.query()
        else:
            self.query_done()

    def query_done(self):
        '''QueryReply has been processed (or will not be sent)'''
        self.homescreen(reset=True)

    def homescreen(self, reset=False):
        '''Dump out the home screen'''
        # EWA
        b = bytes([command.erase_write_alternate, 0xc7])
        # Title field
        b += bytes([order.sf, fa.protect])
        b += f'GE: {self.dinfo.ge}, DBCS: {self.dinfo.dbcs}, reply mode: {self.reply_mode.name.upper()}'.encode('cp037')
        b += bytes([order.sf, fa.protect])
        # APL
        if self.dinfo.ge:
            b += sba_bytes(3, 1, self.dinfo.alt_columns)
            b += bytes.fromhex('2902c0604200') # SFE 3270 protect fg default
            b += 'APL characters'.encode('cp037')
            b += sba_bytes(4, 1, self.dinfo.alt_columns)
            b += bytes.fromhex('1de9') # sf protect,high,modify
            b += 'APL SA '.encode('cp037')
            b += bytes.fromhex('1dc9') # sf high,modify
            b += bytes([order.ic])
            b += bytes.fromhex('2843f1') # sa charset apl
            b += bytes.fromhex('4041424344') # raw text
            b += bytes.fromhex('284300') # sa charset default
            b += bytes.fromhex('454647') # raw text
            b += bytes.fromhex('2843f1') # sa charset apl
            b += bytes.fromhex('48494a4b4c4d4e4f') # raw text
            b += bytes.fromhex('284300') # sa charset default
            b += bytes.fromhex('1d60') # sf protect
            b += sba_bytes(5, 1, self.dinfo.alt_columns)
            b += bytes.fromhex('1de9') # sf protect,high,modify
            b += 'APL SFE'.encode('cp037')
            b += bytes.fromhex('2902c0c943f1') # sfe 3270 high,modify charset apl
            b += bytes.fromhex('5051525354') # raw text
            b += bytes.fromhex('2843f1') # sa charset apl
            b += bytes.fromhex('555657') # raw text
            b += bytes.fromhex('284300') # sa charset default
            b += bytes.fromhex('58595a5b5c5d5e5f') # raw text
            b += bytes.fromhex('1d60') # sf protect

        # DBCS
        if self.dinfo.dbcs:
            pass

        b += sba_bytes(self.dinfo.alt_rows, 1, self.dinfo.alt_columns)
        b += f'F3=END     F8=FIELD F9=XFIELD F10=CHAR'.encode('cp037')
        self.send_host(b)

        match self.reply_mode:
            case rm.xfield:
                b = bytes.fromhex('110005090001') # wsf.set-reply-mode 00 extended-field
            case rm.char:
                b = bytes.fromhex('11000609000243') # wsf.set-reply-mode 00 character charset
            case _:
                b = bytes.fromhex('110005090000') # wsf.set-reply-mode 00 field
        self.send_host(b)



