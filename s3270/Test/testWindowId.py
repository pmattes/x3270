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
# s3270 window ID tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *

@unittest.skipUnless(sys.platform.startswith('win'), 'Windows-specific test')
@requests_timeout
class TestS3270WindowId(cti):

    # s3270 window ID test
    def test_s3270_window_id(self):

        # Start s3270.
        sport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(sport)

        # Verify the window ID is unknown.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(WindowId)')
        self.assertTrue(r.ok)
        window_id = r.json()['result'][0]
        self.assertIn(window_id, ['0xffffffff', '0xffffffffffffffff'])

        # Verify the same comes back from Set().
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId)')
        self.assertTrue(r.ok)
        window_id_set = r.json()['result'][0]
        self.assertEqual(window_id_set, window_id)

        # Set the window ID.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId,0x1234)')
        self.assertTrue(r.ok)

        # Get it back.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(WindowId)')
        self.assertTrue(r.ok)
        window_id = r.json()['result'][0]
        self.assertIn(window_id, ['0x00001234', '0x0000000000001234'])

        # Set it to nothing.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId,)')
        self.assertTrue(r.ok)

        # Verify the window ID is now unknown.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(WindowId)')
        self.assertTrue(r.ok)
        window_id = r.json()['result'][0]
        self.assertIn(window_id, ['0xffffffff', '0xffffffffffffffff'])

        # Verify that we can set the maximum value.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId,{window_id})')
        self.assertTrue(r.ok)

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 window ID Set() failure test
    def test_s3270_window_id_set_fail(self):

        # Start s3270.
        sport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(sport)

        # Verify the window ID is unknown.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(WindowId)')
        self.assertTrue(r.ok)
        window_id = r.json()['result'][0]
        self.assertIn(window_id, ['0xffffffff', '0xffffffffffffffff'])

        for bad in ['fred', '0x12345678123456789', '27x']:

            # Set the window ID.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId,"{bad}")')
            self.assertFalse(r.ok)

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 window ID canonicalization test
    def test_s3270_window_id_canonicalization(self):

        # Start s3270.
        sport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-set', 'windowId=0x1234']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(sport)

        # Verify the window ID has been canonoicalized.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(WindowId)')
        self.assertTrue(r.ok)
        window_id = r.json()['result'][0]
        self.assertIn(window_id, ['0x00001234', '0x0000000000001234'])

        # Verify the same comes back from Set().
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(WindowId)')
        self.assertTrue(r.ok)
        window_id_set = r.json()['result'][0]
        self.assertEqual(window_id_set, window_id)

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
