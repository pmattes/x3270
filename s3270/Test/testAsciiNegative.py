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
# s3270 negative-coordinate screen scrape tests

import os
from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270AsciiNegative(cti):

    # s3270 Ascii() with negative coordinates success test
    def s3270_Ascii_negative_success(self, origin: int):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Feed s3270 some actions.
            p.send_records(4)
            suffix = '1' if origin == 1 else ''
            sub = 1 if origin == 0 else 0
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii{suffix}(-1,{2-sub},1,4)')
            self.assertEqual('===>', r.json()['result'][0])

            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii{suffix}({24-sub},-80,1,5)')
            self.assertEqual(' ===>', r.json()['result'][0])

            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_Ascii_negative_success(self):
        self.s3270_Ascii_negative_success(0)
    def test_s3270_Ascii1_negative_success(self):
        self.s3270_Ascii_negative_success(1)

    # s3270 Ascii() with negative coordinates failure test
    def s3270_Ascii_negative_failure(self, origin: int):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Feed s3270 some actions.
            p.send_records(4)
            suffix = '1' if origin == 1 else ''
            sub = 1 if origin == 0 else 0
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii{suffix}(-25,1,1,4)')
            self.assertFalse(r.ok)

            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii{suffix}(1,-81,1,4)')
            self.assertFalse(r.ok)

            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_Ascii_negative_failure(self):
        self.s3270_Ascii_negative_failure(0)
    def test_s3270_Ascii1_negative_failure(self):
        self.s3270_Ascii_negative_failure(1)

if __name__ == '__main__':
    unittest.main()
