#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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
# s3270 peer script tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

class TestS3270PeerScript(cti):

    # s3270 peer script back-to-back test.
    def test_s3270_peer_back2back(self):

        # Start s3270.
        sport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', f'127.0.0.1:{sport}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(sport)

        # Throw multiple requests at it in one go.
        s = socket.socket()
        s.connect(('127.0.0.1', sport))
        s.send('''query term
query term
query tasks
quit
'''.encode())
        out = ''
        while True:
            try:
                buf = s.recv(1024)
                if buf == b'':
                    break
            except:
                break
            out += buf.decode();
        s.close()

        # Tidy up.
        self.vgwait(s3270)

        # Check. We expect 5 data lines: The two 'term' results and a 3-line 'tasks' result,
        # showing only the Query() command in progress.
        # There was a bug that left idle commands sitting around from the 'term' queries.
        dlines = [line for line in out.splitlines() if line.startswith('data: ')]
        self.assertEqual(5, len(dlines))

if __name__ == '__main__':
    unittest.main()
