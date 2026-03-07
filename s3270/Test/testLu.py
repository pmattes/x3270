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
# s3270 LU tests

import unittest
from subprocess import Popen

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Lu(cti):

    # Test an invalid LU name on the command line.
    def test_s3270_bad_lu_cmdline(self):
        # Start s3270. It should fail.
        s3270 = Popen(vgwrap(['s3270', 'foo\x01bar@127.0.0.1:1234']), stderr=PIPE)
        self.children.append(s3270)

        # Make sure it complained appropriately.
        self.vgwait(s3270, assertOnFailure=False)
        out = s3270.stderr.readlines()
        self.assertEqual('Hostname syntax error: contains invalid characters', out[0].decode('utf8').strip())
        s3270.stderr.close()

    # Test an invalid LU name as a command.
    def test_s3270_bad_lu_command(self):
        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport)]))
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open(foo\x01bar@127.0.0.1:1234)')
        self.assertFalse(r.ok)
        self.assertEqual('Hostname syntax error: contains invalid characters', r.json()['result'][0])

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
