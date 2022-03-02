#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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
# Python version of the playback utility.

import re
import socket
import threading
import select
import Common.Test.cti as cti

# Simple socket copy server.
class playback():

    conn = None
    file = None
    thread = None

    # Initialization.
    def __init__(self, ct:cti.cti, trace_file:str, port=4001, ipv6=False):
        self.ct = ct
        self.listensocket = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        loopback = '::1' if ipv6 else '127.0.0.1'
        self.listensocket.bind((loopback, port))
        self.listensocket.listen()
        self.file = open(trace_file, 'r')
        self.thread = threading.Thread(target=self.process)
        self.thread.start()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        return
    
    # Shutdown.
    def __del__(self):
        if self.conn != None:
            self.conn.close()
            self.conn = None
        if self.file != None:
            self.file.close()
            self.file = None
        if self.thread != None:
            self.thread.join()
            self.thread = None

    def close(self):
        '''Close the object'''
        self.__del__()

    # Accept a connection asynchronously.
    def process(self):
        (self.conn, _) = self.listensocket.accept()
        self.listensocket.close()
    
    def wait_accept(self, timeout=2):
        '''Wait for a connection'''
        cti.cti.try_until(self.ct, lambda: self.conn != None, timeout, 'Emulator did not connect')

    def send_records(self, n=1):
        '''Copy n records to the emulator'''
        self.wait_accept()
        # Copy until we hit the n'th EOR.
        while n > 0:
            line = self.file.readline()
            if line == '':
                break
            if re.match('^< 0x[0-9a-f]+ +', line):
                data = line.split()[2]
                self.conn.send(bytes.fromhex(data))
                if data.endswith('ffef'):
                    n -= 1
        self.ct.assertEqual(0, n, 'Trace file EOF before records read')
        # Send a timing mark.
        self.conn.send(b'\xff\xfd\x06')
        # Wait for it to come back. This code assumes that the emulator will do
        # a TCP PUSH after the IAC WONT TM.
        accum = ''
        while True:
            r, _, _ = select.select([self.conn], [], [], 2)
            self.ct.assertNotEqual([], r, 'Emulator did not send TM response')
            accum += bytes.hex(self.conn.recv(1024))
            if accum.endswith('fffc06'):
                break
