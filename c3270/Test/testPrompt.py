#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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

import unittest
from subprocess import Popen, PIPE, DEVNULL
import sys
if not sys.platform.startswith('win'):
    import pty
import os
import os.path
import time
import Common.Test.playback as playback
import Common.Test.cti as cti

@unittest.skipIf(sys.platform.startswith('win'), "Windows does not support PTYs")
class TestC3270Prompt(cti.cti):

    # c3270 prompt open test
    def test_c3270_3270_smoke(self):

        # Start 'playback' to read s3270's output.
        playback_port, ts = cti.unused_port()
        with playback.playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            ts.close()

            # Fork a child process with a PTY between this process and it.
            c3270_port, ts = cti.unused_port()
            os.environ['TERM'] = 'xterm-256color'
            (pid, fd) = pty.fork()
            if pid == 0:
                # Child process
                ts.close()
                os.execvp(cti.vgwrap_ecmd('c3270'),
                    cti.vgwrap_eargs(['c3270', '-httpd', f'127.0.0.1:{c3270_port}']))
                self.assertTrue(False, 'c3270 did not start')

            # Parent process.

            # Make sure c3270 started.
            self.check_listen(c3270_port)
            ts.close()

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

if __name__ == '__main__':
    unittest.main()
