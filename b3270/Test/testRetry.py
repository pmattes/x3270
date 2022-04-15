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
# b3270 retry tests

import json
from subprocess import Popen, PIPE, DEVNULL
import unittest

import Common.Test.playback as playback
import Common.Test.cti as cti

class TestB3270Retry(cti.cti):

    # b3270 retry test
    def test_b3270_retry(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, ts = cti.unused_port()

        # Start b3270.
        b3270 = Popen(cti.vgwrap(['b3270', '-set', 'retry', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        self.timed_readline(b3270.stdout, 2, 'b3270 did not start')

        # Tell b3270 to connect.
        b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
        b3270.stdin.flush()

        # Wait for it to try to connect and fail.
        while True:
            out = self.timed_readline(b3270.stdout, 2, 'b3270 did not fail the connection')
            if b'run-result' in out:
                break
        outj = json.loads(out.decode('utf8'))['run-result']
        self.assertEqual(False, outj['success'])
        self.assertEqual(True, outj['retrying'])

        # Start 'playback' to talk to b3270.
        with playback.playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            ts.close()

            # Wait for b3270 to connect.
            p.wait_accept(timeout=6)

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        b3270.stdout.close()

if __name__ == '__main__':
    unittest.main()
