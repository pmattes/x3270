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
# x3270 _NET_WM_NAME tests

import os
import requests
import shutil
import stat
from subprocess import Popen, PIPE, DEVNULL, check_output
import tempfile
import unittest

import Common.Test.playback as playback
import Common.Test.cti as cti

@unittest.skipIf(os.system('xset q >/dev/null 2>&1') != 0, "X11 server needed for tests")
@unittest.skipIf(os.system('tightvncserver --help 2>/dev/null') != 65280, "tightvncserver needed for tests")
class TestX3270WmName(cti.cti):

    # Set up procedure.
    def setUp(self):
        cti.cti.setUp(self)

    # Tear-down procedure.
    def tearDown(self):
        # Tear down the VNC server, in case a test failed and did not
        # clean up.
        cwd=os.getcwd()
        os.system(f'HOME={cwd}/x3270/Test/vnc tightvncserver -kill :2 2>/dev/null')
        cti.cti.tearDown(self)

    # x3270 title test.
    def title_test(self, cmdlineTitle: bool = False):

        # Start a tightvnc server.
        # The password file needs to be 0600 or Vnc will prompt for it again.
        os.chmod('x3270/Test/vnc/.vnc/passwd', stat.S_IREAD | stat.S_IWRITE)
        cwd=os.getcwd()
        # Set SSH_CONNECTION to keep the VirtualBox extensions from starting in the tightvncserver.
        self.assertEqual(0, os.system(f'HOME={cwd}/x3270/Test/vnc USER=foo SSH_CONNECTION=foo tightvncserver :2 2>/dev/null'))
        self.check_listen(5902)
        self.try_until(lambda: os.system('xlsfonts -display :2 -ll -fn "-*-helvetica-bold-r-normal--14-*-100-100-p-*-iso8859-1" >/dev/null 2>&1'), 2, 'Xvncserver is not sane')

        # Start 'playback' to feed x3270.
        playback_port, ts = cti.unused_port()
        with playback.playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            self.check_listen(playback_port)
            ts.close()

            # Start x3270.
            x3270_port, ts = cti.unused_port()
            cmdline = ["x3270",
                "-xrm", f"x3270.connectFileName: {os.getcwd()}/x3270/Test/vnc/.x3270connect",
                "-httpd", f"127.0.0.1:{x3270_port}" ]
            if cmdlineTitle:
                cmdline += [ '-title', 'foo']
            cmdline.append(f'127.0.0.1:{playback_port}')
            env = os.environ.copy()
            env['DISPLAY'] = ':2'
            x3270 = Popen(cti.vgwrap(cmdline), stdout=DEVNULL, env=env)
            self.children.append(x3270)
            self.check_listen(x3270_port)
            ts.close()

            # Feed x3270 some data.
            p.send_records(4)

            # Find x3270's window ID using Query().
            r = requests.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Query(WindowId)')
            wid = r.json()['result'][0]

            # Check the _NET_WM_NAME property (the window title).
            name = check_output(['xprop', '-display', ':2', '-id', wid, '_NET_WM_NAME'])
            if cmdlineTitle:
                self.assertEqual(name.decode(), f'_NET_WM_NAME(UTF8_STRING) = "foo"\n')
            else:
                self.assertEqual(name.decode(), f'_NET_WM_NAME(UTF8_STRING) = "x3270-4 127.0.0.1:{playback_port}"\n')

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
        self.vgwait(x3270)
        self.assertEqual(0, os.system(f'HOME={cwd}/x3270/Test/vnc tightvncserver -kill :2 2>/dev/null'))

    # x3270 default title test.
    def test_default_title(self):
        self.title_test()

    # x3270 command-line title test.
    def test_explicit_title(self):
        self.title_test(cmdlineTitle=True)

if __name__ == '__main__':
    unittest.main()
