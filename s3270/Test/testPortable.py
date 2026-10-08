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
# s3270 portable mode tests

import os
import shutil
from subprocess import Popen, DEVNULL
import sys
import tempfile
import unittest

from Common.Test.cti import *

@unittest.skipUnless(sys.platform.startswith('win'), 'Windows-specific test')
@requests_timeout
class TestS3270Portable(cti):

    # Start s3270 with the supplied portable-mode options.
    def start_s3270(self, executable, options):
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap([executable] + options + ['-httpd',
            f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)
        return http_port, s3270

    # Check the portable state, configuration directory and trace directory.
    def check_portable(self, http_port, expected, confdir, installdir=None):
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Portable)')
        self.assertEqual([str(expected).lower()], r.json()['result'])

        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Show(confDir)')
        actual_confdir = r.json()['result'][0]
        self.assertEqual(os.path.normcase(os.path.normpath(confdir)),
            os.path.normcase(os.path.normpath(actual_confdir)))

        if installdir is not None:
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Trace(on)')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(TraceFile)')
            tracefile = r.json()['result'][0]
            self.assertEqual(os.path.normcase(os.path.normpath(installdir)),
                os.path.normcase(os.path.normpath(os.path.dirname(tracefile))))

    # Stop s3270.
    def stop_s3270(self, http_port, s3270):
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')
        self.vgwait(s3270)

    # Test the flag file and command-line portable mode controls.
    def test_s3270_portable(self):
        source = shutil.which('s3270.exe')
        self.assertIsNotNone(source, 'Could not find s3270.exe in PATH')

        with tempfile.TemporaryDirectory() as installdir:
            shutil.copytree(os.path.dirname(source), installdir, dirs_exist_ok=True)
            executable = os.path.join(installdir, os.path.basename(source))
            flagfile = os.path.join(installdir, 'PORTABLE.txt')

            with open(flagfile, 'w'):
                pass
            http_port, s3270 = self.start_s3270(executable, [])
            self.check_portable(http_port, True, installdir, installdir)
            self.stop_s3270(http_port, s3270)

            http_port, s3270 = self.start_s3270(executable, ['-noportable'])
            self.check_portable(http_port, False, os.getcwd())
            self.stop_s3270(http_port, s3270)

            os.unlink(flagfile)
            http_port, s3270 = self.start_s3270(executable, ['-portable'])
            self.check_portable(http_port, True, installdir, installdir)
            self.stop_s3270(http_port, s3270)

if __name__ == '__main__':
    unittest.main()
