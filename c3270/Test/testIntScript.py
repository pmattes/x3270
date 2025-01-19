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
# c3270 tests for interactive scripts

import os
import os.path
import select
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import time
import unittest

from Common.Test.cti import *
import Common.Test.playback as playback

@unittest.skipIf(sys.platform == "darwin", "Not ready for c3270 graphic tests")
@unittest.skipIf(sys.platform.startswith('win'), "Windows uses different c3270 graphic tests")
@requests_timeout
class TestC3270IntScript(cti):

    # Used to stop the timeout thread.
    stop_timeout = False

    # Push a script command to c3270.
    def push_command(self, port: int, prompt: bool):
        prompt_arg = '' if prompt else '-nopromptafter,'
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Escape({prompt_arg}"Script(-interactive,c3270/Test/ask.py)")')
        self.assertTrue(r.ok)

    # c3270 interactive script test
    def c3270_interactive_script_test(self, prompt:bool):

        # Fork c3270 with a PTY between this process and it.
        c3270_port, ts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            ts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8', '-secure',
                    '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        ts.close()

        # In the background, tell c3270 to run an interactive script.
        request_thread = threading.Thread(target=self.push_command, args=[c3270_port, prompt])
        request_thread.start()

        # Collect output until we get the prompt from the script.
        self.wait_for_pty_output(2, fd, 'Input: ')

        # Tell the script to proceed.
        os.write(fd, b'\n')
        request_thread.join(timeout=2)

        # Wait for the prompt.
        if prompt:
            self.wait_for_pty_output(2, fd, '[Press <Enter>]')

        # Tell c3270 to exit.
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Quit())')

        self.vgwait_pid(pid)
        os.close(fd)
    
    def test_c3270_interactive_script_noprompt(self):
        self.c3270_interactive_script_test(prompt=False)
    def test_c3270_interactive_script_prompt(self):
        self.c3270_interactive_script_test(prompt=True)

    
        
    # c3270 bad interactive script test
    def test_c3270_interactive_script_wrong(self):

        # Fork c3270 with a PTY between this process and it.
        c3270_port, ts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            ts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8', '-secure',
                    '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        ts.close()

        # Tell c3270 to run an interactive script with invalid arguments.
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Script(-interactive,-async,"Foo,bar"))')
        self.assertFalse(r.ok, 'Expected a syntax error')

        # Tell c3270 to exit.
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Quit())')

        self.vgwait_pid(pid)
        os.close(fd)

if __name__ == '__main__':
    unittest.main()
