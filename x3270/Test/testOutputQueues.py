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
# x3270 output queue config tests

from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *

class TestX3270OutputQueue(cti):

    # x3270 output queue config test
    def x3270_oq(self, spec: str, expect: str, stderr=False):

        # Start x3270.
        args=['x3270', '-script']
        if spec != None:
            args += ['-set', f'outputQueues={spec}']
        x3270 = Popen(vgwrap(args), stdin=PIPE, stdout=PIPE, stderr=PIPE)
        self.children.append(x3270)

        # Feed x3270 one action.
        got = x3270.communicate(input=b'Show(OutputQueues)\n', timeout=2)

        # Wait for the processes to exit.
        self.vgwait(x3270)

        line1 = got[0].splitlines()[0].decode('utf8')
        self.assertEqual('data: ' + expect, line1)
        err = got[1]
        if stderr:
            self.assertNotEqual(b'', err)
        else:
            self.assertEqual(b'', err)
    
    def test_x3270_oq_1m(self):
        self.x3270_oq('1m', 'queueing enabled limit 976KiB')
    def test_x3270_oq_bad(self):
        self.x3270_oq('1mx', 'queueing enabled limit 10MiB', stderr=True)

if __name__ == '__main__':
    unittest.main()
