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
# x3270 mixed-case model and oversize tests

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
class TestX3270MixedModel(cti):

    # x3270 mixed-case model test
    def test_x3270_mixed_case_model(self):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Set up the fonts.
            obj = os.path.abspath(os.path.split(shutil.which('x3270'))[0])
            self.assertEqual(0, os.system(f'mkfontdir {obj}'))
            self.assertEqual(0, os.system(f'DISPLAY=:2 xset +fp {obj}/'))
            self.assertEqual(0, os.system('DISPLAY=:2 xset fp rehash'))

            # Start x3270 with a mixed-case IBM- model on the command line.
            port, ts = unused_port()
            x3270 = Popen(vgwrap(['x3270', '-display', ':2', '-httpd', f'{port}', '-model', 'iBm-3279-2']))
            self.children.append(x3270)
            self.check_listen(port)
            ts.close()

            # Verify the model is right.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            result = r.json()['result']
            self.assertEqual(1, len(result))
            self.assertEqual('3279-2-E', result[0])

            # Try a model name with a different-cased IBM- at the front and a lowercase -E.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model,IbM-3278-3-e) Set(model)')
            s = r.json()
            self.assertTrue(r.ok)
            result = r.json()['result']
            self.assertEqual(1, len(result))
            self.assertEqual('3278-3-E', result[0])

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

    # x3270 mixed-case overize test
    def test_x3270_mixed_case_oversize(self):

        # Start a tightvnc server.
        with tvs.tightvncserver(self):

            # Set up the fonts.
            obj = os.path.abspath(os.path.split(shutil.which('x3270'))[0])
            self.assertEqual(0, os.system(f'mkfontdir {obj}'))
            self.assertEqual(0, os.system(f'DISPLAY=:2 xset +fp {obj}/'))
            self.assertEqual(0, os.system('DISPLAY=:2 xset fp rehash'))

            # Start x3270 with an uppercase and leading-zero oversize on the command line.
            port, ts = unused_port()
            x3270 = Popen(vgwrap(['x3270', '-display', ':2', '-httpd', f'{port}', '-oversize', '0100X100']))
            self.children.append(x3270)
            self.check_listen(port)
            ts.close()

            # Verify oversize is right.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(oversize)')
            self.assertTrue(r.ok)
            result = r.json()['result']
            self.assertEqual(1, len(result))
            self.assertEqual('100x100', result[0])

            # Try an uppercase-X oversize with a Set().
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(oversize,99X99) Set(oversize)')
            s = r.json()
            self.assertTrue(r.ok)
            result = r.json()['result']
            self.assertEqual(1, len(result))
            self.assertEqual('99x99', result[0])

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

if __name__ == '__main__':
    unittest.main()
