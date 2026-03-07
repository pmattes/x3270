#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
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
# Proxy simulator (passthru, telnet, http, socks4)

from enum import IntEnum, auto
import select
import socket
import struct
import threading

import Common.Test.cti as cti

class ProxyType(IntEnum):
    passthru = auto()
    telnet = auto()
    http = auto()
    socks4 = auto()
    socks4a = auto()
    socks5 = auto()

class proxy_server():

    # Proxy server.
    def __init__(self, cti:cti.cti, remote_port: int, local_port: int, type=ProxyType.passthru, force_error=False):

        self.ct = cti
        self.remote_port = remote_port
        self.local_port = local_port
        self.prefix = '' if type == ProxyType.passthru else 'connect '
        self.proxy_type = type
        self.force_error = force_error

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.close()

    def close(self):
        self.thread.join()
        pass

    def run(self):
        self.thread = threading.Thread(target=self.server)
        self.thread.start()

    def server(self):
        s = socket.socket()
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('127.0.0.1', self.local_port))
        s.listen()
        t = s.accept()[0]   # check_listen
        t.close()
        t = s.accept()[0]   # real connection
        s.close()
        if self.force_error:
            t.close()
            return
        if self.proxy_type == ProxyType.http:
            blob = b''
            while True:
                r, _, _ = select.select([t], [], [], 0.5)
                if r == []:
                    break
                blob += t.recv(1024)
            self.ct.assertEqual(f'CONNECT 127.0.0.1:{self.remote_port} HTTP/1.1\r\nHost: 127.0.0.1:{self.remote_port}\r\n\r\n'.encode('utf8'), blob)
            t.send(b'HTTP/1.1 200 OK\r\n\r\n')
        elif self.proxy_type == ProxyType.passthru or self.proxy_type == ProxyType.telnet:
            proxy = t.recv(1024)
            self.ct.assertEqual(f'{self.prefix}127.0.0.1 {self.remote_port}\r\n'.encode('utf8'), proxy)
        elif self.proxy_type == ProxyType.socks4:
            blob = t.recv(1024)
            self.ct.assertEqual(b'\x04\x01' + struct.pack('!H', self.remote_port) + b'\x7f\x00\x00\x01' + 'fred'.encode('utf8') + b'\x00', blob)
            # Success.
            t.send(b'\x00\x5a\x00\x00\x00\x00\x00\x00')
        elif self.proxy_type == ProxyType.socks4a:
            blob = t.recv(1024)
            self.ct.assertEqual(b'\x04\x01' + struct.pack('!H', self.remote_port) + b'\x00\x00\x00\x01' + 'fred'.encode('utf8') + b'\x00' + '127.0.0.1'.encode('utf8') + b'\x00', blob)
            # Success.
            t.send(b'\x00\x5a\x00\x00\x00\x00\x00\x00')
        else:
            self.ct.assertTrue(False, f'Unknown proxy type {self.proxy_type.name}')
        u = socket.socket()
        u.connect(('127.0.0.1', self.remote_port))
        done = False
        try:
            while not done:
                r, _, _ = select.select([t, u], [], [])
                for i in [(t, u), (u, t)]:
                    if i[0] in r:
                        data = i[0].recv(1024)
                        if data == b'':
                            done = True
                            break
                        i[1].send(data)
        except ConnectionResetError:
            pass
        t.close()
        u.close()
