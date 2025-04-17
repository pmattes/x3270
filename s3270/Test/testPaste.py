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
# s3270 paste test

from subprocess import Popen, DEVNULL
import sys
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Paste(cti):

    # s3270 paste test
    def test_s3270_paste(self):

        port, ts = unused_port()
        null = 'NUL:' if sys.platform.startswith('win') else '/dev/null'
        with playback(self, null, port=port,) as p:
            ts.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-nvt', f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Wait for the connection to complete.
            self.try_until(lambda: 'connected' in self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(ConnectionState)').json()['result'][0], 2, "didn't connect")

            # Pump in a string that almost wraps.
            a79 = ''.join(['A' for i in range(79)])
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String({a79})')

            # Paste in a string that wraps.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PasteString(4242)')

            # Get the cursor location.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            col = r.json()['result'][0].split()[3]

            # Make sure it is 2 (not 80).
            rx = r.json()
            self.assertEqual(2, int(col), f'result is {rx}')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 paste -nowrap test
    def test_s3270_paste_nowrap(self):

        port, ts = unused_port()
        with playback(self, 's3270/Test/target.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-nvt', f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(2)

            # Pump in a string that would normally wrap.
            s = ''.join(['41' for i in range(81)])
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PasteString(-nomargin,0x{s})')

            # Get the cursor location.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            rx = r.json()['result'][0].split()
            row = rx[1]
            col = rx[3]

            # Make sure it is row 24, column 6.
            self.assertEqual(24, int(row), f'result is {rx}')
            self.assertEqual(6, int(col), f'result is {rx}')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
