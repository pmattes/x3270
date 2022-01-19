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
# s3270 test for command-line host connection errors

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import sys
import TestCommon

class TestS3270CmdLineHostError(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # s3270 command-line host connect failure test.
    def test_s3270_cmdline_host_connect_error(self):

        # Start s3270.
        s3270 = Popen(['s3270', '255.255.255.255:22'], stdin=DEVNULL, stdout=PIPE,
                stderr=PIPE)
        self.children.append(s3270)

        # Get the result.
        out = s3270.communicate(timeout=2)

        # Wait for the process to exit.
        rc = s3270.wait(timeout=2)
        self.assertNotEqual(rc, 0)

        # Check.
        # There should be nothing on stdout, but something on stderr.
        self.assertEqual(b'', out[0])
        self.assertTrue(out[1].startswith(b'Connection failed:'))

    # s3270 command-line host negotiation error test
    def test_s3270_cmdline_host_negotiation_error(self):

        # Start 'playback' to read s3270's output.
        playback_port, ts = TestCommon.unused_port()
        playback = Popen(["playback", "-w", "-p", str(playback_port),
            "s3270/Test/ibmlink.trc"], stdin=PIPE, stdout=DEVNULL)
        self.children.append(playback)
        TestCommon.check_listen(playback_port)
        ts.close()

        # Start s3270.
        s3270_port, ts = TestCommon.unused_port()
        s3270 = Popen(['s3270', '-trace',
            '-xrm', 's3270.contentionResolution: false',
            '-xrm', 's3270.scriptedAlways: true',
            '-httpd', f'127.0.0.1:{s3270_port}',
            f'127.0.0.1:{playback_port}'], stdin=PIPE, stdout=PIPE, stderr=PIPE)
        self.children.append(s3270)
        TestCommon.check_listen(s3270_port)
        ts.close()

        # Start negotation, but break the connection before drawing the
        # screen.
        playback.stdin.write(b'r\n')
        playback.stdin.flush()
        TestCommon.check_push(playback, s3270_port, 1)
        playback.stdin.write(b'd\n')
        playback.stdin.flush()

        # Get the result.
        s3270.stdin.write(b'Quit()\n')
        out = s3270.communicate(timeout=2)

        # Wait for the processes to exit.
        playback.stdin.close()
        rc = playback.wait(timeout=2)
        self.assertEqual(0, rc)
        rc = s3270.wait(timeout=2)
        self.assertEqual(0, rc)

        # Check.
        # There should be nothing on stdout, but something on stderr.
        self.assertTrue(out[0].startswith(b'L U U N N 4 43 80 0 0 0x0 '))
        self.assertEqual(b'Wait(): Host disconnected\n', out[1])

if __name__ == '__main__':
    unittest.main()
