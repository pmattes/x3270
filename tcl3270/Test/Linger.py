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
# tcl3270 smoke tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import psutil
import requests
import os
import signal
import TestCommon

class TestTcl3270Linger(unittest.TestCase):

    # tcl3270 linger test, making sure s3270 exits when tcl3270 does.
    def test_tcl3270_linger1(self):

        # Start tcl3270.
        tcl3270 = Popen(["tcl3270", "tcl3270/Test/linger.tcl", "--",
            "-httpd", "127.0.0.1:9980"],
            stdin=DEVNULL, stdout=DEVNULL)
        TestCommon.check_listen(9980)

        # Make sure it is blocked.
        def test():
            j = requests.get(f'http://127.0.0.1:9980/3270/rest/json/Query(Tasks)').json()
            return any('Wait(' in line for line in j['result'])
        TestCommon.try_until(test, 2, "emulator did not block")

        # Find the copy of s3270 it is running.
        children = psutil.Process(tcl3270.pid).children()
        self.assertEqual(1, len(children))
        s3270 = children[0]
        self.assertEqual('s3270', s3270.name())

        # Kill tcl3270.
        tcl3270.kill()
        tcl3270.wait()

        # Make sure s3270 is gone, too.
        def test2():
            return 'terminated' in str(s3270)
        TestCommon.try_until(test2, 2, "s3270 did not exit")

    # tcl3270 linger test, making sure tcl3270 exits when s3270 does.
    def test_tcl3270_linger2(self):

        # Start tcl3270.
        tcl3270 = Popen(["tcl3270", "tcl3270/Test/linger2.tcl", "--",
            "-httpd", "127.0.0.1:9981"],
            stdin=DEVNULL, stdout=DEVNULL)
        TestCommon.check_listen(9981)

        # Find the copy of s3270 it is running.
        children = psutil.Process(tcl3270.pid).children()
        self.assertEqual(1, len(children))
        s3270 = children[0]
        self.assertEqual('s3270', s3270.name())

        # Kill s3270.
        os.kill(s3270.pid, signal.SIGTERM)

        # Make sure tcl3270 is gone, too.
        exit_code = tcl3270.wait(timeout=2)
        self.assertEqual(98, exit_code)

if __name__ == '__main__':
    unittest.main()
