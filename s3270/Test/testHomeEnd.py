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
# s3270 tests for NVT Home/FieldEnd/PageUp/PageDown actions.

from subprocess import Popen
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270HomeEnd(cti):

    # s3270 NVT Home/End/PageUp/PageDown test.
    def s3270_nvt_home_end(self, action: str, expect: str, extra_send = ''):

        # Start a thread to read s3270's output.
        nc = copyserver()

        hport, hts = unused_port()
        hts.close()

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f"a:c:t:{nc.qloopback}:{nc.port}"]))
        self.children.append(s3270)
        self.check_listen(hport)

        # Send the extra escape sequence.
        if extra_send != '':
            nc.send(extra_send)
            # Wait until s3270 receives data on the connection.
            self.try_until(lambda: (self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(statsrx)').json()['result'][0]).split(' ')[1] != '0', 2, 'waiting for data')

        # Feed s3270 the action, plus an Enter() to flush the data out and a Disconnect() to flush the socket.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/{action}() Enter() Disconnect()')

        # Make sure they are passed through.
        out = nc.data()
        self.assertEqual((expect + '\r\n').encode(), out)

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_nvt_home(self):
        self.s3270_nvt_home_end('Home', '\x1b[H' )
    def test_s3270_nvt_home_app(self):
        self.s3270_nvt_home_end('Home', '\x1b[OH', '\x1b[?1h')
    def test_s3270_nvt_end(self):
        self.s3270_nvt_home_end('End', '\x1b[F' )
    def test_s3270_nvt_end_app(self):
        self.s3270_nvt_home_end('End', '\x1b[OF', '\x1b[?1h' )
    def test_s3270_nvt_page_up(self):
        self.s3270_nvt_home_end('PageUp', '\x1b[5~' )
    def test_s3270_nvt_page_down(self):
        self.s3270_nvt_home_end('PageDown', '\x1b[6~' )

if __name__ == '__main__':
    unittest.main()
