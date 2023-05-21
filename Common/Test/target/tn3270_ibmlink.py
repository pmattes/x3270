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
# x3270 test target host, TN3270 IBMLINK server.

import datetime
from typing import List

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

class ibmlink(tn3270.tn3270_server):
    '''TN3270 protocol server simulating ibmlink, with help screens'''

    def rcv_data_cooked(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Consume data'''
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270:
            return
        match self.host_state:
            case 0: # main screen
                match data[0]:
                    case aid.PF3.value:
                        self.hangup()
                    case aid.PF1.value:
                        self.arrow = ''
                        self.field6 = ''
                        self.host_state = 1
                        self.send_host(self.get_dump('help1'))
                    case aid.CLEAR.value:
                        self.homescreen()
                    case aid.ENTER.value:
                        self.enter(data)
                    case _:
                        self.send_host(self.get_dump('bad_pf'))
            case _: # some help screen
                if data[0] == aid.PF3.value:
                    self.homescreen()
                    return
                if data[0] == aid.PF7.value and self.host_state > 1:
                    self.host_state -= 1
                if data[0] == aid.PF8.value and self.host_state < 5:
                    self.host_state += 1
                self.send_host(self.get_dump(f'help{self.host_state}'))

    def enter(self, data: bytes):
        '''Process an ENTER AID'''

        # Decode the implied READ BUFFER.
        reported_data = [0 for _ in range(24 * 80)]
        buffer_addr = -1
        i = 3
        while i < len(data):
            if data[i] == order.sba.value:
                buffer_addr = decode_address(data[i + 1 : i + 3])
                i += 3
            else:
                reported_data[buffer_addr] = data[i]
                buffer_addr += 1
                if buffer_addr >= 24 * 80:
                    buffer_addr = 0
                i += 1
        self.account = get_field(reported_data, 21, 13, 80, 8)
        self.userid = get_field(reported_data, 21, 32, 80, 8)
        self.arrow = get_field(reported_data, 24, 7, 80, 60, underscore=False)
        self.field6 = get_field(reported_data, 24, 71, 80, 8, underscore=False)
        if isblank(self.account) or isblank(self.userid):
            self.send_host(self.restamp(self.get_dump('incomplete')))
            return
        self.send_host(self.restamp(self.get_dump('bad_account')))

    def start3270(self):
        '''Start 3270 mode'''
        self.homescreen(reset=True)

    def restamp(self, blob: bytes) -> bytes:
        '''Apply a current timestamp and user-entered data to a screen snapshot'''
        t = datetime.datetime.now()
        hhmmss = f'{t.hour:02}:{t.minute:02}:{t.second:02}'.encode('cp037')
        blob = blob.replace(bytes.fromhex('0101'), hhmmss)
        yymmdd = f'{(t.year%100):02}/{t.month:02}/{t.day:02}'.encode('cp037')
        blob = blob.replace(bytes.fromhex('0909'), yymmdd)
        blob = blob.replace(bytes.fromhex('0303'), self.account.encode('cp037'))
        blob = blob.replace(bytes.fromhex('0404'), self.userid.encode('cp037'))
        blob = blob.replace(bytes.fromhex('0505'), self.arrow.encode('cp037'))
        blob = blob.replace(bytes.fromhex('0606'), self.field6.encode('cp037'))
        blob = blob.replace(bytes.fromhex('0707'), self.termid.encode('cp037'))
        blob = blob.replace(bytes.fromhex('0808'), self.system.encode('cp037'))
        return blob

    def homescreen(self, reset=False):
        '''Dump out the home screen'''
        self.host_state = 0
        if reset:
            self.account = '________'
            self.userid = '________'
            self.arrow = ''
            self.field6 = ''
        self.send_host(self.restamp(self.get_dump('ibmlink')))


