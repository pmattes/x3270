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
# s3270 String() tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270String(cti):

    # s3270 numeric field test.
    def test_s3270_numeric_field(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/numeric.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-set', 'numericLock',
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(1)

            # Try each of the printable characters.
            # Legal characters are 0..9, plus, minus, period, comma (in EBCDIC).
            legals = { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x4e, 0x60, 0x4b, 0x6b }
            for i in range(0x40, 0x100):
                r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Reset() Home() HexString(0x{i:02x})')
                if i in legals:
                    self.assertTrue(r.ok, f'0x{i:02x} should have succeeded')
                else:
                    self.assertFalse(r.ok, f'0x{i:02x} should have failed')
                    j = r.json()['result']
                    self.assertIn('Keyboard locked', j)
                    self.assertIn('Operator error', j)

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 numeric field test, no locking.
    def test_s3270_numeric_field_no_lock(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/numeric.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(1)

            # Try some non-numeric text.
            text = 'hello'
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String({text})')
            self.assertTrue(r.ok, 'string should have succeeded')
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(2,1,80)')
            j = r.json()['result'][0].strip()
            self.assertEqual(text, j, 'Should have gotten the same string back')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
