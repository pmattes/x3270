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
# s3270 smoke tests

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Smoke(cti):

    # s3270 NVT smoke test
    def s3270_nvt_smoke(self, ipv6=False):

        # Start a thread to read s3270's output.
        nc = copyserver(ipv6=ipv6)

        # Start s3270.
        s3270 = Popen(vgwrap(["s3270", f"a:c:t:{nc.qloopback}:{nc.port}"]), stdin=PIPE,
                stdout=DEVNULL)
        self.children.append(s3270)

        # Feed s3270 some actions.
        s3270.stdin.write(b"String(abc)\n")
        s3270.stdin.write(b"Enter()\n")
        s3270.stdin.write(b"Disconnect()\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.data()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 NVT smoke test
    def test_s3270_nvt_smoke(self):
        self.s3270_nvt_smoke()
    def test_s3270_nvt_smoke_ipv6(self):
        self.s3270_nvt_smoke(ipv6=True)

    # s3270 3270 smoke test
    def s3270_3270_smoke(self, ipv6=False, uri=False):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port(ipv6=ipv6)
        with playback(self, 's3270/Test/ibmlink-cr.trc', port=port, ipv6=ipv6) as p:
            ts.close()

            # Start s3270.
            loopback = '[::1]' if ipv6 else '127.0.0.1'
            host = f'{loopback}:{port}' if not uri else f'tn3270://{loopback}:{port}'
            s3270 = Popen(vgwrap(["s3270", host]), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure the emulator does what we expect.
            p.match()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 3270 smoke test
    def test_s3270_3270_smoke(self):
        self.s3270_3270_smoke()
    def test_s3270_3270_smoke_ipv6(self):
        self.s3270_3270_smoke(ipv6=True)
    def test_s3270_3270_uri_smoke(self):
        self.s3270_3270_smoke(uri=True)
    def test_s3270_3270_uri_smoke_ipv6(self):
        self.s3270_3270_smoke(ipv6=True, uri=True)

    # s3270 httpd smoke test
    def s3270_httpd_smoke(self, ipv6=False):

        # Start s3270.
        port, ts = unused_port(ipv6=ipv6)
        loopback = '[::1]' if ipv6 else '127.0.0.1'
        s3270 = Popen(vgwrap(["s3270", "-httpd", f'{loopback}:{port}']))
        self.children.append(s3270)
        self.check_listen(port, ipv6=ipv6)
        ts.close()

        # Send it a JSON GET.
        r = self.get(f'http://{loopback}:{port}/3270/rest/json/Set(monoCase)')
        s = r.json()
        self.assertEqual(s['result'], ['false'])
        self.assertTrue(s['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

        # Send it a JSON POST.
        r = self.post(f'http://{loopback}:{port}/3270/rest/post',
                json={'action': 'set', 'args': ['monoCase']})
        s = r.json()
        self.assertEqual(s['result'], ['false'])
        self.assertTrue(s['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

        # Wait for the process to exit.
        self.get(f'http://{loopback}:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 httpd smoke test
    def test_s3270_httpd_smoke(self):
        self.s3270_httpd_smoke()
    def test_s3270_httpd_smoke_ipv6(self):
        self.s3270_httpd_smoke(ipv6=True)

    # s3270 stdin smoke test
    def test_s3270_stdin(self):

        # Start s3270.
        s3270 = Popen(vgwrap(["s3270"]), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a trivial command at it.
        s3270.stdin.write(b'Set(startTls)\n')

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8').split(os.linesep)

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.assertEqual(4, len(stdout))
        self.assertEqual('data: true', stdout[0])
        self.assertTrue(stdout[1].startswith('L U U N N 4 24 80 0 0 0x0 '))
        self.assertEqual('ok', stdout[2])
        self.assertEqual('', stdout[3])

if __name__ == '__main__':
    unittest.main()
