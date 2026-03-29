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
# c3270 output queue config tests

import sys
if not sys.platform.startswith('win'):
    import pty
import unittest

from Common.Test.cti import *

@unittest.skipIf(sys.platform.startswith('win'), 'POSIX-specific test')
@requests_timeout
class TestC3270OutputQueue(cti):

    # c3270 output queue config test
    def c3270_oq(self, spec: str, expect: str, stderr=False):

        # Start c3270.
        hport, ts = unused_port()
        args=['c3270', '-httpd', str(hport)]
        if spec != None:
            args += ['-set', f'outputQueues={spec}']
        pid, fd = pty.fork()
        if pid == 0:
            # Child process.
            ts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{hport}', '-set', f'outputQueues={spec}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        ts.close()
        self.check_listen(hport)

        # Feed c3270 one action.
        out = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(OutputQueues)')
        self.assertTrue(out.ok)
        result = out.json()['result'][0]
        self.assertEqual(expect, result)

        # Wait for the processes to exit.
        out = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit')
        self.vgwait_pid(pid)
        out = os.read(fd, 16384).decode('utf8')
        os.close(fd)
        if stderr:
            self.assertIn('Cannot parse', out)
        else:
            self.assertNotIn('Cannot parse', out)

    def test_c3270_oq_1m(self):
        self.c3270_oq('1m', 'queueing enabled limit 976KiB')
    def test_c3270_oq_bad(self):
        self.c3270_oq('1mx', 'queueing enabled limit 10MiB', stderr=True)

if __name__ == '__main__':
    unittest.main()
