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
# wc3270 smoke tests

import filecmp
import os
import sys
import tempfile
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipUnless(sys.platform.startswith("win"), "Only works on native Windows")
@requests_timeout
class TestWc3270Smoke(cti):

    def find_in_path(self, exe):
        '''Find an executable in $PATH'''
        for dir in os.environ['PATH'].split(';'):
            cand = dir + '\\' + exe
            if os.path.exists(cand):
                return (dir, cand)
        self.assertTrue(False, f'Could not find {exe} in PATH')

    # wc3270 smoke test
    def test_wc3270_smoke(self):

        # Start 'playback' to feed wc3270.
        playback_port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Create a session file.
            wc3270_port, ts = unused_port()
            (handle, sname) = tempfile.mkstemp(suffix='.wc3270')
            os.write(handle, f'wc3270.title: wc3270\n'.encode('utf8'))
            os.write(handle, f'wc3270.httpd: 127.0.0.1:{wc3270_port}\n'.encode('utf8'))
            os.write(handle, f'wc3270.hostname: 127.0.0.1:{playback_port}\n'.encode('utf8'))
            os.close(handle)

            # Create a shortcut.
            (handle, lname) = tempfile.mkstemp(suffix='.lnk')
            os.close(handle)
            wc3270_dir, wc3270_path = self.find_in_path('wc3270.exe')
            cmd = f'mkshort {wc3270_dir} wc3270.exe {lname} {sname}'
            self.assertEqual(0, os.system(cmd))

            # Start wc3270 in its own window by starting the link.
            self.assertEqual(0, os.system(f'start {lname}'))
            self.check_listen(wc3270_port)
            ts.close()
            os.unlink(sname)
            os.unlink(lname)

            # Feed wc3270 some data.
            p.send_records(4)

            # Dump the window contents.
            time.sleep(0.5)
            (handle, name) = tempfile.mkstemp(suffix='.bmp')
            os.close(handle)
            self.get(f'http://127.0.0.1:{wc3270_port}/3270/rest/json/SnapScreen({name})')

        # Make sure the image is correct.
        self.assertTrue(filecmp.cmp(name, 'wc3270/Test/ibmlink.bmp') or filecmp.cmp(name, 'wc3270/Test/ibmlink-notfront.bmp'),
            f'{name} does not match wc3270/Test/ibmlink.bmp or wc3270/Test/ibmlink-notfront.tmp')
        os.unlink(name)

if __name__ == '__main__':
    unittest.main()
