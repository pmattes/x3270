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
# pr3287 -4/-6 tests

import os
from subprocess import Popen
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipIf(sys.platform.startswith('win'), 'Does not run on Windows')
@unittest.skipIf(sys.platform == 'cygwin', 'This does some very strange things on Cygwin')
@unittest.skipUnless(hostsSetup, setupHosts.warning)
class TestPr3287_46(cti):

    # pr3287 -4/-6 test
    def pr3287_46(self, ipv6=False):

        # Start 'playback' to feed data to pr3287.
        port, ts = unused_port()
        with playback(self, 'pr3287/Test/smoke.trc', port=port, ipv6=ipv6) as p:
            ts.close()

            # Start pr3287.
            (po_handle, po_name) = tempfile.mkstemp()
            (sy_handle, sy_name) = tempfile.mkstemp()
            pr3287 = Popen(vgwrap(["pr3287",
                '-6' if ipv6 else '-4',
                "-command", f"cat >'{po_name}'; date >'{sy_name}'",
                f'{setupHosts.test_hostname}:{port}']))
            self.children.append(pr3287)

            # Play the trace to pr3287.
            p.send_to_mark(1, send_tm=False)

            # Wait for the sync file to appear.
            self.try_until((lambda: (os.lseek(sy_handle, 0, os.SEEK_END) > 0)), 2, "pr3287 did not produce output")
            os.close(sy_handle)
            os.unlink(sy_name)

        # Wait for the processes to exit.
        pr3287.kill()
        self.children.remove(pr3287)
        self.vgwait(pr3287, assertOnFailure=False)

        # Read back the file.
        os.lseek(po_handle, 0, os.SEEK_SET)
        new_printout = os.read(po_handle, 65536)
        os.close(po_handle)
        os.unlink(po_name)

        # Compare.
        with open('pr3287/Test/smoke.out', 'rb') as file:
            ref_printout = file.read()

        self.assertEqual(new_printout, ref_printout)

    def test_pr3287_4(self):
        self.pr3287_46()
    def test_pr3287_6(self):
        self.pr3287_46(ipv6=True)

if __name__ == '__main__':
    unittest.main()
