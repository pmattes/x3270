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
# b3270 crash test

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *

@unittest.skipUnless('CRASH' in os.environ, 'Test used to exercise test infra')
@requests_timeout
class TestB3270Crash(cti):

    # b3270 crash test
    def test_b3270_crash(self):

        # Start b3270.
        hport, socket = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-httpd', str(hport)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        self.check_listen(hport)

        # Force it to crash.
        # The HTTP request will fail and the test will bomb out. When the cti
        # infra cleans up the child process, it will also bomb out, because
        # the child was killed by a signal.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Crash(null)', timeout=5)
        self.assertTrue(r.ok)

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

if __name__ == '__main__':
    unittest.main()
