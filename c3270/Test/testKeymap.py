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
# c3270 keymap tests

import os
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import unittest

from Common.Test.cti import *

@unittest.skipIf(sys.platform.startswith('win'), "Windows does not support PTYs")
class TestC3270Keymap(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                os.read(fd, 1024)
            except:
                return

    # c3270 keymap dump test.
    def test_c3270_keymap_dump(self):

        # Fork a child process with a PTY between this process and it.
        c3270_port, ts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            ts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            keymap = '<Key>space: Key(a)\n<Key>colon: Key(b)\nAlt<Key>x: Key(z)'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{c3270_port}',
                              '-xrm', 'c3270.keymap.test: ' + keymap, '-e', '/bin/sh']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Start a thread to drain c3270's output.
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        ts.close()

        # Dump the keymap, and make sure ours is at the top.
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Keymap(test)')
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Query(Keymap)')
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(['[test:1 temp] space: Key(a)', '[test:2 temp] colon: Key(b)', '[test:3 temp] Alt<Key>x: Key(z)'], result[0:3])

        # Send alt-q cause c3270 to exit.
        os.write(fd, b'\x01q')

        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()

if __name__ == '__main__':
    unittest.main()
