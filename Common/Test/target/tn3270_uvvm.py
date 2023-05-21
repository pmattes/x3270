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
# x3270 test target host, TN3270 UVVM server.

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

class uvvm(tn3270.tn3270_server):
    '''TN3270 protocol server simulating uvvm'''

    def rcv_data_cooked(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Consume data'''
        if mode != tn3270e_proto.data_type.d3270_data  or not self.in3270:
            return
        match data[0]:
            case aid.PF3.value:
                self.hangup()
            case _:
                self.homescreen()

    def start3270(self):
        '''Start 3270 mode'''
        self.homescreen(reset=True)

    def homescreen(self, reset=False):
        '''Dump out the home screen'''
        self.send_host(self.get_dump('uvvm'))
