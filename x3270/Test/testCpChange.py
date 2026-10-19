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
# x3270 code page change test

import os
from subprocess import Popen, DEVNULL
import tempfile
import time
import unittest

from Common.Test.cti import *
import x3270.Test.vfb as vfb

@unittest.skipUnless(vfb.server_test(), vfb.no_server)
@unittest.skipIf(os.system('import -h 2>/dev/null') != 256, 'ImageMagick needed for tests')
@requests_timeout
class TestX3270CpChange(cti):

    # x3270 code page change test
    def test_x3270_codepage_change(self):

        # Start a virtual frame buffer.
        with vfb.Xvfb(self, need_fonts=True) as v:

            # Start x3270.
            x3270_port, ts = unused_port()
            x3270 = Popen(vgwrap(['x3270', '-httpd', f'127.0.0.1:{x3270_port}', '-utenv',
                                  '-efont', '3270-12']), stdout=DEVNULL, env=v.Env)
            self.children.append(x3270)
            self.check_listen(x3270_port)
            ts.close()

            # Get x3270's window ID.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/query')
            wid = r.json()['status'].split()[-2]

            # Dump the window contents.
            (handle, name1) = tempfile.mkstemp(suffix='.bmp')
            os.close(handle)
            self.assertEqual(0, os.system(f'import -display {v.Display} -window {wid} "{name1}"'))

            # Change the code page.
            r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Set(codepage,cp275)')
            time.sleep(0.5)

            # Dump the window contents again.
            (handle, name2) = tempfile.mkstemp(suffix='.bmp')
            os.close(handle)
            self.assertEqual(0, os.system(f'import -display {v.Display} -window {wid} "{name2}"'))

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

        # Make sure the images match.
        self.assertEqual(0, os.system(f'cmp {name1} {name2}'))
        os.unlink(name1)
        os.unlink(name2)

if __name__ == '__main__':
    unittest.main()
