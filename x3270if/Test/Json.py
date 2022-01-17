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
# x3270if JSON tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import json
import TestCommon

class TestX3270ifJson(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # x3270if basic JSON test
    def test_x3270if_json(self):

        # Start a copy of s3270 to talk to.
        port, ts = TestCommon.unused_port()
        s3270 = Popen(["s3270", "-scriptport", f"127.0.0.1:{port}"],
                stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        TestCommon.check_listen(port)
        ts.close()

        # Run x3270if with a trivial JSON query.
        x3270if = Popen(["x3270if", "-t", str(port), '"Set(startTls)"'],
                stdout=PIPE)
        self.children.append(x3270if)

        # Decode the result.
        stdout = x3270if.communicate()[0].decode('utf8')

        # Wait for the processes to exit.
        s3270.kill()
        s3270.wait()

        # Test the output.
        j = json.loads(stdout)
        self.assertEqual('true', j['result'][0])
        self.assertTrue(j['success'])
        self.assertTrue(j['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

if __name__ == '__main__':
    unittest.main()
