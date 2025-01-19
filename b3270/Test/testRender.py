#!/usr/bin/env python3
#
# Copyright (c) 2022-2025 Paul Mattes.
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
# b3270 rendering bug tests

import json
from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

class TestB3270Render(cti):

    # b3270 invisible underscore test
    def test_invisible_underscore(self):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, f'b3270/Test/invisible_underscore.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send the screen image.
            p.send_records(1)

            # Check for a screen update.
            while True:
                out = pq.get(2, 'b3270 did not produce screen output')
                if b'Field:' in out:
                    break
            self.assertNotIn(b'underline', out)

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # b3270 reverse video test
    def test_reverse(self):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, f'b3270/Test/reverse.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send the screen image.
            p.send_records(1)

            # Check for a screen update.
            while True:
                out = pq.get(2, 'b3270 did not produce screen output')
                if b'_____' in out:
                    break

            # Check the output.
            row1 = json.loads(out.decode('utf8'))['screen']['rows'][0]['changes']
            change1 = row1[0]
            self.assertNotIn('bg', change1)
            change2 = row1[1]
            self.assertIn('bg', change2)
            self.assertEqual('red', change2['bg'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

if __name__ == '__main__':
    unittest.main()
