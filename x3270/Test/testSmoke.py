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
# x3270 smoke tests

import os
from subprocess import Popen, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import x3270.Test.vfb as vfb

@unittest.skipUnless(vfb.server_test(), vfb.no_server)
@unittest.skipIf(os.system('import -h 2>/dev/null') != 256, 'ImageMagick needed for tests')
@requests_timeout
class TestX3270Smoke(cti):

    # x3270 smoke test
    def test_x3270_smoke(self):

        # Start a virtual frame buffer.
        with vfb.Xvfb(self, need_fonts=True) as v:

            # Start 'playback' to feed x3270.
            playback_port, ts = unused_port()
            with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
                ts.close()

                # Start x3270.
                x3270_port, ts = unused_port()
                x3270 = Popen(vgwrap(['x3270',
                    '-httpd', f'127.0.0.1:{x3270_port}', '-utenv', f'127.0.0.1:{playback_port}']), stdout=DEVNULL, env=v.Env)
                self.children.append(x3270)
                self.check_listen(x3270_port)
                ts.close()

                # Feed x3270 some data.
                p.send_records(4)

                # Find x3270's window ID.
                r = self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/query')
                wid = r.json()['status'].split()[-2]

                # Dump the window contents.
                if 'GENERATE' in os.environ:
                    name = os.environ['GENERATE']
                else:
                    (handle, name) = tempfile.mkstemp(suffix=".bmp")
                    os.close(handle)
                self.assertEqual(0, os.system(f'import -display {v.Display} -window {wid} "{name}"'))

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{x3270_port}/3270/rest/json/Quit()')
            self.vgwait(x3270)

        # Make sure the image is correct.
        rv1 = os.system(f'cmp -s {name} x3270/Test/ibmlink.bmp')
        rv2 = os.system(f'cmp -s {name} x3270/Test/ibmlink2.bmp')
        self.assertTrue(rv1 == 0 or rv2 == 0, 'Screen image did not match reference image')
        if not 'GENERATE' in os.environ:
            os.unlink(name)

if __name__ == '__main__':
    unittest.main()
