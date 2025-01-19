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
# x3270 _NET_WM_NAME tests

import os
from subprocess import Popen, DEVNULL, check_output
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import x3270.Test.tvs as tvs

@unittest.skipIf(tvs.tightvncserver_test() == False, "tightvncserver needed for tests")
class TestX3270WmName(cti):

    # x3270 title test.
    def title_test(self, cmdlineTitle: bool = False):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Start 'playback' to feed x3270.
            playback_port, ts = unused_port()
            with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
                ts.close()

                # Start x3270.
                x3270_port, ts = unused_port()
                cmdline = ["x3270",
                    "-xrm", f"x3270.connectFileName: {os.getcwd()}/x3270/Test/vnc/.x3270connect",
                    "-httpd", f"127.0.0.1:{x3270_port}" ]
                if cmdlineTitle:
                    cmdline += [ '-title', 'foo']
                cmdline.append(f'127.0.0.1:{playback_port}')
                env = os.environ.copy()
                env['DISPLAY'] = ':2'
                x3270 = Popen(vgwrap(cmdline), stdout=DEVNULL, env=env)
                self.children.append(x3270)
                self.check_listen(x3270_port)
                ts.close()

                # Feed x3270 some data.
                p.send_records(4)

                # Find x3270's window ID using Query().
                r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Query(WindowId)')
                wid = r.json()['result'][0]

                # Check the _NET_WM_NAME property (the window title).
                name = check_output(['xprop', '-display', ':2', '-id', wid, '_NET_WM_NAME'])
                if cmdlineTitle:
                    self.assertEqual(name.decode(), f'_NET_WM_NAME(UTF8_STRING) = "foo"\n')
                else:
                    self.assertEqual(name.decode(), f'_NET_WM_NAME(UTF8_STRING) = "x3270-4 127.0.0.1:{playback_port}"\n')

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

    # x3270 default title test.
    def test_default_title(self):
        self.title_test()

    # x3270 command-line title test.
    def test_explicit_title(self):
        self.title_test(cmdlineTitle=True)

if __name__ == '__main__':
    unittest.main()
