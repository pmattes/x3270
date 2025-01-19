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
# s3270 connection fail tests

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *

class TestS3270ConnectFail(cti):

    # s3270 connect fail test
    def test_s3270_connect_fail(self):

        # Start s3270.
        s3270 = Popen(vgwrap(["s3270"]), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a trivial command at it.
        s3270.stdin.write(b'Connect(localhost:1)\n')

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8').split(os.linesep)

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.assertEqual(5, len(stdout))
        self.assertTrue(stdout[0].startswith('data: Connection failed:'))
        self.assertTrue(stdout[1].startswith('data: localhost, port 1: '))
        self.assertTrue('refused' in stdout[1])
        self.assertTrue(stdout[2].startswith('L U U N N 4 24 80 0 0 0x0 '))
        self.assertEqual('error', stdout[3])
        self.assertEqual('', stdout[4])

if __name__ == '__main__':
    unittest.main()
