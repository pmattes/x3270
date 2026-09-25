#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
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
# x3270 XTWINOPS stack tests

import os
from subprocess import Popen, DEVNULL, check_output
import unittest

from Common.Test.cti import *
import x3270.Test.tvs as tvs

@unittest.skipIf(tvs.tightvncserver_test() == False, "tightvncserver needed for tests")
@requests_timeout
class TestX3270Stack(cti):

    # Read the window title.
    def window_title(self, window_id: str) -> str:
        name = check_output(['xprop', '-display', ':2', '-id', window_id, '_NET_WM_NAME'])
        return name.decode().split(' = ', 1)[1].strip().strip('"')

    # Wait for the window title to change.
    def check_window_title(self, window_id: str, title: str):
        self.try_until(lambda: self.window_title(window_id) == title, 2, f'Window title did not become "{title}"')

    # XTWINOPS push/pop stack tests.
    def test_xtwinops_stack(self):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Start a server to throw NVT escape sequences at x3270.
            s = copyserver()

            # Start x3270.
            hport, ts = unused_port()
            env = os.environ.copy()
            env['DISPLAY'] = ':2'
            x3270 = Popen(vgwrap(['x3270', '-set', 'x3270.allowWindowOps',
                '-set', 'noTelnetInputMode=character', '-httpd', f'127.0.0.1:{hport}',
                f'a:c:t:127.0.0.1:{s.port}']), stdout=DEVNULL, env=env)
            self.children.append(x3270)
            self.check_listen(hport)
            ts.close()

            # Find x3270's window ID.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(WindowId)')
            self.assertTrue(r.ok)
            window_id = r.json()['result'][0]

            # Set the initial window title.
            s.send('\033]2;base\007')
            self.check_window_title(window_id, 'base')

            # Push the title, change it and pop back to the saved value.
            s.send('\033[22;2t')
            s.send('\033]2;pushed\007')
            self.check_window_title(window_id, 'pushed')
            s.send('\033[23;2t')
            self.check_window_title(window_id, 'base')

            # An indexed push and pop must both be ignored.
            s.send('\033[22;2;1t')
            s.send('\033]2;after-invalid-push\007')
            self.check_window_title(window_id, 'after-invalid-push')
            s.send('\033[23;2t')
            self.check_window_title(window_id, 'after-invalid-push')

            s.send('\033[22;2t')
            s.send('\033]2;before-invalid-pop\007')
            s.send('\033[23;2;1t')
            self.check_window_title(window_id, 'before-invalid-pop')
            s.send('\033[23;2t')
            self.check_window_title(window_id, 'after-invalid-push')

            # Eleven pushes should retain the most recent ten saved titles.
            s.send('\033]2;item0\007')
            for i in range(11):
                s.send('\033[22;2t')
                s.send(f'\033]2;item{i + 1}\007')
            for i in range(10, 0, -1):
                s.send('\033[23;2t')
                self.check_window_title(window_id, f'item{i}')

            # There should be no older title left on the stack.
            s.send('\033[23;2t')
            self.check_window_title(window_id, 'item1')

            # Clean up.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
            self.vgwait(x3270)
            s.data()

if __name__ == '__main__':
    unittest.main()
