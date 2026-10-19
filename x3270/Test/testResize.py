#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
import x3270.Test.vfb as vfb

@unittest.skipUnless(vfb.server_test(), vfb.no_server)
@requests_timeout
class TestX3270Resize(cti):

    # x3270 resize test
    def test_x3270_resize(self):

        # Start a virtual frame buffer.
        with vfb.Xvfb(self, need_fonts=True) as v:

            # Start x3270.
            x3270_port, ts = unused_port()
            x3270 = Popen(vgwrap(['x3270', '-httpd', f'127.0.0.1:{x3270_port}', '-utenv']), stdout=DEVNULL, env=v.Env)
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

        # Start a virtual frame buffer.
        with vfb.Xvfb(self, need_fonts=True) as v:

            # Start x3270 in monochrome mode.
            x3270_port, ts = unused_port()
            x3270 = Popen(vgwrap(['x3270', '-mono', '-httpd', f'127.0.0.1:{x3270_port}', '-utenv']), stdout=DEVNULL, env=v.Env)
            self.children.append(x3270)
            self.check_listen(x3270_port)
            ts.close()

            # Resize it.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(model,3279-2)')
            self.assertTrue(r.ok)

            # Make sure the model stayed as a 3278, since we're in -mono mode.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual('3278-2', r.json()['result'][0])

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

if __name__ == '__main__':
    unittest.main()
