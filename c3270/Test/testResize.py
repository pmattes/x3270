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
# c3270 resize tests

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
class TestC3270Resize(cti):

    # c3270 test for a too-small console window, host on the command line.
    def test_c3270_too_small_cmdline(self):

        playback_port, pts = unused_port()
        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            termios.tcsetwinsize(0, (22, 79))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8',
                    '-httpd', f'127.0.0.1:{c3270_port}', '-set', 'retry', f'127.0.0.1:{playback_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        # Start 'playback' to feed c3270.
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            pts.close()
            cts.close()

            # The child process is supposed to exit, because the window is too small.
            self.vgwait_pid(pid, assertOnFailure=False)

            # Collect the output.
            result = ''
            while True:
                try:
                    rbuf = os.read(fd, 1024)
                except OSError:
                    break
                result += rbuf.decode('utf8')

            self.assertIn("won't fit", result)

            os.close(fd)
            p.abandon()

    # Read the PTY until we get a match.
    # Returns any leftover text.
    def read_until(self, fd: int, match: str, timeout=2, xtra=''):
        result = xtra
        if match in result:
            return re.sub(f'(?s).*{match}', '', result, count=1)
        t0 = time.monotonic()
        while True:
            (r, w, x) = select.select([fd], [], [fd], timeout)
            self.assertTrue(r != [] or w != [] or x != [])
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                return None
            # print('got', rbuf.decode().replace('\033', '<ESC>'))
            result += rbuf.decode('utf8')
            if match in result:
                return re.sub(f'(?s).*{match}', '', result, count=1)
            timeout -= time.monotonic() - t0
            self.assertGreater(timeout, 0.0)

    # Read the PTY until we don't get a match.
    def read_until_not(self, fd: int, match: str, timeout=2):
        result = ''
        t0 = time.monotonic()
        while True:
            (r, w, x) = select.select([fd], [], [fd], timeout)
            if r == [] and w == [] and x == []:
                return
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                return None
            result += rbuf.decode('utf8')
            self.assertNotIn(match, result)
            timeout -= time.monotonic() - t0
            if timeout <= 0:
                return

    # c3270 test for a too-small console window, explicit 'open' operation from the prompt.
    def test_c3270_too_small_prompt(self):

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            termios.tcsetwinsize(0, (22, 79))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8',
                    '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        # Start 'playback' to feed c3270.
        playback_port, pts = unused_port()
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            pts.close()
            cts.close()
            os.set_blocking(fd, False)

            # Make sure c3270 started.
            self.check_listen(c3270_port)

            # Tell c3270 to connect, then make sure it bombed out, but went to the prompt rather than exiting.
            os.write(fd, f'open 127.0.0.1:{playback_port}\n'.encode())
            self.read_until(fd, "won't fit")
            os.write(fd, b'quit\n')

            # Clean up.
            p.abandon()
            self.vgwait_pid(pid)
            os.close(fd)

    def get_terminal_size(self, c3270_port: int):
        '''Gets the terminal size from c3270'''
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Query(Curses)')
        self.assertTrue(r.ok)
        reply = r.json()['result'][1].split(' ')
        return (int(reply[2]), int(reply[4]))
    
    def get_connection_state(self, c3270_port: int):
        '''Gets the connection state from c3270'''
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Query(ConnectionState)')
        self.assertTrue(r.ok)
        return r.json()['result'][0]
    
    def drain(self, fd: int, timeout=0.5):
        '''Drain the PTY'''
        while True:
            (r, w, x) = select.select([fd], [], [fd], timeout)
            if r == []:
                return
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                return

    # c3270 test for growing a too-small console window and successfully connecting.
    def test_c3270_too_small_recover(self):

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            termios.tcsetwinsize(0, (22, 79))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8',
                    '-httpd', f'127.0.0.1:{c3270_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        # Start 'playback' to feed c3270.
        playback_port, pts = unused_port()
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            pts.close()
            cts.close()
            os.set_blocking(fd, False)

            # Make sure c3270 started.
            self.check_listen(c3270_port)

            # Tell c3270 to connect, then make sure it bombed out, but went to the prompt rather than exiting.
            self.drain(fd)
            os.write(fd, f'open 127.0.0.1:{playback_port}\n'.encode())
            xtra = self.read_until(fd, "won't fit")
            xtra = self.read_until(fd, 'c3270>', xtra=xtra)

            # Resize the window so it will fit, then try connecting again.
            new_size = (24, 80)
            termios.tcsetwinsize(fd, new_size)
            self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
            os.write(fd, f'open 127.0.0.1:{playback_port}\n'.encode())
            self.try_until(lambda: self.get_connection_state(c3270_port) != 'not-connected', 2, 'Expected connection state to change')
            p.send_records(5)
            self.read_until(fd, 'desired product or service', xtra=xtra)

            # Get back to the prompt.
            # This could be done more easily with an HTTP operation, but this is more fun.
            self.drain(fd)
            os.write(fd, b'\035')
            self.read_until(fd, 'c3270>')

            # Clean up.
            os.write(fd, b'quit\n')
            self.vgwait_pid(pid)
            os.close(fd)

    # c3270 test for squeezing out the space under the menubar and over the OIA.
    def test_c3270_squeeze(self):

        # Create a dummy server to talk to -- just a listening socket.
        playback_port, pts = unused_port()
        pts.close()
        s = socket.socket()
        s.bind(('127.0.0.1', playback_port))
        s.listen()

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            termios.tcsetwinsize(0, (47, 80))
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-utf8', '-httpd', f'127.0.0.1:{c3270_port}', f'127.0.0.1:{playback_port}']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.
        cts.close()
        os.set_blocking(fd, False)

        # Make sure c3270 started.
        self.check_listen(c3270_port)

        # Wait for the connection to be set up.
        self.try_until(lambda: self.get_connection_state(c3270_port) != 'not-connected', 2, 'Expected connection')

        # Shrink the window by one line, which should work.
        new_size = (46, 80)
        termios.tcsetwinsize(fd, new_size)
        self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
        self.drain(fd)
        self.read_until_not(fd, 'c3270>', timeout=0.2)

        # Three more should work.
        new_size = (45, 80)
        termios.tcsetwinsize(fd, new_size)
        self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
        self.drain(fd)
        self.read_until_not(fd, 'c3270>', timeout=0.2)
        new_size = (44, 80)
        termios.tcsetwinsize(fd, new_size)
        self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
        self.drain(fd)
        self.read_until_not(fd, 'c3270>', timeout=0.2)
        new_size = (43, 80)
        termios.tcsetwinsize(fd, new_size)
        self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
        self.drain(fd)
        self.read_until_not(fd, 'c3270>', timeout=0.2)

        # The fifth line should not work.
        new_size = (42, 80)
        termios.tcsetwinsize(fd, new_size)
        self.try_until(lambda: self.get_terminal_size(c3270_port) == new_size, 2, 'Expected terminal size to change')
        self.read_until(fd, 'c3270>')

        # Clean up.
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Quit()')
        self.vgwait_pid(pid)
        os.close(fd)
        s.close()

if __name__ == '__main__':
    unittest.main()
