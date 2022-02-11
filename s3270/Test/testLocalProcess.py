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
# s3270 local process tests

import re
import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import os
import sys
import TestCommon

@unittest.skipIf(sys.platform.startswith("win"), "No local process on Windows")
class TestS3270LocalProcess(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # s3270 TERM variable test
    def s3270_lp_term(self, model, term, override=None):

        # Start s3270.
        port, ts = TestCommon.unused_port()
        args = [ 's3270', '-model', model, '-httpd', str(port) ]
        if override != None:
            args.append('-tn')
            args.append(override)
        args.append('-e')
        args.append('/bin/bash')
        args.append('-c')
        args.append('s3270/Test/echo_term.bash')
        s3270 = Popen(args, stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()

        # Wait for the script to exit and get the result.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Wait(Disconnect)', timeout=2)
        self.assertEqual(requests.codes.ok, r.status_code)
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Ascii1(1,1,1,80)')
        self.assertEqual(requests.codes.ok, r.status_code)
        j = r.json()
        self.assertEqual(term, j['result'][0].strip())

        s3270.kill()
        s3270.wait(timeout=2)

    # s3270 TERM tests
    def test_s3270_lp_color(self):
        self.s3270_lp_term('3279-2-E', 'xterm-color')
    def test_s3270_lp_mono(self):
        self.s3270_lp_term('3278-2-E', 'xterm')
    def test_s3270_lp_override(self):
        self.s3270_lp_term('3279-2-E', 'foo', override='foo')

if __name__ == '__main__':
    unittest.main()
