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
# s3270 HTTPS tests

from subprocess import Popen, PIPE, DEVNULL
import requests
import unittest

from Common.Test.cti import *

class TestS3270Httpd(cti):

    # s3270 HTTPD persist test.
    def test_s3270_httpd_persist(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Start a requests session and do a get.
        # Doing the get within a session keeps the connection alive.
        # Note: This is done by most tests now, without the explcit check for the pool.
        s = requests.Session()
        r = s.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)')
        self.assertEqual(requests.codes.ok, r.status_code)
        self.assertEqual('false', r.json()['result'][0])

        # Do it again.
        r = s.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)')
        self.assertEqual(requests.codes.ok, r.status_code)
        self.assertEqual('false', r.json()['result'][0])

        # Make sure the connection pool is non-empty.
        self.assertNotEqual(0, len(r.connection.poolmanager.pools.keys()))

        # Wait for the process to exit successfully.
        s.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        s.close()
        self.vgwait(s3270)

    # s3270 HTTPD stext error test.
    def s3270_httpd_stext_error_test(self, actions:str, content:str):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Make a bad request in stext mode.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/stext{actions}')
        self.assertFalse(r.ok)
        response = r.text.split('\n')
        self.assertEqual(3, len(response), 'Expected two lines of response')
        self.assertIn(' U ', response[0], 'Expected prompt in first line of output')
        self.assertIn(content, response[1], 'Expected error message in second line of response')

        # Wait for the process to exit successfully.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_httpd_stext_error(self):
        self.s3270_httpd_stext_error_test('/Sebfp(monoCase)', 'Sebfp')
    def test_s3270_httpd_stext_missing(self):
        self.s3270_httpd_stext_error_test('', 'Missing')
    def test_s3270_httpd_stext_missing2(self):
        self.s3270_httpd_stext_error_test('/', 'Missing')
    def test_s3270_httpd_stext_syntax(self):
        self.s3270_httpd_stext_error_test('/Foo(', 'Syntax')

    # s3270 HTTPD JSON error test.
    def s3270_httpd_json_error_test(self, actions:str, content: str):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Make a bad request in JSON mode.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json{actions}')
        self.assertFalse(r.ok)
        response = r.json()
        self.assertIn('result', response)
        self.assertIn('status', response)
        self.assertIn(content, response['result'][0])

        # Wait for the process to exit successfully.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_httpd_json_error(self):
        self.s3270_httpd_json_error_test('/Sebfp(monoCase)', 'Sebfp')
    def test_s3270_httpd_json_missing(self):
        self.s3270_httpd_json_error_test('', 'Missing')
    def test_s3270_httpd_json_missing2(self):
        self.s3270_httpd_json_error_test('/', 'Missing')
    def test_s3270_httpd_json_syntax(self):
        self.s3270_httpd_json_error_test('/Foo(', 'Syntax')

    # s3270 HTTPD HTML error test.
    def s3270_httpd_html_error_test(self, actions:str, content: str):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Make a bad request in HTML mode.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/html{actions}')
        self.assertFalse(r.ok)
        response = r.text
        self.assertIn('Status', response)
        self.assertIn(content, response)

        # Wait for the process to exit successfully.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_httpd_html_error(self):
        self.s3270_httpd_html_error_test('/Sebfp(monoCase)', 'Sebfp')
    def test_s3270_httpd_html_missing(self):
        self.s3270_httpd_html_error_test('', 'Missing')
    def test_s3270_httpd_html_missing2(self):
        self.s3270_httpd_html_error_test('/', 'Missing')
    def test_s3270_httpd_html_syntax(self):
        self.s3270_httpd_html_error_test('/Foo(', 'Syntax')

if __name__ == '__main__':
    unittest.main()
