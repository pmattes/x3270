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
# x3270 retry tests

import os
import shutil
from subprocess import Popen, PIPE, DEVNULL
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipUnless(shutil.which('xdotool') != None, 'Need xdotool')
@requests_timeout
class TestX3270Retry(cti):

    # Wait for the Connection Error pop-up to appear.
    def find_popup(self) -> str:
        xdotool = Popen(['xdotool', 'search', '--onlyvisible', '--name', 'x3270 Error'], stdout=PIPE, stderr=DEVNULL)
        out = xdotool.communicate()[0].decode('utf8').strip().split()
        xdotool.wait(2)
        return out

    # x3270 retry cancel test
    def test_x3270_retry_cancel(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, pts = unused_port()

        # Start x3270.
        hport, hts = unused_port()
        hts.close()
        x3270 = Popen(vgwrap(['x3270', '-set', 'retry', '-httpd', str(hport), f'127.0.0.1:{playback_port}']))
        self.children.append(x3270)

        # Wait for the Connection Error pop-up to appear.
        self.try_until(lambda: self.find_popup() != [], 4, 'Connect error pop-up did not appear')

        # Make it stop.
        ids = self.find_popup()
        id = ids[0] if len(ids) == 1 else ids[1]
        os.system(f'xdotool windowfocus --sync {id} mousemove --window {id} 42 83 click 1')

        # Verify that x3270 is no longer reconnecting.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/printtext string oia').json()['result']
        self.assertEqual('X Not Connected', r[-1][7:22])

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/quit')
        self.vgwait(x3270)
        pts.close()

    # x3270 retry succeed test
    def test_x3270_retry_succeed_5s(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, pts = unused_port()

        # Start x3270.
        hport, hts = unused_port()
        hts.close()
        x3270 = Popen(vgwrap(['x3270', '-set', 'retry', '-httpd', str(hport), f'127.0.0.1:{playback_port}']))
        self.children.append(x3270)

        # Wait for the Connection Error pop-up to appear.
        self.try_until(lambda: self.find_popup != [], 4, 'Connect error pop-up did not appear')

        # Start playback to accept the connection.
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            pts.close()

            # Wait for x3270 to connect.
            p.wait_accept(timeout=6)

            # Wait for the status to update.
            def connected():
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/query host').json()['result'][0]
                return r == f'host 127.0.0.1 {playback_port}'
            self.try_until(connected, 2, 'x3270 did not connect')

            # Make sure the pop-up has popped itself down.
            self.assertFalse(self.find_popup())

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/quit')
        self.vgwait(x3270)
        pts.close()

if __name__ == '__main__':
    unittest.main()
