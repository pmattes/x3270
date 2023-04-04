#!/usr/bin/env python3
#
# Copyright (c) 2021-2023 Paul Mattes.
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
# Test for basic TN3270E NVT-DATA and SSCP-LU mode I/O.

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import Common.Test.playback as playback
import Common.Test.cti as cti

class TestMenu(cti.cti):

    def menu_test(self, file: str):

        # Start 'playback' to drive s3270.
        playback_port, ts = cti.unused_port()
        with playback.playback(self, f's3270/Test/{file}.trc', port=playback_port) as p:
            ts.close()

            # Start s3270 with a webserver.
            s3270_port, ts = cti.unused_port()
            s3270 = Popen(cti.vgwrap(["s3270", "-httpd", f"127.0.0.1:{s3270_port}", f"127.0.0.1:{playback_port}"]))
            self.children.append(s3270)
            self.check_listen(s3270_port)
            ts.close()

            # Step until the login screen is visible.
            p.match(disconnect=False, nrecords=2)
            p.send_tm()

            # Make sure the menu appears on the screen.
            r = requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Ascii1(1,1,5)')
            self.assertEqual(requests.codes.ok, r.status_code)
            self.assertEqual('x3270', r.json()['result'][0])

            # Send 'foo'.
            requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/String(foo)')
            requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Enter()')

            # Make sure it is echoed properly.
            r = requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Ascii1(13,5,3)')
            self.assertEqual(requests.codes.ok, r.status_code)
            self.assertEqual('foo', r.json()['result'][0])

            # Make sure it is sent to the host.
            p.match()

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_nvt_data(self):
        '''Test basic NVT-DATA I/O'''
        self.menu_test('nvt-data')

    def test_sscp_lu_data(self):
        '''Test basic SSCP-LU-DATA I/O'''
        self.menu_test('sscp-lu-data')

if __name__ == '__main__':
    unittest.main()
