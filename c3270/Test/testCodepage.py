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
# c3270 codepage tests

import os
import sys
if not sys.platform.startswith('win'):
    import pty
import termios
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(sys.platform.startswith('win'), "Windows uses different c3270 graphic tests")
@requests_timeout
class TestC3270Codepage(cti):

    # c3270 missing codepage test.
    def test_c3270_missing_codepage(self):

        # Fork a child process with a PTY between this process and it.
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            termios.tcsetwinsize(0, (28, 80))
            env = os.environ.copy()
            env['TERM'] = 'xterm'
            os.execvpe(vgwrap_ecmd('c3270'), vgwrap_eargs(['c3270', '-model', '2', '-utf8', '-codepage', 'abc\x01']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Tell c3270 to quit.
        os.write(fd, b'quit\n')
        self.vgwait_pid(pid)

        # Collect the output.
        output = ''
        while True:
            try:
                line = os.read(fd, 1024)
                if line == b'':
                    break
            except OSError:
                break
            output += line.decode('utf8')

        os.close(fd)
        lines = output.splitlines()
        self.assertIn("Warning: Cannot find code page 'abc^A'", lines)

if __name__ == '__main__':
    unittest.main()
