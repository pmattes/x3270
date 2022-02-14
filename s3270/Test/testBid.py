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
# draft-04 BID tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import Common.Test.ct as ct

class TestS3270Bid(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # s3270 BID test
    def test_s3270_bid(self):

        # Start 'playback' to read s3270's output.
        port, socket = ct.unused_port()
        playback = Popen(["playback", "-b", "-p", str(port),
            "s3270/Test/bid.trc"], stdout=DEVNULL)
        self.children.append(playback)
        ct.check_listen(port)
        socket.close()

        # Start s3270.
        s3270 = Popen(ct.vgwrap(["s3270", f"127.0.0.1:{port}"]), stdin=PIPE,
                stdout=DEVNULL)
        self.children.append(s3270)

        # Feed s3270 some actions.
        s3270.stdin.write(b"PF(3)\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Wait for the processes to exit.
        rc = playback.wait(timeout=2)
        self.assertEqual(rc, 0)
        s3270.stdin.close()
        ct.vgwait(s3270)

    # s3270 no-BID test
    def test_s3270_no_bid(self):

        # Start 'playback' to read s3270's output.
        port, socket = ct.unused_port()
        playback = Popen(["playback", "-b", "-p", str(port),
            "s3270/Test/no_bid.trc"], stdout=DEVNULL)
        self.children.append(playback)
        ct.check_listen(port)
        socket.close()

        # Start s3270.
        s3270 = Popen(ct.vgwrap(["s3270", "-xrm", "s3270.contentionResolution: false",
            f"127.0.0.1:{port}"]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(s3270)

        # Feed s3270 some actions.
        s3270.stdin.write(b"PF(3)\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Wait for the processes to exit.
        rc = playback.wait(timeout=2)
        self.assertEqual(rc, 0)
        s3270.stdin.close()
        ct.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
