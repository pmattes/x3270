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
# x3270 Query() tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestX3270Query(cti):

    # x3270 Query() test for window-specific items.
    def test_x3270_query_window(self):
        # Start x3270.
        http_port, ts = unused_port()
        x3270 = Popen(vgwrap(['x3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(x3270)
        ts.close()
        self.check_listen(http_port)

        q_window = ['CharacterPixels', 'DisplayPixels', 'WindowLocation', 'WindowPixels', 'WindowState', 'WindowId']

        # Query everything and make sure it includes all the window items.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query()')
        result = r.json()['result']
        for query in q_window:
            matches = [qline for qline in result if query in qline]
            self.assertEqual(1, len(matches), f'expected {query}')
            s = matches[0].split()
            match (query):
                case 'CharacterPixels':
                    self.assertNotEqual([0, 0], [s[0], s[4]])
                case 'DisplayPixels':
                    self.assertNotEqual([0, 0], [s[0], s[4]])
                case 'WindowPixels':
                    self.assertNotEqual([0, 0], [s[0], s[4]])
                case 'WindowState':
                    self.assertEqual('normal',s[1])
                    pass
                case 'WidowLocation':
                    pass
                case 'WindowId':
                    self.assertNotEqual(0, int(s[1], base=16))
                    pass


        # Stop x3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(x3270)

    # x3270 Query(CommandLine) test.
    def test_x3270_query_command_line(self):
        # Start x3270.
        http_port, ts = unused_port()
        x3270 = Popen(vgwrap(['x3270', '-httpd', f'127.0.0.1:{http_port}', '-user', 'x \x01y\nz']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(x3270)
        ts.close()
        self.check_listen(http_port)

        # Query the command line.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(CommandLine)')
        self.assertTrue(r.ok)
        result = r.json()['result'][0]
        ew = f' -httpd 127.0.0.1:{http_port} -user "x ^Ay^Jz"'
        self.assertTrue(result.endswith(ew))

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(x3270)

if __name__ == '__main__':
    unittest.main()
