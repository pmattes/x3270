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
# s3270 SSCP-LU mode tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270SscpLu(cti):

    # s3270 SSCP-LU mode test
    def test_s3270_sscp_lu(self):

        # Start 'playback' to read s3270's output.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/sscp-lu.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f"127.0.0.1:{pport}"]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Send initial negotiations and the first SSCP-LU message.
            p.send_records(21)

            # Make sure the emulator has switched to SSCP-LU mode.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertEqual('4BS', r.json()['result'][-1][0:3], 'Expected SSCP-LU mode')

            # Send another SSCP-LU and then a regular 3270 record.
            p.send_records(2)

            # Make sure the emulator is back to 3270 mode.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertEqual('4B ', r.json()['result'][-1][0:3], 'Expected 3270 mode')

        # Wait for the processes to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
