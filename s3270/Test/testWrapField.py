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
# s3270 field wrap tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

def kw_split(result):
    ret = {}
    for line in result:
        kv = line.split(': ')
        ret[kv[0]] = kv[1]
    return ret

def ebcdic(s: str) -> bytes:
    '''Convert ASCII to EBCDIC'''
    dd = Popen(['dd', 'conv=ebcdic'], stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
    return dd.communicate(s.encode())[0]

@requests_timeout
class TestS3270WrapField(cti):

    def get_cursor(self, port: int):
        rs = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(cursor1)').json()['result'][0].split()
        return (int(rs[1]), int(rs[3]))

    # s3270 field wrap test
    def test_s3270_wrap_field(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/wrap_field.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            # The screen will be painted with a protected field on row 1, whose SF is on the bottom right corner of
            # the screen (43,80), and an unprotected field covering the rest of the screen, whose SF is at the
            # right edge of row 1 (1,80).
            p.send_records(1)

            # Verify that the cursor lands at row 2, column 1.
            self.assertEqual((2, 1), self.get_cursor(sport))

            # Jump to the end of the screen and wrap input back to the top of the screen.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/MoveCursor1(43,79)')
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(xx)')
            self.assertTrue(r.ok)
            self.assertEqual((2, 2), self.get_cursor(sport))

            # Move the cursor up into the read-only field and try typing. It should fail.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Up()')
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(xx)')
            self.assertFalse(r.ok)
            rs = r.json()['result']
            self.assertIn('Operator error', rs)
            self.assertIn('Keyboard locked', rs)

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 ReadBuffer(field) test
    def test_s3270_readbuffer_field(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/wrap_field.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            # The screen will be painted with a protected field on row 1, whose SF is on the bottom right corner of
            # the screen (43,80), and an unprotected field covering the rest of the screen, whose SF is at the
            # right edge of row 1 (1,80).
            p.send_records(1)

            # Check the field we land on.
            rs = kw_split(self.get(f'http://127.0.0.1:{sport}/3270/rest/json/ReadBuffer(field)').json()['result'])
            self.assertEqual('1 80', rs['Start1'])
            self.assertEqual('79', rs['StartOffset'])
            self.assertEqual('2 1', rs['Cursor1'])
            self.assertEqual('80', rs['CursorOffset'])
            c = rs['Contents'].split()
            self.assertEqual('SF(c0=c4)', c[0])
            self.assertEqual(['00' for i in range(0, 3359)], c[1:])

            # Check the one above it.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Up()')
            rs = kw_split(self.get(f'http://127.0.0.1:{sport}/3270/rest/json/ReadBuffer(field)').json()['result'])
            self.assertEqual('43 80', rs['Start1'])
            self.assertEqual('3439', rs['StartOffset'])
            self.assertEqual('1 1', rs['Cursor1'])
            self.assertEqual('0', rs['CursorOffset'])

            # This is the text in the first field. Pad it with NUL bytes on the right and expand it to hex to compare.
            s = 'This is the read-only field at the top of the screen.'
            b = bytes.hex(s.ljust(79, '\0').encode())
            c = rs['Contents']
            self.assertEqual(c.replace(' ', ''), 'SF(c0=f0)' + b)

            # Try again with EBCDIC.
            rs = kw_split(self.get(f'http://127.0.0.1:{sport}/3270/rest/json/ReadBuffer(field,ebcdic)').json()['result'])
            b = bytes.hex(ebcdic(s.ljust(79, '\0')))
            c = rs['Contents']
            self.assertEqual(c.replace(' ', ''), 'SF(c0=f0)' + b)

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
