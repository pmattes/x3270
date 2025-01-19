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
# c3270 hosts file tests

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
class TestC3270HostsFile(cti):

    # Drain the PTY.
    def drain(self, fd):
        while True:
            try:
                os.read(fd, 1024)
            except:
                return

    # c3270 hosts file test
    def test_c3270_hosts_file(self):

        # Set up a hosts file.
        pport, pts = unused_port()
        handle, name = tempfile.mkstemp()
        os.write(handle, f'fubar primary a:c:127.0.0.1:{pport}=gazoo\n'.encode('utf8'))
        os.close(handle)

        # Start c3270.
        hport, ts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            ts.close()
            os.execvp(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', "-utf8",
                    '-httpd', str(hport), '-secure',
                    '-set', f'hostsFile={name}']))
            self.assertTrue(False, 'c3270 did not start')
        self.check_listen(hport)
        ts.close()

        # Start a thread to drain c3270's output.
        drain_thread = threading.Thread(target=self.drain, args=[fd])
        drain_thread.start()

        # Start playback to connect to.
        with playback(self, 's3270/Test/ibmlink.trc', port=pport) as p:
            pts.close()

            # Connect to an alias.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(fubar)', timeout=5)
            self.assertTrue(r.ok)

            p.wait_accept()
            p.close()

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait_pid(pid)
        os.close(fd)
        drain_thread.join()
        os.unlink(name)

if __name__ == '__main__':
    unittest.main()
