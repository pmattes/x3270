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
# c3270 Alt-Q tests

import os
import os.path
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import unittest

from Common.Test.cti import *

@unittest.skipIf(sys.platform.startswith('win'), "Windows does not support PTYs")
class TestC3270AltQ(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                os.read(fd, 1024)
            except:
                return

    # c3270 Alt-Q (quit) test.
    def test_c3270_alt_q(self):

        # Fork a child process with a PTY between this process and it.
        c3270_port, ts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            ts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{c3270_port}', '-e', '/bin/sh']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Start a thread to drain c3270's output.
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        ts.close()

        # Send alt-q cause c3270 to exit.
        os.write(fd, b'\x01q')

        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()

if __name__ == '__main__':
    unittest.main()
