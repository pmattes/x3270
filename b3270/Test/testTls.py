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
# b3270 TLS tests

import json
from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
import Common.Test.setupCert as setupCert
import Common.Test.tls_server as tls_server

@unittest.skipUnless(setupCert.present(), setupCert.warning)
class TestB3270Tls(cti):

    # b3270 TLS test
    def test_b3270_tls(self):

        # Start a server to read b3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, port) as server:
            ts.close()

            # Start b3270.
            args = ['b3270', '-json']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            b3270 = Popen(vgwrap(args), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Connect b3270 to the server.
            b3270.stdin.write(f'"open(l:a:c:t:127.0.0.1:{port}=TEST)"\n'.encode('UTF8'))
            b3270.stdin.flush()

            # Do the TLS thing.
            server.wrap()

            # Check for a TLS status update.
            while True:
                out = pq.get(2, 'b3270 did not produce TLS status output')
                if b'"tls":' in out:
                    break

            verified = json.loads(out.decode('utf8'))['tls']['verified']
            self.assertTrue(verified)

        # Wait for the process to exit.
        b3270.stdin.close()
        b3270.stdout.close()
        self.vgwait(b3270)

if __name__ == '__main__':
    unittest.main()
