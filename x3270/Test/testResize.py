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
# x3270 resize tests

import os
import shutil
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
import Common.Test.playback as playback
import x3270.Test.tvs as tvs

@unittest.skipIf(os.system('xset q >/dev/null 2>&1') != 0, "X11 server needed for tests")
@unittest.skipIf(tvs.tightvncserver_test() == False, "tightvncserver needed for tests")
@requests_timeout
class TestX3270Resize(cti):

    # x3270 resize test
    def test_x3270_resize(self):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Set up the fonts.
            obj = os.path.abspath(os.path.split(shutil.which('x3270'))[0])
            self.assertEqual(0, os.system(f'mkfontdir {obj}'))
            self.assertEqual(0, os.system(f'DISPLAY=:2 xset +fp {obj}/'))
            self.assertEqual(0, os.system('DISPLAY=:2 xset fp rehash'))

            # Start x3270.
            x3270_port, ts = unused_port()
            env = os.environ.copy()
            env['DISPLAY'] = ':2'
            x3270 = Popen(vgwrap(['x3270',
                '-xrm', f'x3270.connectFileName: {os.getcwd()}/x3270/Test/vnc/.x3270connect',
                '-httpd', f'127.0.0.1:{x3270_port}']), stdout=DEVNULL, env=env)
            self.children.append(x3270)
            self.check_listen(x3270_port)
            ts.close()

            # Resize it.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(model,2,oversize,100x100,extendedDataStream,false)')
            self.assertTrue(r.ok)

            # Make sure oversize failed, because we also turned off extendedDataStream.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(oversize)')
            self.assertTrue(r.ok)
            self.assertEqual('', r.json()['result'][0])

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

    # x3270 color-mode change test
    def test_x3270_color_mode_change(self):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Set up the fonts.
            obj = os.path.abspath(os.path.split(shutil.which('x3270'))[0])
            self.assertEqual(0, os.system(f'mkfontdir {obj}'))
            self.assertEqual(0, os.system(f'DISPLAY=:2 xset +fp {obj}/'))
            self.assertEqual(0, os.system('DISPLAY=:2 xset fp rehash'))

            # Start x3270 in monochrome mode.
            x3270_port, ts = unused_port()
            env = os.environ.copy()
            env['DISPLAY'] = ':2'
            x3270 = Popen(vgwrap(['x3270', '-mono',
                '-xrm', f'x3270.connectFileName: {os.getcwd()}/x3270/Test/vnc/.x3270connect',
                '-httpd', f'127.0.0.1:{x3270_port}']), stdout=DEVNULL, env=env)
            self.children.append(x3270)
            self.check_listen(x3270_port)
            ts.close()

            # Resize it.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(model,3279-2)')
            self.assertTrue(r.ok)

            # Make sure the model stayed as a 3278, since we're in -mono mode.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual('3278-2-E', r.json()['result'][0])

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

if __name__ == '__main__':
    unittest.main()
