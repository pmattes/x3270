#!/usr/bin/env python3
#
# Copyright (c) 2021 Paul Mattes.
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
# c3270 smoke tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import pty
import os
import time
import re
import TestCommon

class TestC3270Smoke(unittest.TestCase):

    # c3270 3270 smoke test
    def test_c3270_3270_smoke(self):

        # Start 'playback' to read s3270's output.
        playback = Popen(["playback", "-w", "-p", "9998",
            "c3270/Test/ibmlink2.trc"], stdin=PIPE, stdout=DEVNULL)
        TestCommon.check_listen(9998)

        # Fork a child process with a PTY between this process and it.
        os.environ['TERM'] = 'xterm-256color'
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            os.execlp("c3270", "c3270", "-model", "2", "-utf8",
                "-httpd", "127.0.0.1:9999", "127.0.0.1:9998")

        # Parent process.
        
        # Make sure c3270 started.
        TestCommon.check_listen(9999)

        # Write the stream to c3270.
        playback.stdin.write(b'r\nr\nr\nr\nr\n')
        playback.stdin.flush()
        TestCommon.check_push(playback, 9999, 1)
        playback.stdin.write(b'e\n')
        playback.stdin.flush()

        # Collect the output.
        result = ''
        while True:
            try:
                rbuf = os.read(fd, 1024)
            except OSError:
                break
            result += rbuf.decode('utf8')
        
        # Make the output a bit more readable and split it into lines.
        # Then replace the text that varies with the build with tokens.
        result = result.replace('\x1b', '<ESC>').split('\n')
        result[0] = re.sub(' v.*\r', '<version>\r', result[0], count=1)
        result[1] = re.sub(' 1989-.* by', ' <years> by', result[1], count=1)
        rtext = '\n'.join(result)
        if 'GENERATE' in os.environ:
            # Use this to regenerate the template file.
            file = open(os.environ['GENERATE'], "w")
            file.write(rtext)
            file.close()
        else:
            # Compare what we just got to the reference file.
            file = open("c3270/Test/smoke.txt", "r", newline='')
            ctext = file.read()
            file.close()
            self.assertEqual(rtext, ctext)

        playback.stdin.close()
        playback.kill()
        playback.wait(timeout=2)
        os.waitpid(pid, 0)

if __name__ == '__main__':
    unittest.main()
