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
# Passthru proxy

import select
import socket
import sys
import threading

class proxy_exception(Exception):
    """Things did not go well"""

class proxy_server():

    # Proxy server.
    def __init__(self, local_port: int):

        self.local_port = local_port

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
        t = s.accept()[0]
        s.close()
        proxy = t.recv(1024).decode('utf8').split()
        if len(proxy) != 2:
            raise proxy_exception('Bad proxy syntax')
        u = socket.socket()
        print(f'connecting to {proxy[0]} {proxy[1]}')
        u.connect((proxy[0], int(proxy[1])))
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

if __name__ == '__main__':
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    else:
        port = 3514
    p = proxy_server(port)
    p.run()
    p.close()
