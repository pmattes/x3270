#
#!/usr/bin/env python3
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
# b3270 APL underscore tests

import json
from subprocess import Popen, PIPE
import unittest
import xml.etree.ElementTree as ET

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

class TestB3270AplUnderscore(cti):

    # b3270 APL underscore test
    def test_b3270_apl_underscore(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo.trc', port=pport) as p:
            socket.close()

            b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Connect to playback.
            j = { "run": { "actions": { "action": "open", "args": [ f"localhost:{pport}" ] } } }
            b3270.stdin.write(f'"Open(localhost:{pport})"\n'.encode())
            b3270.stdin.flush()

            # Paint the screen.
            p.send_records(1)

            # Get the result.
            pq = pipeq.pipeq(self, b3270.stdout)
            formatted = False
            while True:
                line = pq.get(2, 'b3270 did not produce expected output')
                self.assertNotEqual(b'', line)
                d = line.decode()
                if d == '{"formatted":{"state":true}}':
                    formatted = True
                elif formatted and d.startswith('{"screen":{"cursor":{"enabled":true,'):
                    break

            # Wait for the processes to exit.
            b3270.stdin.close()
            b3270.stdout.close()
            self.vgwait(b3270)
            pq.close()

        # Check.
        want = '{"screen":{"cursor":{"enabled":true,"row":14,"column":2},"rows":[{"row":1,"changes":[{"column":1,"fg":"neutralWhite","gr":"highlight,selectable","text":" APL characters"},{"column":16,"fg":"neutralWhite","gr":"highlight,selectable","count":65}]},{"row":2,"changes":[{"column":2,"text":"Using SA"},{"column":13,"gr":"underline,private-use","text":"ABCD"},{"column":17,"text":"áãå"},{"column":20,"gr":"underline,private-use","text":"HI"}]},{"row":3,"changes":[{"column":2,"text":"Using SFE"},{"column":13,"gr":"underline,private-use","text":"JKLMNOPQR"}]},{"row":14,"changes":[{"column":1,"fg":"green","count":80}]}]}}'
        self.assertEqual(want, line.decode())

if __name__ == '__main__':
    unittest.main()
