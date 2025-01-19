#!/usr/bin/env python3
#
# Copyright (c) 2021-2025 Paul Mattes.
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
# s3270 keepalive tests

import time
import unittest
from subprocess import Popen, DEVNULL

from Common.Test.cti import *

@requests_timeout
class TestS3270Keepalive(cti):

    # s3270 keepalive test
    def s3270_keepalive(self, telnet=True, set=True, dynamic=False):

        # Start a thread to read s3270's output.
        nc = copyserver()

        # Start s3270.
        port, ts = unused_port()
        sval = '1' if set else '0'
        topt = '' if telnet else 't:'
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port), '-set', f'nopSeconds={sval}', f'{topt}a:c:{nc.qloopback}:{nc.port}']), stdin=DEVNULL,
                stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Set the option at run-time.
        if (dynamic):
            self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(nopSeconds,1)')

        # Give s3270 2.5 seconds to send two NOPs (or not), then close it.
        time.sleep(2.5)
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')

        # Make sure they showed up, or didn't.
        out = nc.data()
        if telnet and (set or dynamic):
            self.assertEqual(b'\xff\xf1\xff\xf1', out)
        else:
            self.assertEqual(b'', out)

        # Wait for the processes to exit.
        self.vgwait(s3270)
    
    def test_s3270_keepalive(self):
        self.s3270_keepalive(telnet=True)
    def test_s3270_keepalive_no_telnet(self):
        self.s3270_keepalive(telnet=False)
    def test_s3270_keepalive_not(self):
        self.s3270_keepalive(telnet=True, set=False)
    def test_s3270_keepalive_dynamic(self):
        self.s3270_keepalive(telnet=True, set=False, dynamic=True)

if __name__ == '__main__':
    unittest.main()
