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
# s3270 Windows print-to-directory tests

import os
import pathlib
import requests
from subprocess import Popen, PIPE, DEVNULL
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@unittest.skipIf(not sys.platform.startswith('win'), 'Windows-specific test')
class TestS3270WindowsPrintDir(cti):

    # s3270 Windows print-to-dir test
    def s3270_windows_print_dir(self, kind: str):

        # Set up a temporary directory.
        with tempfile.TemporaryDirectory() as tempdir:

            # Start 'playback' to talk to s3270.
            port, ts = unused_port()
            with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
                ts.close()

                # Start s3270.
                loopback = '127.0.0.1'
                s3270 = Popen(vgwrap(["s3270", "-xrm", "s3270.contentionResolution: false",
                    '-xrm', f's3270.printer.name: {tempdir}',
                    f'{loopback}:{port}']), stdin=PIPE, stdout=DEVNULL)
                self.children.append(s3270)

                # Paint the screen.
                p.send_records(4)

                # Tell s3270 to do a screen trace, twice, on the printer.
                if kind == "screentrace":
                    s3270.stdin.write(b'ScreenTrace(on,printer)\n')
                    s3270.stdin.write(b'ScreenTrace(off)\n')
                    s3270.stdin.write(b'ScreenTrace(on,printer)\n')
                    s3270.stdin.write(b'ScreenTrace(off)\n')
                else:
                    s3270.stdin.write(b'PrintText()\n')
                    s3270.stdin.write(b'PrintText()\n')
                s3270.stdin.flush()

                # Wait for the files.
                self.try_until((lambda: (len(os.listdir(tempdir)) == 2)), 2,
                    's3270 did not produce output file')

                # Grab the first file, and remove the first line.
                outfile = os.path.join(tempdir, os.listdir(tempdir)[0])
                file1 = pathlib.Path(outfile).read_text()
                if kind == "screentrace":
                    file1 = '\n'.join(file1.split('\n')[1:])
                else:
                    file1 = '\n' + file1

            # Wait for the processes to exit.
            s3270.stdin.close()
            self.vgwait(s3270)

            # Compare contents.
            ref = pathlib.Path('s3270/Test/wdstrace.txt').read_text()
            self.assertEqual(ref, file1)

    def test_s3270_windows_screentrace_print_dir(self):
        self.s3270_windows_print_dir("screentrace")
    def test_s3270_windows_printtext_print_dir(self):
        self.s3270_windows_print_dir("printtext")

if __name__ == '__main__':
    unittest.main()
