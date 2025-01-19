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
# s3270 WONT test

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Wont(cti):

    # s3270 WONT test
    def test_s3270_sscp_lu(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/wont-tn3270e.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-set', 'wrongTerminalName', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Send negotiations and make sure the responses match, but don't disconnect.
            p.match(disconnect=False)

            # Make sure the emulator is in 3270 mode now.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertEqual('4A ', r.json()['result'][-1][0:3], 'Expected TN3270 mode')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
