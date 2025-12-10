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
# b3270 Query() tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *

@requests_timeout
class TestB3270Query(cti):

    # b3270 Query() test to make sure the window-specific Query() items are present.
    def test_b3270_query_window(self):
        # Start b3270.
        http_port, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(http_port)

        q_window = ['CharacterPixels', 'DisplayPixels', 'WindowLocation', 'WindowPixels', 'WindowState']

        # Query everything and make sure it includes the window items.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query()')
        result = r.json()['result']
        for query in q_window:
            matches = [query for qline in result if qline.startswith(query + ':')]
            self.assertEqual(1, len(matches), f'expected {matches}')

        # Stop b3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

    def try_query(self, hport: int, query: str, desired: str) -> bool:
        '''Try a Query() action and expect specific results'''
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query({query})')
        result = r.json()['result'][0]
        # print('try_query got', result)
        return result == desired

    # Test one window query.
    def b3270_query_window_test(self, stdin_send: str, query: str, desired: str):
        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport), '-json']), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        self.check_listen(hport)
        ts.close()

        # Send the command to stdin.
        b3270.stdin.write(stdin_send.encode() + b'\n')
        b3270.stdin.flush()

        # Query the corresponding item.
        self.try_until(lambda: self.try_query(hport, query, desired), 2, f'Query({query}) did not get desired result "{desired}"')

        # Stop b3270.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

    # Test each of the window queries.
    def test_b3270_query_specific(self):
        self.b3270_query_window_test('{"window-change":{"operation":"state","state":"maximized"}}', 'WindowState', 'maximized')
        self.b3270_query_window_test('{"window-change":{"operation":"state","state":"iconified"}}', 'WindowState', 'iconified')
        self.b3270_query_window_test('{"window-change":{"operation":"state","state":"full-screen"}}', 'WindowState', 'full-screen')
        self.b3270_query_window_test('{"window-change":{"operation":"move","x":100,"y":200}}', 'WindowLocation', 'x 100 y 200')
        self.b3270_query_window_test('{"window-change":{"operation":"size","type":"window","height":300,"width":400}}', 'WindowPixels', 'height 300 width 400')
        self.b3270_query_window_test('{"window-change":{"operation":"size","type":"screen","height":500,"width":600}}', 'DisplayPixels', 'height 500 width 600')
        self.b3270_query_window_test('{"window-change":{"operation":"size","type":"character","height":14,"width":7}}', 'CharacterPixels', 'height 14 width 7')

if __name__ == '__main__':
    unittest.main()
