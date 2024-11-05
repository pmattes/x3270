#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# s3270 PrintText() tests

import filecmp
import os
import requests
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import unittest
import Common.Test.playback as playback
import Common.Test.cti as cti

class TestS3270PrintText(cti.cti):

    # s3270 PrintText(html) test
    def s3270_PrintText(self, type: str):

        # Start 'playback' to emulate the host.
        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/login.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Generate an image file.
            image_file = tempfile.NamedTemporaryFile(suffix='.' + type)
            tparam = (type + ',') if type != 'txt' else ''
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText({tparam}file,{image_file.name})')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertTrue(filecmp.cmp(image_file.name, f's3270/Test/login.{type}'), f'Expected correct {type} output')

        # Wait for s3270 to exit.
        r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_html(self):
        self.s3270_PrintText('html')
    def test_s3270_rtf(self):
        self.s3270_PrintText('rtf')
    def test_s3270_txt(self):
        self.s3270_PrintText('txt')

if __name__ == '__main__':
    unittest.main()
