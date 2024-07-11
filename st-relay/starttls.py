#!/usr/bin/env python3
#
# Copyright (c) 2022-2024 Paul Mattes.
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
# STARTTLS relay, STARTTLS layer.

import select
import socket

import oopts
from telnet_proto import *
from ttelnet import *

class starttls_layer(ttelnet, consumer.consumer):
    '''STARTTLS server'''

    tls_ready = False

    def __init__(self, conn: socket.socket, logger: logging.Logger, peername: str, tls: bool, opts: oopts.oopts):
        super().__init__(conn, logger, peername, self)
        self.tls = tls
        self.opts = opts
    def __enter__(self):
        super().__enter__()
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        super().__exit__(exc_type, exc_value, exc_traceback)
    def __del__(self):
        super().__del__()

    def ready(self) -> bool:
        '''Initialize state'''
        if self.tls:
            # Negotiated TLS.
            # Wait 2 seconds for input, then ask for STARTTLS.
            r, _, _ = select.select([self.conn], [], [], 2)
            if r == []:
                self.debug('starttls', 'initial TLS negotiation read timeout')
                self.send_do(telopt.STARTTLS)
            else:
                data = self.conn.recv(1, socket.MSG_PEEK)
                if data == b'\x16':
                    # They sent a TLS Hello. Complete the negotiation.
                    if not self.wrap(self.opts.get('cert'), self.opts.get('key')):
                        self.hangup()
                    else:
                        self.tls_ready = True
                        self.info('starttls', 'connection is secure (immediate)')
                elif data != b'':
                    # They sent something else. Whatever it is, we will process it as TELNET.
                    # Send them a DO STARTTLS, assuming that they speak TELNET.
                    self.send_do(telopt.STARTTLS)
        else:
            self.tls_ready = True
        return True
    
    def negotiation_complete(self) -> bool:
        '''Check for TLS negotiation complete'''
        return self.tls_ready

    # Called from TELNET.
    def rcv_data(self, data: bytes):
        '''Process data from TELNET'''
        # Send it to the emulated host.
        self.rcv_data_cooked(data)

    def send_host(self, data: bytes):
        '''Send host data'''
        self.send_data(data, eor=True)

    def rcv_sb(self, option: telopt, data: bytes):
        '''Consume SB'''
        if option == telopt.STARTTLS:
            if len(data) < 1 or data[0] != int(teltls.FOLLOWS):
                self.warning('starttls', 'bad SB STARTTLS')
                self.send_data(b'Bad SB STARTTLS\r\n')
                self.hangup()
                return
            self.send_sb(telopt.STARTTLS, bytes([int(teltls.FOLLOWS)]))
            if not self.wrap(self.opts.get('cert'), self.opts.get('key')):
                self.hangup()
            else:
                self.tls_ready = True
                self.info('starttls', 'connection is secure (negotiated)')

    def rcv_will(self, option: telopt) -> bool:
        return option == telopt.STARTTLS

    def rcv_wont(self, option: telopt) -> bool:
        '''Notify of WONT option'''
        if option == telopt.STARTTLS and self.tls:
            self.warning('starttls', 'refused STARTTLS')
            self.send_data(b'STARTTLS is mandatory\r\n')
            self.hangup()
        return True

    def rcv_do(self, option: telopt) -> bool:
        '''Approve DO option'''
        return False

    def rcv_dont(self, option: telopt) -> bool:
        '''Approve DONT option'''
        return False

    def rcv_cmd(self, cmd: telcmd):
        '''Notify of command'''
        self.debug('starttls', f'got {cmd.name} command')