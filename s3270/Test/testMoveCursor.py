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
# s3270 MoveCursor tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270MoveCursor(cti):

    # s3270 MoveCursor NVT mode test
    def test_s3270_MoveCursor_nvt(self):

        # Start a thread to read s3270's output.
        nc = copyserver()

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port), f'a:c:t:{nc.qloopback}:{nc.port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Feed s3270 some actions that will fail.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Wait(nvtMode)')
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/MoveCursor(1,1)')
        self.assertFalse(r.ok)
        self.assertIn('NVT mode', r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/MoveCursor1(1,1)')
        self.assertFalse(r.ok)
        self.assertIn('NVT mode', r.json()['result'][0])

        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Disconnect()')
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    # s3270 MoveCursor basic test
    def s3270_MoveCursor_basic(self, origin: int):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Feed s3270 some actions.
            p.send_records(4)
            suffix = '1' if origin == 1 else ''
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}({origin+10},{origin+20})')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            res = r.json()['result'][0].split()
            self.assertEqual('11', res[1])
            self.assertEqual('21', res[3])

            # Test a random offset.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}({(10*80)+20})')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            res = r.json()['result'][0].split()
            self.assertEqual('11', res[1])
            self.assertEqual('21', res[3])

            # Test the furthest offset.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}({(24*80)-1})')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            res = r.json()['result'][0].split()
            self.assertEqual('24', res[1])
            self.assertEqual('80', res[3])

            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_MoveCursor_basic(self):
        self.s3270_MoveCursor_basic(0)
    def test_s3270_MoveCursor1_basic(self):
        self.s3270_MoveCursor_basic(1)

    # s3270 MoveCursor negative test
    def s3270_MoveCursor_negative(self, origin: int):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Feed s3270 some actions.
            p.send_records(4)
            suffix = '1' if origin == 1 else ''
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(-10,-20)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            res = r.json()['result'][0].split()
            self.assertEqual('15', res[1])
            self.assertEqual('61', res[3])

            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_MoveCursor_negative(self):
        self.s3270_MoveCursor_negative(0)
    def test_s3270_MoveCursor1_negative(self):
        self.s3270_MoveCursor_negative(1)

    # s3270 MoveCursor boundary test
    def s3270_MoveCursor_boundary(self, origin: int):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port,) as p:
            ts.close()

            # Start s3270.
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Feed s3270 some actions.
            p.send_records(4)
            suffix = '1' if origin == 1 else ''
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}({24+origin},1)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(1,{80+origin})')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(-25,1)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(1,-81)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(-1)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}({origin+(24*80)})')
            self.assertFalse(r.ok)
            if suffix == 1:
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(0, 1)')
                self.assertFalse(r.ok)
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor{suffix}(1, 0)')
                self.assertFalse(r.ok)

            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_MoveCursor_boundary(self):
        self.s3270_MoveCursor_boundary(0)
    def test_s3270_MoveCursor1_boundary(self):
        self.s3270_MoveCursor_boundary(1)

if __name__ == '__main__':
    unittest.main()
