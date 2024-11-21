#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# x3270 MoveCursor tests

import os
from subprocess import Popen, PIPE, DEVNULL
import requests
import shutil
import tempfile
import unittest

import Common.Test.playback as playback
import Common.Test.cti as cti

@unittest.skipUnless(shutil.which('xdotool') != None, 'Need xdotool')
class TestX3270MoveCursor(cti.cti):

    # Wait for a window to appear.
    def find_window(self, title: str):
        xdotool = Popen(['xdotool', 'search', '--onlyvisible', '--name', title], stdout=PIPE, stderr=DEVNULL)
        out = xdotool.communicate()[0].decode('utf8').strip()
        xdotool.wait(2)
        return out != ''
    
    # x3270 MoveCursor NVT-mode test.
    def x3270_MoveCursor_nvt(self, suffix: str):

        # Find an unused port, but do not listen on it yet.
        playback_port, pts = cti.unused_port()
        with playback.playback(self, 's3270/Test/ibmlink.trc', playback_port) as p:

            # Start x3270.
            hport, hts = cti.unused_port()
            hts.close()
            (handle, tf) = tempfile.mkstemp()
            os.close(handle)
            x3270 = Popen(cti.vgwrap(['x3270', '-title', 'Under Test', '-httpd', str(hport), '-trace', '-tracefile', tf, '-set', 'traceMonitor=false',
                '-keymap', 'foo', '-xrm', f'x3270.keymap.foo: #override <Btn1Down>: MoveCursor{suffix}()',
                f'a:c:t:127.0.0.1:{playback_port}']))
            self.children.append(x3270)
            self.check_listen(hport)
            self.try_until(lambda: self.find_window('Under Test'), 4, 'x3270 did not appear')

            # Find x3270's window ID.
            r = requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(WindowId)')
            self.assertTrue(r.ok, 'Expected Window ID query to succeed')
            window_id = r.json()['result'][0]

            # Try clicking, invalidly.
            os.system(f'xdotool windowfocus --sync {window_id} mousemove --window {window_id} 50 50 click 1')
            self.try_until(lambda: self.find_window('X3270 Error'), 4, 'Error pop-up did not appear')

            # Verify the error pop-up contents.
            requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/trace off')
            with open(tf, "r") as t:
                lines = t.readlines()
            self.assertTrue(any('is not valid in NVT mode' in line for line in lines))

            requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/quit')
            self.vgwait(x3270)
            pts.close()
            os.unlink(tf)

    def test_x3270_MoveCursor_nvt(self):
        self.x3270_MoveCursor_nvt('')
    def test_x3270_MoveCursor1_nvt(self):
        self.x3270_MoveCursor_nvt('1')

if __name__ == '__main__':
    unittest.main()
