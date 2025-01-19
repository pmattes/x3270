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
# s3270 script tests

from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *

class TestS3270Script(cti):

    # Run the test in one of three modes.
    def run_script_test(self, mode):

        # Start a thread to read s3270's output.
        nc = copyserver()

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', '-xrm', 's3270.noTelnetInputMode: character',
                f'a:c:t:127.0.0.1:{nc.port}']), stdin=PIPE, stdout=DEVNULL)
        self.children.append(s3270)

        # Feed s3270 the action.
        text = f'Script(python3,s3270/Test/script/simple.py,-{mode},hello,there)\n'
        s3270.stdin.write(text.encode('utf8'))
        s3270.stdin.flush()

        # Make sure it works.
        out = nc.data()
        self.assertEqual(b'hello there', out)

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 HTTP script test
    def test_s3270_script_http(self):
        self.run_script_test('http')

    # s3270 socket script test
    def test_s3270_script_socket(self):
        self.run_script_test('socket')

    # s3270 pipe script test
    @unittest.skipIf(sys.platform.startswith("win"), "No s3270 pipes in Windows")
    def test_s3270_script_pipe(self):
        self.run_script_test('pipe')

if __name__ == '__main__':
    unittest.main()
