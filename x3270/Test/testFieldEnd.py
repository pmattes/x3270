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
# x3270 FieldEnd tests

import os
import shutil
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipUnless(shutil.which('xdotool') != None, 'Need xdotool')
@requests_timeout
class TestX3270FieldEnd(cti):

    # Wait for a window to appear.
    def find_window(self, title: str):
        xdotool = Popen(['xdotool', 'search', '--onlyvisible', '--name', title], stdout=PIPE, stderr=DEVNULL)
        out = xdotool.communicate()[0].decode('utf8').strip()
        xdotool.wait(2)
        return out != ''
    
    # x3270 FieldEnd test.
    def test_x3270_FieldEnd(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, pts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', playback_port) as p:

            # Start x3270.
            hport, hts = unused_port()
            hts.close()
            (handle, tf) = tempfile.mkstemp()
            os.close(handle)
            x3270 = Popen(vgwrap(['x3270', '-title', 'Under Test', '-httpd', str(hport),
                f'127.0.0.1:{playback_port}']))
            self.children.append(x3270)
            self.check_listen(hport)
            self.try_until(lambda: self.find_window('Under Test'), 4, 'x3270 did not appear')

            # Find x3270's window ID.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(WindowId)')
            self.assertTrue(r.ok, 'Expected Window ID query to succeed')
            window_id = r.json()['result'][0]

            # Draw the screen.
            p.send_records(4)

            # Enter some data, then move the cursor back to the beginning of the field.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String("Hello threre")')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Home()')
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(cursor1)')
            self.assertTrue(r.ok, 'Expected cursor query to succeed')
            cursor = (r.json()['result'][0]).split(' ')[3]
            self.assertEqual('13', cursor)

            # Send End.
            # XXX: The Timing Mark helps, but does not ensure that the key was processed.
            os.system(f'xdotool windowfocus --sync {window_id} key --window {window_id} End')
            p.send_tm()

            # Verify the cursor moved to the end of the field.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(cursor1)')
            self.assertTrue(r.ok, 'Expected cursor query to succeed')
            cursor = (r.json()['result'][0]).split(' ')[3]
            self.assertEqual('20', cursor)

            # Clean up.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/quit')
            self.vgwait(x3270)
            pts.close()
            os.unlink(tf)

if __name__ == '__main__':
    unittest.main()
