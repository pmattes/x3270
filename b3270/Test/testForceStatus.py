#!/usr/bin/env python3
#
# Copyright (c) 2025-2026 Paul Mattes.
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
# b3270 ForceStatus tests

import json
from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

class TestB3270ForceStatus(cti):

    # b3270 ForceStatus error test.
    def b3270_ForceStatus_error(self, input: str, output: str):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        pq = pipeq.pipeq(self, b3270.stdout)
        pq.get(2, 'b3270 did not start')

        # Send input with control characters to b3270 and make sure they are expanded.
        b3270.stdin.write(f'"{input}"\n'.encode('utf8'))
        b3270.stdin.flush()

        # Check for a pop-up about the bad address.
        out = pq.get(2, 'b3270 did not produce error')
        outj = json.loads(out.decode('utf8'))['run-result']['text'][0]
        self.assertEqual(output, outj)

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()
    
    def test_b3270_FS_unknown_reason(self):
        self.b3270_ForceStatus_error('ForceStatus(abc\u0001def)', "ForceStatus(): Unknown reason 'abc^Adef'")

    def test_b3270_FS_missing_arg1(self):
        self.b3270_ForceStatus_error('ForceStatus(oerr)', "ForceStatus(): Reason 'oerr' requires an argument")

    def test_b3270_FS_bad_oerr(self):
        self.b3270_ForceStatus_error('ForceStatus(oerr,abc\u0001def)', "ForceStatus(): Unknown oerr type 'abc^Adef'")

    def test_b3270_FS_missing_arg2(self):
        self.b3270_ForceStatus_error('ForceStatus(scrolled)', "ForceStatus(): Reason 'scrolled' requires an argument")

    def test_b3270_FS_bad_scrolled(self):
        self.b3270_ForceStatus_error('ForceStatus(scrolled,abc\u0001def)', "ForceStatus(): Invalid scrolled amount 'abc^Adef'")

    def test_b3270_FS_extra_arg(self):
        self.b3270_ForceStatus_error('ForceStatus(syswait,abc\u0001def)', "ForceStatus(): Reason 'syswait' does not take an argument")

if __name__ == '__main__':
    unittest.main()
