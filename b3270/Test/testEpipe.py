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
# b3270 stdout EPIPE tests

import json
from subprocess import Popen, PIPE
import sys
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
import Common.Test.setupCert as setupCert
import Common.Test.tls_server as tls_server

class TestB3270Epipe(cti):

    # b3270 TLSEPIPE test
    def test_b3270_tls(self):

        # Start b3270.
        args = ['b3270', '-json']
        b3270 = Popen(vgwrap(args), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        pq = pipeq.pipeq(self, b3270.stdout, limit=1)
        pq.get(2, 'b3270 did not start')

        # Close stdout.
        pq.close()
        b3270.stdout.close()

        # Generate a lot of output.
        b3270.stdin.write(b'"readbuffer"\n')
        b3270.stdin.flush()

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

if __name__ == '__main__':
    unittest.main()
