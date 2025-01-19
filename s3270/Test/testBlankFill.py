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
# s3270 Blank Fill mode tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270BlankFill(cti):

    # s3270 blank fill test.
    # Verifying a bug fix for Blank Fill mode with underscores embedded in a field.
    def test_s3270_blank_fill_eat(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/sruvm.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(2)

            # Jump to the COMMAND field.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Tab()')
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Tab()')

            # Fill in a value that has some underscores at the end.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("abc___")')

            # Set insert mode, go back to the beginning of the field, and type a character.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(insertMode,true)')
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/BackTab()')
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(x)')

            # What we should get back in the field is "xabc___".
            # With the bug, we got back "xabc__" (last underscore consumed instead of the NULs).
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/AsciiField()')
            self.assertTrue(r.ok)
            result = r.json()['result'][0]
            self.assertEqual('xabc___', result.strip(), 'Expected underscores to be intact')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 underscore blank fill test.
    def s3270_underscore_blank_fill(self, on: bool):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/sruvm.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-set', 'underscoreBlankFill=' + str(on).lower(),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(2)

            # Fill in a value that has some underscores at the end.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("abcdef__")')

            # Set insert mode, go back to the beginning of the field, and type a character.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(insertMode,true)')
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/BackTab()')
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(x)')

            if on:
                # The action should succeed, and what we should get back in the field is "xabcdef_".
                self.assertTrue(r.ok)
                r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/AsciiField()')
                self.assertTrue(r.ok)
                result = r.json()['result'][0]
                self.assertEqual('xabcdef_', result.strip(), 'Expected trailing underscore to be consumed')
            else:
                # The action should fail.
                self.assertFalse(r.ok, 'Expected failed request')
                self.assertEqual('Keyboard locked', r.json()['result'][0], 'Expected keyboard lock error')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_underscore_blank_fill_on(self):
        self.s3270_underscore_blank_fill(True)
    def test_s3270_underscore_blank_fill_off(self):
        self.s3270_underscore_blank_fill(False)

if __name__ == '__main__':
    unittest.main()
