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
# c3270 ibm_hosts tests

import os
import sys
if not sys.platform.startswith('win'):
    import pty
import tempfile
import threading
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(sys.platform.startswith('win'), "Windows uses different c3270 graphic tests")
@requests_timeout
class TestC3270IbmHosts(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                os.read(fd, 1024)
            except:
                return

    # c3270 ibm_hosts case sensitivity test
    def test_c3270_ibm_hosts_ci(self):

        playback_port, pts = unused_port()

        # Create an ibm_hosts file that points to playback.
        (handle, hostsfile_name) = tempfile.mkstemp()
        os.close(handle)
        with open(hostsfile_name, 'w') as f:
            f.write(f"fooey primary a:c:t:127.0.0.1:{playback_port}\n")

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2',
                    '-httpd', f'127.0.0.1:{c3270_port}',
                    '-hostsfile', hostsfile_name]), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Start a thread to drain c3270's output.
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        cts.close()

        # Start 'playback' to read c3270's output.
        p = playback(self, 's3270/Test/ibmlink.trc', port=playback_port)
        pts.close()

        # Make sure c3270 is connected.
        os.write(fd, b'open fOOey\r')
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Wait(2,inputField)')
        self.assertTrue(r.ok)
        cs = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Query(connectionState)').json()['result'][0]
        self.assertEqual('connected-nvt', cs)
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        p.close()
        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()
        os.unlink(hostsfile_name)

if __name__ == '__main__':
    unittest.main()
