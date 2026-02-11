#
#!/usr/bin/env python3
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
# b3270 pop-up tests

from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

class TestB3270Popups(cti):

    # b3270 stored pop-up test
    def test_b3270_stored_popup(self):

        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', '1.2.3.4:1234']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        pq = pipeq.pipeq(self, b3270.stdout)

        # Get the result. It is generated before b3270's stdout is initialized (and the 'initialize' indication comes out),
        # but it comes out after.
        init = False
        while True:
            line = pq.get(2, 'b3270 did not produce expected output')
            if line.startswith(b'{"initialize":'):
                init = True
            self.assertNotEqual(None, line)
            if line.startswith(b'{"popup":{"type":"error","text":"httpd bind: '):
                break

        # Wait for the processes to exit.
        b3270.stdin.close()
        b3270.stdout.close()
        self.vgwait(b3270)
        pq.close()

        # Check.
        self.assertTrue(init, 'Expected initialize before popup')

if __name__ == '__main__':
    unittest.main()
