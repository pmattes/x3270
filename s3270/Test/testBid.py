#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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
# draft-04 BID tests

import requests
from subprocess import Popen, PIPE, DEVNULL
import unittest
import Common.Test.playback as playback
import Common.Test.cti as cti

class TestS3270Bid(cti.cti):

    # s3270 BID test
    def test_s3270_bid(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/bid.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(cti.vgwrap(["s3270", f"127.0.0.1:{port}"]), stdin=PIPE,
                    stdout=DEVNULL)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 no-BID test
    def test_s3270_no_bid(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/no_bid.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(cti.vgwrap(["s3270", "-xrm", "s3270.contentionResolution: false",
                f"127.0.0.1:{port}"]), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 contention resolution test
    def test_s3270_cr(self):

        # Start 'playback' to read s3270's output.
        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/contention-resolution.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(hport), f"127.0.0.1:{pport}"]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Send initial negotiations and half a screen.
            p.send_records(6)

            # Make sure the keyboard remains locked.
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertIn('X Wait', r.json()['result'][-1], 'Expected Wait')

            # Send 3270 data with SEND-DATA.
            p.send_records(1)

            # Make sure the keyboard is unlocked now.
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertEqual('     ', r.json()['result'][-1][11:16], 'Expected no lock')

        # Wait for the processes to exit.
        r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 BID typeahead test
    def test_s3270_bid_ta(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/bid-ta.trc', port=port) as p:
            socket.close()

            # Start s3270.
            hport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(["s3270", '-model', '5', '-oversize', '132x40',
                    '-httpd', f'127.0.0.1:{hport}', f"127.0.0.1:{port}"]),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Send data to get s3270 into a BID lock.
            p.send_records(3)

            # Type ahead an 'a' and Enter().
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Key(a)')
            self.assertTrue(r.ok)
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Enter()')
            self.assertTrue(r.ok)

            # Unblock the BID.
            p.send_records(1, send_tm = False)

            # Get the queued response.
            data = p.nread(8 + 14, 0.5)
        
        self.assertEqual(data, b'\x02\x00\x00\x00/\x00\xff\xef\x00\x00\x00\x00\x00}\x01\xa1\x11\x01\xa0\x81\xff\xef', 'Expected Enter')

        # Wait for the processes to exit.
        r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
