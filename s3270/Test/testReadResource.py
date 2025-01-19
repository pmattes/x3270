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
# s3270 resource file read test

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270ReadResource(cti):

    # s3270 read resource test
    def test_s3270_read_resource(self):

        # Start s3270.
        hport, socket = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), 's3270/Test/bsResource.s3270']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        socket.close()
        self.check_listen(hport)

        # Check the terminal name. It should have a trailing backslash, which was impossible
        # until the parser was fixed.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(TerminalName)')
        self.assertEqual('foo.bar\\', r.json()['result'][-1], 'Expected TerminalName')

        # Check the model, which would have been missed with the broken parser.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Model)')
        self.assertEqual('IBM-3279-2-E', r.json()['result'][-1], 'Expected Model')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
