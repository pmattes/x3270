#!/usr/bin/env python3
#
# Copyright (c) 2022 Paul Mattes.
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
# TLS test server

import socket
import select
import ssl
import threading
import re
import Common.Test.ct as ct
import Common.Test.telnet as telnet

# TLS server.
class tls_server():
    '''TLS server'''
    context = None
    listen_socket = None
    accept_socket = None
    thread = None
    tls_conn = None
    clear_conn = None
    def __init__(self, address, port, cert, key, ipv6=False):
        # Set up the TLS context.
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)

        # Create the listening socket and wrap it.
        self.listen_socket = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listen_socket.bind((address, port))
        self.listen_socket.listen(1)

        # Asynchronously accept one connection.
        self.thread = threading.Thread(target=self.process)
        self.thread.start()

    # Accept a connection.
    def process(self):
        (self.clear_conn, _) = self.listen_socket.accept()
        self.listen_socket.close()
        self.listen_socket = None

    def recv_to_end(self, timeout=2):
        '''Return everything sent on the socket'''
        ct.try_until(lambda: self.clear_conn != None, 2, "Client did not connect")
        self.thread.join()
        self.tls_conn = self.context.wrap_socket(self.clear_conn, server_side=True)
        ret = b''
        while True:
            r, _, _ = select.select([self.tls_conn], [], [], timeout)
            assert([] != r)
            data = self.tls_conn.recv(1024)
            if data == b'':
                break
            ret += data
        self.tls_conn.close()
        self.tls_conn = None
        self.clear_conn.close()
        self.clear_conn = None
        return ret

    def nread(self, c, n, timeout=2):
        '''Read n bytes from a socket with a timeout'''
        nleft = n
        ret = b''
        while nleft > 0:
            r, _, _ = select.select([c], [], [], timeout)
            assert [] != r
            chunk = c.recv(nleft)
            assert chunk != b''
            ret += chunk
            nleft -= len(chunk)
        return ret

    def starttls(self, timeout=2):
        '''Do STARTTLS negotiation'''
        ct.try_until(lambda: self.clear_conn != None, 2, "Client did not connect")
        self.thread.join()
        # Send IAC DO STARTTLS.
        self.clear_conn.send(telnet.iac + telnet.do + telnet.startTls)
        # Make sure they respond with IAC WILL STARTTLS and the right SB.
        startTlsSb = telnet.iac + telnet.sb + telnet.startTls + telnet.follows + telnet.iac + telnet.se
        expectStartTls = telnet.iac + telnet.will + telnet.startTls + startTlsSb
        data = self.nread(self.clear_conn, len(expectStartTls), timeout)
        assert data == expectStartTls
        # Send the SB.
        self.clear_conn.send(startTlsSb)
        # Wrap the clear socket with TLS.
        self.tls_conn = self.context.wrap_socket(self.clear_conn, server_side=True)

    def check_trace(self, traceFile, timeout=2):
        '''Check emulator against a trace file'''
        direction = ''
        accum = ''
        lno = 0
        with open(traceFile, 'r') as file:
            while True:
                lno += 1
                line = file.readline()
                if line == '':
                    break
                isIo = re.match('^[<>] 0x[0-9a-f]+ +', line)
                if not isIo or line[0] != direction:
                    # Possibly dump output or wait for input.
                    if direction == '<':
                        # Send to emulator.
                        self.tls_conn.send(bytes.fromhex(accum))
                    elif direction == '>':
                        # Receive from emulator.
                        want = bytes.fromhex(accum)
                        r = self.nread(self.tls_conn, len(want))
                        assert(r == want)
                    direction = ''
                    accum = ''
                if isIo:
                    # Start accumulating.
                    direction = line[0]
                    accum += line.split()[2]
        self.tls_conn.close()
        self.clear_conn.close()