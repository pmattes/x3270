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
# s3270 HTTPS tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import Common.Test.ct as ct

class TestS3270Httpd(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # s3270 HTTPD persist test.
    def test_s3270_httpd_persist(self):

        # Start s3270.
        port, ts = ct.unused_port()
        s3270 = Popen(ct.vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        ct.check_listen(port)
        ts.close()

        # Start a requests session and do a get.
        # Doing the get within a session keeps the connection alive.
        s = requests.Session()
        r = s.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)')
        self.assertEqual(requests.codes.ok, r.status_code)
        self.assertEqual('false', r.json()['result'][0])

        # Do it again.
        r = s.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)')
        self.assertEqual(requests.codes.ok, r.status_code)
        self.assertEqual('false', r.json()['result'][0])

        # Make sure the connection pool is non-empty.
        self.assertNotEqual(0, len(r.connection.poolmanager.pools.keys()))

        # Wait for the process to exit successfully.
        s.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        s.close()
        ct.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
