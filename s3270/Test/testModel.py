#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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
# s3270 Set(model) tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Model(cti):

    # s3270 model/oversize interaction tests.
    def test_s3270_set_model_and_oversize(self):

        # Start s3270.
        sport, socket = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(sport)
        socket.close()

        # Set oversize. Make sure the model switches to 2.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(oversize,80x25)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
        self.assertTrue(r.ok)
        self.assertEqual('3279-2', r.json()['result'][0])

        # Try to set the model to 5. It should fail silently.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model,5)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
        self.assertTrue(r.ok)
        self.assertEqual('3279-2', r.json()['result'][0])

        # Now clear oversize and set the model again. It should succeed.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(oversize,"",model,5)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
        self.assertTrue(r.ok)
        self.assertEqual('3279-5', r.json()['result'][0])

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
