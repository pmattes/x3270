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
# c3270 prompt tests

import os
import os.path
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(sys.platform.startswith('win'), "Windows does not support PTYs")
@requests_timeout
class TestC3270Prompt(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                b = os.read(fd, 1024)
                self.drain_input += b.decode()
            except:
                return
    
    def expect(self, timeout, fd: int, text: str, flush=True):
        '''Expect simple output from c3270'''
        if flush:
            self.drain_input = ''
        self.try_until(lambda: text in self.drain_input, timeout, f'expected "{text}"')

    # c3270 prompt open test
    def test_c3270_prompt_open(self):

        playback_port, pts = unused_port()

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            pts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        cts.close()

        # Start 'playback' to feed c3270.
        p = playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port)
        pts.close()

        # Start a thread to drain c3270's output.
        self.drain_input = ''
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Send an Open command to c3270.
        os.write(fd, f'Open(127.0.0.1:{playback_port})\r'.encode('utf8'))

        # Write the stream to c3270.
        p.send_records(7)

        # Break to the prompt and send a Quit command to c3270.
        os.write(fd, b'\x1d')
        time.sleep(0.1)
        os.write(fd, b'Quit()\r')
        p.close()

        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()

    # c3270 interactive file transfer test ('other' option)
    def test_c3270_prompt_ft_other(self):

        playback_port, pts = unused_port()

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            env = os.environ.copy()
            env['TERM'] = 'dumb'
            env['PAGER'] = 'none'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        cts.close()

        # Start a thread to drain c3270's output.
        self.drain_input = ''
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Start 'playback' to talk to c3270.
        p = playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port)
        pts.close()

        # Connect c3270 to playback.
        self.expect(2, fd, 'c3270> ', flush=False)
        os.write(fd, f'Connect(127.0.0.1:{playback_port})\r'.encode())

        # Write the stream to c3270.
        p.send_records(6)

        # Wait for the connection to finish.
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Wait(2,InputField)')
        self.assertTrue(r.ok, 'Connection did not complete')

        # Tab to the big field, break to the prompt and send an interactive Transfer() action to c3270.
        os.write(fd, b'\t\t\t\x1d')
        self.expect(2, fd, 'c3270> ')
        os.write(fd, b'Transfer()\r')
        self.expect(2, fd, 'Continue? ')

        # Answer mostly with defaults, sending /etc/group to ETC GROUP A on a VM host.
        os.write(fd, b'\r')
        self.expect(2, fd, '[receive] ')

        os.write(fd, b'send\r')
        self.expect(2, fd, 'source file on this workstation: ')

        os.write(fd, b'/etc/group\r')
        self.expect(2, fd, 'on the host: ')

        os.write(fd, b'ETC GROUP A\r')
        self.expect(2, fd, '[tso] ')

        os.write(fd, b'vm\r')
        self.expect(2, fd, '[ascii] ')

        os.write(fd, b'\r')
        self.expect(2, fd, '[remove] ')

        os.write(fd, b'\r')
        self.expect(2, fd, '[yes] ')

        os.write(fd, b'\r')
        self.expect(2, fd, '[default] ')

        os.write(fd, b'\r')
        self.expect(2, fd, '[16384] ')

        os.write(fd, b'\r')
        self.expect(2, fd, 'Other IND$FILE options: [] ')

        # Specify BAZ as an extra IND$FILE option, and make sure it is echoed back in the summary.
        os.write(fd, b'BAZ\r')
        self.expect(2, fd, 'Continue? (y/n) [y] ')
        
        # Go ahead, and make sure BAZ is specified (it's X'C2C1E9' in the text).
        os.write(fd, b'\r')
        want = bytes.fromhex('00000000007d5d5e11d94c6d6d6d6d6d6d6d6d11d95f6d6d6d6d6d6d6d6d115cf6c9d5c45bc6c9d3c540d7e4e340c5e3c340c7d9d6e4d740c1404dc1e2c3c9c940c3d9d3c640c2c1e9115df6ffef')
        cmd = p.nread(len(want))
        self.assertEqual(want, cmd, 'Expected specific output')

        p.close()
        os.write(fd, b'quit\r')

        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()

if __name__ == '__main__':
    unittest.main()
