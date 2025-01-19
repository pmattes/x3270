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
# x3270if password input tests

import os
from subprocess import Popen, PIPE, DEVNULL
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import unittest

from Common.Test.cti import *

@unittest.skipIf(sys.platform.startswith('win'), "Windows does not support PTYs")
class TestX3270ifPassword(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                b = os.read(fd, 1024)
                # print('drain: got', b)
                self.drain_input += b.decode()
            except:
                return

    def expect(self, timeout, fd: int, text: str):
        '''Expect simple output from x3270if'''
        self.try_until(lambda: self.drain_input.endswith(text), timeout, f'expected "{text}", got "{self.drain_input}"')    

    # x3270if password test
    def test_x3270if_password(self):

        # Start a copy of s3270 to talk to.
        port, ts = unused_port()
        s3270 = Popen(['s3270', '-scriptport', f'127.0.0.1:{port}'], stdout=DEVNULL, stderr=DEVNULL)
        ts.close()
        self.children.append(s3270)
        self.check_listen(port)

        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            env = os.environ.copy()
            env['X3270PORT'] = str(port)
            env['TERM'] = 'dumb'
            env['PAGER'] = 'none'
            os.execvpe(vgwrap_ecmd('x3270if'), vgwrap_eargs(['x3270if', '-I', 's3270']), env)
            self.assertTrue(False, 'x3270if did not start')

        # Parent process.

        # Start a thread to drain x3270if's output.
        self.drain_input = ''
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Wait for a prompt.
        self.expect(2, fd, 's3270> ')

        # Ask for some input.
        os.write(fd, b'RequestInput\r')
        self.expect(2, fd, 'Input: ')
        os.write(fd, b'foo\r')
        self.expect(2, fd, "Input: foo\r\nYou said 'foo'\r\ns3270> ")

        # Ask for some input without echoing.
        os.write(fd, b'RequestInput -noecho\r')
        self.expect(2, fd, "Your last answer was 'foo'\r\nInput: ")
        os.write(fd, b'bar\r')
        self.expect(2, fd, "Input: \r\nYou said 'bar'\r\ns3270> ")

        # Clean up.
        os.write(fd, b'\004')
        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()

        # Wait for the processes to exit.
        s3270.kill()
        self.children.remove(s3270)
        s3270.wait(2)

if __name__ == '__main__':
    unittest.main()
