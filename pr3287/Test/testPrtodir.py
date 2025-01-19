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
# prtodir tests

import os
import pathlib
from subprocess import Popen
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(sys.platform.startswith('win'), 'Does not run on Windows')
class TestPrtodir(cti):

    # prtodir command line test
    def test_prtodir_cmdline(self):
        self.assertNotEqual(0, os.system('prtodir 2>/dev/null'))
        self.assertNotEqual(0, os.system('prtodir a b 2>/dev/null'))

    # prtodir basic test
    def test_prtodir_basic(self):

        with tempfile.TemporaryDirectory() as tempdir:
            # Create 10 files.
            for i in range(10):
                self.assertEqual(0, os.system(f'echo hello | prtodir {tempdir}'))

            # Make sure there are 10 of them.
            files = os.listdir(tempdir)
            self.assertEqual(10, len(files))

            # Make sure they contain what we expect.
            for file in files:
                contents = pathlib.Path(os.path.join(tempdir, file)).read_text()
                self.assertEqual('hello\n', contents)
    
    # test for files being (relatively) complete
    def file_check(self, tempdir: str):
        files = os.listdir(tempdir)
        if len(files) != 2:
            return False
        for file in files:
            if os.path.getsize(os.path.join(tempdir, file)) < 1024:
                return False
        return True

    # pr3287 plus prtodir
    def test_prtodir_pr3287(self):

        # Grab the expected output.
        ref_printout = pathlib.Path('pr3287/Test/smoke.out').read_text()

        # Set up a temporary directory.
        with tempfile.TemporaryDirectory() as tempdir:
            # Start 'playback' to feed data to pr3287.
            port, ts = unused_port()
            with playback(self, 'pr3287/Test/smoke.trc', port=port) as p:
                ts.close()

                # Start pr3287.
                pr3287 = Popen(vgwrap(['pr3287', '-command', f'prtodir {tempdir}',
                    f'127.0.0.1:{port}']))
                self.children.append(pr3287)

                # Play the trace to pr3287.
                p.send_to_mark(1, send_tm=False)

                # Wait for the output files to appear.
                self.try_until((lambda: (self.file_check(tempdir))), 2,
                    'pr3287+prtodir did not produce output files')

            # Wait for the processes to exit.
            pr3287.kill()
            self.children.remove(pr3287)
            self.vgwait(pr3287, assertOnFailure=False)

            # Read back the files.
            n = [pathlib.Path(os.path.join(tempdir, file)).read_text() for file in os.listdir(tempdir)]

        # Compare.
        # The reference file is actually the second of the two print-outs, but we don't know the order
        # that the file names will be enumerated, so we have to compare against each of them.
        self.assertTrue(n[0] == ref_printout or n[1] == ref_printout)
                
if __name__ == '__main__':
    unittest.main()
