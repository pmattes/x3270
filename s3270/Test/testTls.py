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
import requests
import sys
import os
import TestCommon

@unittest.skipIf(sys.platform.startswith("win"),
        "Windows does not have openssl")
class TestS3270Tls(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

        if sys.platform == 'darwin':
            # Add the fake root cert.
            sec = Popen(["security", "dump-trust", "-d"], stdout=PIPE,
                    stderr=DEVNULL)
            sec_out = sec.communicate()[0].decode('utf8').split('\n')
            if not any('fakeca' in line for line in sec_out):
                # Add the fake CA root cert
                print()
                print("***** Adding fake CA to trusted root certs for TLS tests")
                os.system('sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain s3270/Test/tls/myCA.pem')
                print("***** To remove the cert:")
                print("*****  security remove-trusted-cert -d s3270/Test/tls/myCA.pem")


    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # s3270 TLS smoke test
    def test_s3270_tls_smoke(self):

        # Start 'openssl s_server' to read s3270's output.
        port, ts = TestCommon.unused_port()
        if sys.platform == 'darwin':
            # MacOS openssl does not allow port re-use.
            ts.close()
        server = Popen(["openssl", "s_server", "-cert",
            "s3270/Test/tls/TEST.crt", "-key", "s3270/Test/tls/TEST.key",
            "-port", str(port), "-quiet"], stdout=PIPE)
        self.children.append(server)
        TestCommon.check_listen(port)
        if sys.platform != 'darwin':
            ts.close()

        # Start s3270.
        args = ["s3270"]
        if sys.platform != 'darwin':
            args += [ "-cafile", "s3270/Test/tls/myCA.pem" ]
        args.append(f"l:a:c:t:127.0.0.1:{port}=TEST")
        s3270 = Popen(args, stdin=PIPE, stdout=DEVNULL)
        self.children.append(s3270)

        # Feed s3270 some actions.
        s3270.stdin.write(b"String(abc)\n")
        s3270.stdin.write(b"Enter()\n")
        s3270.stdin.write(b"Disconnect()\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Make sure they are passed through.
        out = server.stdout.read(5)
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        server.stdout.close()
        server.kill()
        server.wait(timeout=2)
        s3270.stdin.close()
        s3270.wait(timeout=2)

if __name__ == '__main__':
    unittest.main()
