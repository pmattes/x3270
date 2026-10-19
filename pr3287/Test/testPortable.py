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
# pr3287 portable mode tests

import glob
import os
import shutil
from subprocess import Popen, DEVNULL
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipUnless(sys.platform.startswith('win'), 'Windows-specific test')
@requests_timeout
class TestPr3287Portable(cti):

    # Start pr3287 with the supplied portable-mode options.
    def start_pr3287(self, executable: str, options: list[str], cwd: str) -> tuple[int, Popen]:
        pr3287 = Popen(vgwrap([executable, '-trace'] + options), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL, cwd=cwd)
        self.children.append(pr3287)
        return pr3287
    
    # Stop pr3287.
    def stop_pr3287(self, pr3287: Popen):
        self.vgwait(pr3287, expected_status=1)

    # Check the trace directory.
    def check_portable(self, pr3287: Popen, p: playback, expect_dir: str):

        # Reject the device type.
        p.send_lines(3, send_tm=False)

        # Wait for pr3287 to fail before checking anything.
        self.stop_pr3287(pr3287)

        # Make sure a trace file appeared where we expected.
        self.assertTrue(glob.glob(os.path.join(expect_dir, 'x3trc.*')), 'Trace file not found in expected directory')

    # Test the flag file and command-line portable mode controls.
    def pr3287_portable(self, cmdline: list[str], portable=False, create_flagfile=False):
        source = shutil.which('pr3287.exe')
        self.assertIsNotNone(source, 'Could not find pr3287.exe in PATH')

        with tempfile.TemporaryDirectory() as cwd:
            with tempfile.TemporaryDirectory() as installdir:
                shutil.copytree(os.path.dirname(source), installdir, dirs_exist_ok=True)
                executable = os.path.join(installdir, os.path.basename(source))
                flagfile = os.path.join(installdir, 'PORTABLE.txt')

                if create_flagfile:
                    with open(flagfile, 'w'):
                        pass

                playback_port, ts = unused_port()
                with playback(self, 'pr3287/Test/reject.trc', port=playback_port) as p:
                    ts.close()

                    pr3287 = self.start_pr3287(executable, cmdline + [f'127.0.0.1:{playback_port}'], cwd=cwd)
                    self.check_portable(pr3287, p, installdir if portable else cwd)

    def test_pr3287_portable_default(self):
        self.pr3287_portable([], portable=False)
    def test_pr3287_portable_flagfile(self):
        self.pr3287_portable([], portable=True, create_flagfile=True)
    def test_pr3287_portable_cmdline_disabled(self):
        self.pr3287_portable(['-noportable'], create_flagfile=True)
    def test_pr3287_portable_cmdline_enabled(self):
        self.pr3287_portable(['-portable'], portable=True)

if __name__ == '__main__':
    unittest.main()
