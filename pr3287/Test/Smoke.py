#!/usr/bin/env python3
#
# Copyright (c) 2021 Paul Mattes.
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
# pr3287 smoke tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import os
import TestCommon

class TestPr3287Smoke(unittest.TestCase):

    # pr3287 smoke test
    def test_pr3287_smoke(self):

        # Start 'playback' to feed data to pr3287.
        playback = Popen(["playback", "-w", "-p", "9998",
            "pr3287/Test/smoke.trc"], stdin=PIPE, stdout=DEVNULL)
        TestCommon.check_listen(9998)

        # Start pr3287.
        (po_handle, po_name) = tempfile.mkstemp()
        (sy_handle, sy_name) = tempfile.mkstemp()
        pr3287 = Popen(["pr3287", "-command",
            f"cat >'{po_name}'; date >'{sy_name}'", "127.0.0.1:9998"])

        # Play the trace to pr3287.
        playback.stdin.write(b'm\n')
        playback.stdin.flush()

        # Wait for the sync file to appear.
        TestCommon.try_until((lambda: (os.lseek(sy_handle, 0, os.SEEK_END) > 0)), 2, "pr3287 did not produce output")
        os.close(sy_handle)
        os.unlink(sy_name)

        # Wait for the processes to exit.
        pr3287.kill()
        pr3287.wait(timeout=2)
        playback.stdin.close()
        playback.wait(timeout=2)

        # Read back the file.
        os.lseek(po_handle, 0, os.SEEK_SET)
        new_printout = os.read(po_handle, 65536)
        os.close(po_handle)
        os.unlink(po_name)

        # Compare.
        with open('pr3287/Test/smoke.out', 'rb') as file:
            ref_printout = file.read()

        self.assertEqual(new_printout, ref_printout)

if __name__ == '__main__':
    unittest.main()
