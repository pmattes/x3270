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
# s3270 TLS tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import sys
import os
import Common.Test.setupCert as setupCert
import Common.Test.cti as cti
import Common.Test.tls_server as tls_server

@unittest.skipUnless(setupCert.present(), setupCert.warning)
class TestS3270Tls(cti.cti):

    # s3270 TLS smoke test
    def test_s3270_tls_smoke(self):

        # Start a server to read s3270's output.
        port, ts = cti.unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'l:a:c:t:127.0.0.1:{port}=TEST')
            s3270 = Popen(cti.vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Do the TLS thing.
            server.wrap()

            # Feed s3270 some actions.
            s3270.stdin.write(b"String(abc)\n")
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.write(b"Disconnect()\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure they are passed through.
            out = server.recv_to_end()
            self.assertEqual(b"abc\r\n", out)

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 STARTTLS test
    def test_s3270_starttls(self):

        # Start a server to read s3270's output.
        port, ts = cti.unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ibmlink.trc', port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270', '-xrm', 's3270.contentionResolution: false']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'127.0.0.1:{port}=TEST')
            s3270 = Popen(cti.vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Make sure it all works.
            server.starttls()
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()
            server.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 file transfer crash validation
    @unittest.skipUnless(sys.platform.startswith('win'), 'Windows-specific test')
    def test_s3270_ft_crash(self):

        # Start a server to read s3270's output.
        port, ts = cti.unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ft-crash.trc', port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270', f'l:y:127.0.0.1:{port}=TEST']
            s3270 = Popen(cti.vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Do the TLS thing.
            server.wrap()

            # Feed s3270 some actions.
            s3270.stdin.write(b'Transfer(direction=send,localfile=s3270/Test/short.bin,hostfile=test,mode=binary)\n')
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.write(b"Disconnect()\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure the right thing happens.
            server.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
