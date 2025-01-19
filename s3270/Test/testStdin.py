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
# s3270 standard input tests

from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

class TestS3270Stdin(cti):

    # s3270 stdin hang test
    @unittest.skipIf(sys.platform.startswith('win'), "POSIX-only test")
    def test_s3270_stdin_hang(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', f'127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            p.send_records(4)

            # Feed s3270 some actions.
            s3270.stdin.write(b"Foo")
            s3270.stdin.flush()

            # Send a timing mark (and expect one back).
            p.send_tm()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
