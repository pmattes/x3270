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
import Common.Test.ct as ct

# TLS server class that shuttles socket data to stdout.
class tls_server():
    '''TLS server'''
    context = None
    listen_socket = None
    tls_socket = None
    accept_socket = None
    thread = None
    conn = None
    def __init__(self, address, port, cert, key, ipv6=False):
        # Set up the TLS context.
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)

        # Create the listening socket and wrap it.
        self.listen_socket = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listen_socket.bind((address, port))
        self.listen_socket.listen(1)
        self.tls_socket = self.context.wrap_socket(self.listen_socket, server_side=True)

        # Asynchronously accept one connection.
        self.thread = threading.Thread(target=self.process)
        self.thread.start()

    # Accept a connection.
    def process(self):
        (self.conn, _) = self.tls_socket.accept()
        self.tls_socket.close()
        self.tls_socket = None
        self.listen_socket.close()
        self.listen_socket = None

    def recv_to_end(self, timeout=2):
        '''Return everything sent on the socket'''
        ct.try_until(lambda: self.conn != None, 2, "Client did not connect")
        self.thread.join()
        ret = b''
        while True:
            r, _, _ = select.select([self.conn], [], [], timeout)
            assert([] != r)
            data = self.conn.recv(1024)
            if data == b'':
                break
            ret += data
        self.conn.close()
        self.conn = None
        return ret