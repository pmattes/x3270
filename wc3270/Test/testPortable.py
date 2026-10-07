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
# wc3270 portable mode tests

import os
import shutil
import sys
import tempfile
import unittest

from Common.Test.cti import *

@unittest.skipUnless(sys.platform.startswith('win'), 'Only works on native Windows')
@requests_timeout
class TestWc3270Portable(cti):

    # Start wc3270 with the supplied portable-mode options.
    def start_wc3270(self, executable, installdir, options, instance):
        http_port, ts = unused_port()
        session = os.path.join(installdir, f'portable-{instance}.wc3270')
        with open(session, 'w') as f:
            f.write(f'wc3270.httpd: 127.0.0.1:{http_port}\n')

        self.assertEqual(0, os.system(
            f'start "" conhost "{executable}" {" ".join(options)} "{session}"'))
        self.check_listen(http_port)
        ts.close()
        return http_port

    # Check the portable state and its default trace directory.
    def check_portable(self, http_port, expected, installdir=None):
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Portable)')
        self.assertEqual([str(expected).lower()], r.json()['result'])

        if installdir is not None:
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Trace(on)')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(TraceFile)')
            tracefile = r.json()['result'][0]
            self.assertEqual(os.path.normcase(os.path.normpath(installdir)),
                os.path.normcase(os.path.normpath(os.path.dirname(tracefile))))

    # Stop wc3270.
    def stop_wc3270(self, http_port):
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')

    # Test the flag file and command-line portable mode controls.
    def test_wc3270_portable(self):
        source = shutil.which('wc3270.exe')
        self.assertIsNotNone(source, 'Could not find wc3270.exe in PATH')

        with tempfile.TemporaryDirectory() as installdir:
            shutil.copytree(os.path.dirname(source), installdir, dirs_exist_ok=True)
            executable = os.path.join(installdir, os.path.basename(source))
            flagfile = os.path.join(installdir, 'PORTABLE.txt')

            with open(flagfile, 'w'):
                pass
            http_port = self.start_wc3270(executable, installdir, [], 1)
            self.check_portable(http_port, True, installdir)
            self.stop_wc3270(http_port)

            http_port = self.start_wc3270(executable, installdir, ['-noportable'], 2)
            self.check_portable(http_port, False)
            self.stop_wc3270(http_port)

            os.unlink(flagfile)
            http_port = self.start_wc3270(executable, installdir, ['-portable'], 3)
            self.check_portable(http_port, True, installdir)
            self.stop_wc3270(http_port)

if __name__ == '__main__':
    unittest.main()
