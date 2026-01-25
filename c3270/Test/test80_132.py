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
# c3270 80/132 mode-switch tests

import os
import re
import sys
if not sys.platform.startswith('win'):
    import pty
import termios
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(sys.platform.startswith('win'), "Windows uses different c3270 graphic tests")
@requests_timeout
class TestC327080_132(cti):

    # c3270 test for bad altscreen/defscreen syntax.
    def c3270_bad_altscreen(self, spec: str, errmsg: str):

        full_spec = '27x132=' + spec
        # Fork a child process with a PTY between this process and it.
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            termios.tcsetwinsize(0, (22, 79))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8', '-altscreen', full_spec, '-defscreen', full_spec]), env)
            self.assertTrue(False, 'c3270 did not start')

        # The child process is supposed to exit, because of the syntax error.
        self.vgwait_pid(pid, assertOnFailure=False)

        # Collect the output.
        result = ''
        while True:
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                break
            result += rbuf.decode('utf8')

        self.assertIn(errmsg, result)

        os.close(fd)

    def test_c3270_bad_altscreen(self):
        self.c3270_bad_altscreen('\\xq', 'Invalid hex string')
        self.c3270_bad_altscreen('\\x', 'Incomplete hex or octal string')
        self.c3270_bad_altscreen('\\0p', 'Invalid octal string')
        self.c3270_bad_altscreen('\\0', 'Incomplete hex or octal string')
        self.c3270_bad_altscreen('\\', 'Incomplete backslash sequence')

    # c3270 test for new altscreen/defscreen syntax.
    def c3270_fancy_altscreen(self, spec: str):

        full_spec = f'27x132=<{spec}>'
        expect = f'<{spec}>'.replace('\\e', '\033').replace('\\033', '\033').replace('\\x1b', '\033')

        playback_port, pts = unused_port()
        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            termios.tcsetwinsize(0, (43, 80))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8',
                    '-httpd', f'127.0.0.1:{c3270_port}', '-set', 'retry',
                    '-altscreen', full_spec, '-defscreen', full_spec,
                      f'127.0.0.1:{playback_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        cts.close()

        # Start 'playback' to feed c3270.
        with playback(self, 'c3270/Test/alt.trc', port=playback_port) as p:
            pts.close()
            p.send_records(1)

        # Collect the output.
        result = ''
        while True:
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                break
            result += rbuf.decode('utf8')

        self.assertIn(expect, result)

        self.vgwait_pid(pid)
        os.close(fd)

    def test_c3270_fancy_altscreen(self):
        self.c3270_fancy_altscreen('\\e123')
        self.c3270_fancy_altscreen('\\033123')
        self.c3270_fancy_altscreen('\\x1b123')