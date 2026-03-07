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
# s3270 Set() tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Query(cti):

    # s3270 Set() test with control characters in the value.
    def test_s3270_set_command_line_cc(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}', '-user', 'x \x01y\nz']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query the value we set from the command line.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(user)')
        self.assertTrue(r.ok)
        result = r.json()['result'][0]
        self.assertEqual('x ^Ay^Jz', result)

        # Query everything and make sure that value is correct.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set()')
        self.assertTrue(r.ok)
        result = r.json()['result']
        user = [line for line in result if line.startswith('user: ')]
        self.assertEqual(1, len(user))
        self.assertEqual('user: x ^Ay^Jz', user[0])

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Set() error tests with control characters in the names.
    def test_s3270_set_bad_cc(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query a nonexistent name, with control characters in it.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(xyz\x01\x02)')
        self.assertFalse(r.ok)
        result = r.json()['result'][0]
        self.assertEqual("Set(): Unknown toggle name 'xyz^A^B'", result)

        # Fail to provide a value for a name with control characters in it.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(user,foo,xyz\x01\x02)')
        self.assertFalse(r.ok)
        result = r.json()['result'][0]
        self.assertEqual("Set(): 'xyz^A^B' requires a value", result)

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
