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
# s3270 NVT tests

import re
import unittest
from subprocess import Popen

from Common.Test.cti import *

@requests_timeout
class TestS3270Nvt(cti):

    # NVT 1049 mode test
    def nvt_1049(self, mode_alt, mode_normal):

        # Start a server to throw NVT escape sequences at s3270.
        s = sendserver(self)

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'a:c:t:127.0.0.1:{s.port}']))
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        # Send some text and read it back.
        s.send(b'hello')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(hello,1)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,1,1,80)')
        self.assertTrue(r.ok)
        self.assertEqual('hello', r.json()['result'][0].strip())

        # Switch to the alternate display.
        s.send(b'\x1b[?1049' + mode_alt + b'there')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(there,1)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,1,1,10)')
        self.assertTrue(r.ok)
        self.assertEqual('     there', r.json()['result'][0])

        # Switch back to the main display. Make sure it picks back up exactly as it was.
        s.send(b'\x1b[?1049' + mode_normal + b'fella')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(fella,1)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,1,1,10)')
        self.assertTrue(r.ok)
        self.assertEqual('hellofella', r.json()['result'][0])
        
        # Alternate again. Make sure it's blank.
        s.send(b'\x1b[?1049' + mode_alt + b'hubba')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(hubba,1)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,1,1,15)')
        self.assertTrue(r.ok)
        self.assertEqual('          hubba', r.json()['result'][0])

        # Clean up.
        s.close()
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
    # Test with set/reset.
    def test_nvt_1049_set(self):
        self.nvt_1049(b'h', b'l')
    # Test with save/restore
    def test_nvt_1049_save(self):
        self.nvt_1049(b'h', b'r')

    # ECH test.
    def test_ech(self):

        # Start a server to throw NVT escape sequences at s3270.
        s = sendserver(self)

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'a:c:t:127.0.0.1:{s.port}']))
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        # Send some text that fills two lines.
        ten = b'1234567890'
        for i in range(16):
            s.send(ten)

        # Move the cursor near the end of the first line, then erase 5 characters (ECH).
        s.send(b'\033[1;70H')
        s.send(b'\033[5X')
        # Make sure the cursor has not moved, and the first line is as we expect.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Cursor1)')
        self.assertTrue(r.ok)
        self.assertEqual('row 1 column 70 offset 69', r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,69,1,7)')
        self.assertTrue(r.ok)
        self.assertEqual('9     5', r.json()['result'][0])

        # Move the cursor past that area and try erasing past the end of the line.
        s.send(b'\033[1;79H')
        s.send(b'\033[5X')
        # Make sure the end of the line has been erased.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,78,1,3)')
        self.assertTrue(r.ok)
        self.assertEqual('8  ', r.json()['result'][0])

        # Make sure the next line has not been modified.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,1,1,10)')
        self.assertTrue(r.ok)
        self.assertEqual('1234567890', r.json()['result'][0])

        # Clean up.
        s.close()
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Secondary DA test.
    def test_secondary_da(self):

        # Start a server to throw NVT escape sequences at s3270.
        s = copyserver()

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport), f'a:c:t:127.0.0.1:{s.port}']))
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        # Send the secondary DA sequence.
        s.send('\033[>c')

        # End the session.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Compute the current version number, as reported by DA.
        with open('Common/version.txt') as f:
            version_lines = f.readlines()
        version = [line for line in version_lines if line.startswith('version=')][0].replace('version=', '').replace('"', '').strip()
        version = ''.join([f'{int(chunk):02d}' for chunk in re.sub('[a-z]+', '.', version).split('.')])

        # See what we get back.
        reply = s.data().decode().split(';')
        self.assertEqual('\033[>0', reply[0])
        self.assertEqual(version, reply[1])
        self.assertEqual('3270c', reply[2])

        # Clean up.
        self.vgwait(s3270)

    # Window report test.
    def window_report(self, send: str, receive: str):

        # Start a server to throw NVT escape sequences at s3270.
        s = copyserver()

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport), f'a:c:t:127.0.0.1:{s.port}']))
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        # Send what they want.
        if send != None:
            s.send(send)

        # Ask for rows and columns.
        s.send('\033[18t')

        # End the session.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # See what we get back.
        reply = s.data().decode()
        self.assertEqual(receive,reply)

        # Clean up.
        self.vgwait(s3270)

    # Basic window report test.
    def test_window_report(self):
        self.window_report(None, '\033[8;43;80t')
    # Window size change success test.
    def test_window_change_success(self):
        self.window_report('\033[8;45;81t', '\033[8;45;81t')
    # Window size change by lines test.
    def test_window_change_success_lines(self):
        self.window_report('\033[45t', '\033[8;45;80t')

    # Window change request that fails.
    def window_change_fail(self, rows: int, cols: int):
        self.window_report(f'\033[8;{rows};{cols}t', '\033[8;43;80t')
    
    def test_window_change_fail_small(self):
        self.window_change_fail(20, 20)
    def test_window_change_fail_large(self):
        self.window_change_fail(1000, 1000)
    def test_window_change_fail_zero_rows(self):
        self.window_change_fail(0, 80)
    def test_window_change_fail_zero_cols(self):
        self.window_change_fail(43, 0)

    # Window changes with missing (use existing value) parameters.
    def test_window_change_omit_rows(self):
        self.window_report('\033[8;;81t', '\033[8;43;81t')
    def test_window_change_omit_cols(self):
        self.window_report('\033[8;45t', '\033[8;45;80t')

if __name__ == '__main__':
    unittest.main()
