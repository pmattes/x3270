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

if __name__ == '__main__':
    unittest.main()
