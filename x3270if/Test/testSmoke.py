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
# x3270if smoke tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *

class TestX3270ifSmoke(cti):

    # x3270if smoke test
    def test_x3270if_smoke(self):

        # Start a copy of s3270 to talk to.
        port, ts = unused_port()
        s3270 = Popen(["s3270", "-scriptport", f"127.0.0.1:{port}"],
                stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Run x3270if with a trivial query.
        x3270if = Popen(vgwrap(["x3270if", "-t", str(port), "Set(startTls)"]), stdout=PIPE)
        self.children.append(x3270if)

        # Decode the result.
        stdout = x3270if.communicate()[0].decode('utf8')

        # Wait for the processes to exit.
        s3270.kill()
        self.children.remove(s3270)
        s3270.wait()
        self.vgwait(x3270if)

        # Test the output.
        self.assertEqual('true\n', stdout)

if __name__ == '__main__':
    unittest.main()
