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
# draft-04 BID tests

from subprocess import Popen, PIPE, DEVNULL
import threading
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Bid(cti):

    # s3270 BID test
    def test_s3270_bid(self):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, 's3270/Test/bid.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(["s3270", f"127.0.0.1:{port}"]), stdin=PIPE,
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
        port, socket = unused_port()
        with playback(self, 's3270/Test/no_bid.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(["s3270", "-xrm", "s3270.contentionResolution: false",
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
        pport, socket = unused_port()
        with playback(self, 's3270/Test/contention-resolution.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f"127.0.0.1:{pport}"]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Send initial negotiations and half a screen.
            p.send_records(6)

            # Make sure the keyboard remains locked.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertIn('X Wait', r.json()['result'][-1], 'Expected Wait')

            # Send 3270 data with SEND-DATA.
            p.send_records(1)

            # Make sure the keyboard is unlocked now.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertEqual('     ', r.json()['result'][-1][11:16], 'Expected no lock')

        # Wait for the processes to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 BID typeahead test
    def test_s3270_bid_ta(self):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, 's3270/Test/bid-ta.trc', port=port) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(["s3270", '-model', '5', '-oversize', '132x40',
                    '-httpd', f'127.0.0.1:{hport}', f"127.0.0.1:{port}"]),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Send data to get s3270 into a BID lock.
            p.send_records(3)

            # Type ahead an 'a' and Enter().
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Key(a)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Enter()')
            self.assertTrue(r.ok)

            # Unblock the BID.
            p.send_records(1, send_tm = False)

            # Get the queued response.
            data = p.nread(8 + 14, 0.5)
        
        self.assertEqual(data, b'\x02\x00\x00\x00/\x00\xff\xef\x00\x00\x00\x00\x00}\x01\xa1\x11\x01\xa0\x81\xff\xef', 'Expected Enter')

        # Wait for the processes to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Send a string and an Enter() with a timeout to s3270.
    def send_string(self, port):
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/String("LOGIN LJU")')
        try:
            self.get(f'http://127.0.0.1:{port}/3270/rest/json/Enter()', timeout=1)
        except:
            pass

    # Check for the Enter() action blocking.
    def check_block(self, sport: int) -> bool:
        '''Check for a blocking Enter() action'''
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(task)').json()['result']
        return any(['KBWAIT => Enter' in line for line in r])

    # s3270 BID lock test
    def test_s3270_bid_lock(self):

        # Start 'playback' to read s3270's output.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/bid-bug.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f"127.0.0.1:{pport}"]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Send the initial screen.
            p.send_records(2)

            # Send a string and press Enter, asynchronously.
            x = threading.Thread(target=self.send_string, args=[hport])
            x.start()

            # Wait for the Enter to block.
            self.try_until(lambda: (self.check_block(hport)), 2, 'Enter() not blocking')

            # Respond with a new screen, but not enough to clear the BID condition.
            p.send_records(5)
            x.join()

            # Make sure that Enter() is still blocked and the keyboard is still locked.
            self.assertTrue(self.check_block(hport), 'Expected Enter() to remain blocked')
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(KeyboardLock)')
            self.assertEqual('true', r.json()['result'][0], 'Expected locked keyboard')
            self.assertEqual('L', r.json()['status'].split()[0], 'Expected L in status')

        # Wait for the processes to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
