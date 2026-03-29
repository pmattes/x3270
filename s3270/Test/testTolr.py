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
# s3270 trace-of-last-resort tests

import os
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *

class TestS3270Tolr(cti):

    # s3270 trace-of-last-resort test
    def test_s3270_tolr(self):

        handle, tracefile = tempfile.mkstemp()
        os.close(handle)

        # Start s3270.
        env = os.environ
        env['X3270_TOLR'] = tracefile
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=DEVNULL, env=env)
        self.children.append(s3270)

        # Tell s3270 to exit.
        s3270.stdin.write(b'Quit()\n')
        s3270.stdin.flush()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

        # Verify what was traced.
        with open(tracefile, 'rb') as file:
            t = file.readlines()
        os.unlink(tracefile)
        started = [line for line in t if b'trace of last resort started' in line]
        self.assertEqual(1, len(started))
        version = [line for line in t if b'Version:' in line]
        self.assertEqual(1, len(version))
        quit = [line for line in t if b'Quit' in line]
        self.assertGreater(len(quit), 0)

if __name__ == '__main__':
    unittest.main()
