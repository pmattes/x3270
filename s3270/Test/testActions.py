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
# s3270 actions tests

import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Actions(cti):

    # s3270 action suppression test
    def test_s3270_action_suppress(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f':{port}', '-set', 'suppressActions=a b() c set']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Send it a Show(), which should succeed.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Show()')
        self.assertTrue(r.ok)
        s = r.json()
        self.assertGreater(len(s['result']), 1)

        # Then a Set(), which should not.
        # I think the original intent was for the action to fail, but what actually happens is that it is
        # ignored silently.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set() Show(Formatted)')
        self.assertTrue(r.ok)
        s = r.json()
        self.assertEqual('unformatted', s['result'][0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
