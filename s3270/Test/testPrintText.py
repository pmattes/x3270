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
# s3270 PrintText() tests

import glob
import os
from subprocess import Popen, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

# Line-ending-independent file comparison.
def cmp_lines(file1, file2):
    l1 = l2 = True
    with open(file1, 'r') as f1, open(file2, 'r') as f2:
        while l1 and l2:
            l1 = f1.readline()
            l2 = f2.readline()
            if l1 != l2:
                return False
    return True

@requests_timeout
class TestS3270PrintText(cti):

    # s3270 PrintText(html) test
    def s3270_PrintText(self, type: str):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/login.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Generate an image file.
            image_file = tempfile.NamedTemporaryFile(suffix='.' + type, delete=False)
            if_name = image_file.name
            tparam = (type + ',') if type != 'txt' else ''
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText({tparam}file,{if_name})')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertTrue(cmp_lines(image_file.name, f's3270/Test/login.{type}'), f'Expected correct {type} output')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        image_file.close()
        os.unlink(if_name)

    def test_s3270_html(self):
        self.s3270_PrintText('html')
    def test_s3270_rtf(self):
        self.s3270_PrintText('rtf')
    def test_s3270_txt(self):
        self.s3270_PrintText('txt')

    # s3270 PrintText(html) test
    def test_s3270_prtodir(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/login.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Dump the screen with prtodir.
            with tempfile.TemporaryDirectory() as td:
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText("prtodir {td}")')
                self.assertTrue(r.ok)

                # Wait for the file to be created.
                wild = os.path.join(td, '*')
                self.try_until(lambda: glob.glob(wild) != [], 5, 'PrintText file not created')

                # Wait for the file to have nonzero size.
                self.try_until(lambda: os.stat(glob.glob(wild)[0]).st_size > 0, 5, 'PrintText file is empty')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
