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
# s3270 Query() tests

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Query(cti):

    # s3270 Query(keyboardlock) test
    def test_s3270_query_keyboard(self, ipv6=False):

        # Start 'playback' to read s3270's output.
        playback_port, ts = unused_port(ipv6=ipv6)
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Start s3270.
            http_port, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}',
                f'127.0.0.1:{playback_port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)
            ts.close()

            # Feed x3270 some data.
            p.send_records(4)

            # Force the keyboard to lock.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Up()')
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Key(a)')

            # Verify that it is locked.
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(KeyboardLock)')
            kb = r.json()['result'][0]
            self.assertEqual('true', kb, 'keyboard should be locked')

            # Unlock it and verify that it is unlocked.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Reset())')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(KeyboardLock)')
            kb = r.json()['result'][0]
            self.assertEqual('false', kb, 'keyboard should not be locked')

            # Stop s3270.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
