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
# b3270 DBCS wrap tests

import json
import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

class TestB3270DbcsWrap(cti):

    # b3270 DBCS wrap test
    def test_b3270_dbcs_wrap(self):

        japanese_text = "国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし"
        pport, ts = unused_port()
        with playback(self, 's3270/Test/dbcs-wrap.trc', port=pport) as p:
            ts.close()
            
            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json', '-codepage', 'japanese-latin']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)
            pq = pipeq.pipeq(self, b3270.stdout)

            # Connect to playback.
            b3270.stdin.write(f'"open 127.0.0.1:{pport}"\n'.encode())
            b3270.stdin.flush()

            # Fill in the screen.
            p.send_records(2)

            # Flush all of the output past the open.
            while True:
                s = pq.get(5, 'did not get open run-result')
                if s.decode().startswith('{"run-result":{'):
                    break

            # Enter some text.
            b3270.stdin.write(f'"string {japanese_text}"\n'.encode())
            b3270.stdin.flush()

            # Wait for the screen update.
            while True:
                s = pq.get(5, 'did not get screen update')
                if s.decode().startswith('{"screen":{'):
                    break
            
            # Verify that the split character is rendered properly.
            ret = json.loads(s.decode())
            rows = ret['screen']['rows']
            self.assertEqual([{'row': 22, 'changes': [{'column': 6, 'gr': 'wide', 'text': japanese_text[:37]}, {'column': 80, 'gr': 'left-half', 'text': '取'}]},
                {'row': 23, 'changes': [{'column': 1, 'gr': 'no-copy,right-half', 'text': '取'}, {'column': 2, 'gr': 'wide', 'text': japanese_text[38:]}]}],
                rows)
            
        # Wait for the processes to exit.
        b3270.stdin.close()
        pq.close()
        b3270.stdout.close()
        self.vgwait(b3270)

if __name__ == '__main__':
    unittest.main()
