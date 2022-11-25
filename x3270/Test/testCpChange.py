#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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
# x3270 code page change test

import os
import requests
import shutil
import stat
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import time
import unittest

import Common.Test.cti as cti

@unittest.skipIf(os.system('xset q >/dev/null 2>&1') != 0, "X11 server needed for tests")
@unittest.skipIf(os.system('tightvncserver --help 2>/dev/null') != 65280, "tightvncserver needed for tests")
@unittest.skipIf(os.system('import -h 2>/dev/null') != 256, "ImageMagick needed for tests")
class TestX3270CpChange(cti.cti):

    # Set up procedure.
    def setUp(self):
        if 'DISPLAY' in os.environ:
            self.display = os.environ['DISPLAY']
        else:
            self.display = None
        cti.cti.setUp(self)

    # Tear-down procedure.
    def tearDown(self):
        # Restore DISPLAY so other tests aren't confused.
        if self.display != None:
            os.environ['DISPLAY'] = self.display
        # Tear down the VNC server, in case a test failed and did not
        # clean up.
        os.system('tightvncserver -kill :2 2>/dev/null')
        cti.cti.tearDown(self)

    # x3270 code page change test
    def test_x3270_codepage_change(self):

        # Start a tightvnc server.
        # The password file needs to be 0600 or Vnc will prompt for it again.
        os.chmod('x3270/Test/vnc/.vnc/passwd', stat.S_IREAD | stat.S_IWRITE)
        cwd=os.getcwd()
        os.environ['HOME'] = cwd + '/x3270/Test/vnc'
        os.environ['USER'] = 'foo'
        # Set SSH_CONNECTION to keep the VirtualBox extensions from starting in the tightvncserver.
        os.environ['SSH_CONNECTION'] = 'foo'
        self.assertEqual(0, os.system('tightvncserver :2 2>/dev/null'))
        self.check_listen(5902)

        os.environ['DISPLAY'] = ':2'
        obj = os.path.abspath(os.path.split(shutil.which('x3270'))[0])
        self.assertEqual(0, os.system(f'mkfontdir {obj}'))
        self.assertEqual(0, os.system(f'xset +fp {obj}/'))
        self.assertEqual(0, os.system('xset fp rehash'))

        # Start x3270.
        x3270_port, ts = cti.unused_port()
        x3270 = Popen(cti.vgwrap(["x3270",
            "-httpd", f"127.0.0.1:{x3270_port}",
            "-efont", "3270-12"]), stdout=DEVNULL)
        self.children.append(x3270)
        self.check_listen(x3270_port)
        ts.close()

        # Get x3270's window ID.
        r = requests.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/query')
        wid = r.json()['status'].split()[-2]

        # Dump the window contents.
        (handle, name1) = tempfile.mkstemp(suffix='.bmp')
        os.close(handle)
        self.assertEqual(0, os.system(f'import -display :2 -window {wid} "{name1}"'))

        # Change the code page.
        r = requests.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(codepage,cp275)')
        time.sleep(0.5)

        # Dump the window contents again.
        (handle, name2) = tempfile.mkstemp(suffix='.bmp')
        os.close(handle)
        self.assertEqual(0, os.system(f'import -display :2 -window {wid} "{name2}"'))

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
        self.vgwait(x3270)

        # Make sure the images match.
        self.assertEqual(0, os.system(f'cmp {name1} {name2}'))
        os.unlink(name1)
        os.unlink(name2)

if __name__ == '__main__':
    unittest.main()
