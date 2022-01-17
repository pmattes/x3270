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
# tcl3270 smoke tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import os
import filecmp
import TestCommon

class TestTcl3270Smoke(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # tcl3270 3270 smoke test
    def test_tcl3270_smoke(self):

        # Start 'playback' to feed data to tcl3270.
        playback_port, ts = TestCommon.unused_port()
        playback = Popen(["playback", "-w", "-p", str(playback_port),
            "s3270/Test/ibmlink.trc"], stdin=PIPE, stdout=DEVNULL)
        self.children.append(playback)
        TestCommon.check_listen(playback_port)
        ts.close()

        # Create a temporary file.
        (handle, name) = tempfile.mkstemp()

        # Start tcl3270.
        tcl_port, ts = TestCommon.unused_port()
        tcl3270 = Popen(["tcl3270", "tcl3270/Test/smoke.tcl", name, "--",
            "-xrm", "tcl3270.contentionResolution: false",
            "-httpd", f"127.0.0.1:{tcl_port}",
            f"127.0.0.1:{playback_port}"],
            stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(playback)
        TestCommon.check_listen(tcl_port)
        ts.close()

        # Send a screenful to tcl3270.
        playback.stdin.write(b'r\nr\nr\nr\n')
        playback.stdin.flush()
        TestCommon.check_push(playback, tcl_port, 1)

        # Wait for the file to show up.
        def Test():
            return os.lseek(handle, 0, os.SEEK_END) > 0
        TestCommon.try_until(Test, 2, "Script did not produce a file")
        os.close(handle)

        # Wait for the processes to exit.
        playback.stdin.close()
        playback.kill()
        playback.wait(timeout=2)
        tcl3270.kill()
        tcl3270.wait(timeout=2)

        # Compare the files
        self.assertTrue(filecmp.cmp(name, 'tcl3270/Test/smoke.txt'))
        os.unlink(name)

if __name__ == '__main__':
    unittest.main()
