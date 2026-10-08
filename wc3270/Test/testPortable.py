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
    def start_wc3270(self, executable: str, options: list[str]) -> tuple[int, Popen]:
        http_port, ts = unused_port()
        self.assertEqual(0, os.system(
            f'start "" conhost "{executable}" -httpd :{http_port} {" ".join(options)}'))
        self.check_listen(http_port)
        ts.close()
        return http_port

    # Stop wc3270.
    def is_dead(self, http_port: int) -> bool:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            s.connect(('127.0.0.1', http_port))
            s.close()
            return False
        except ConnectionRefusedError:
            s.close()
            return True
    def stop_wc3270(self, http_port: int):
        # We need to use indirect means to make sure wc3270 is dead, because it is
        # a child of conhost, not a child of this script.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')
        self.try_until(lambda: self.is_dead(http_port), 2, 'expected wc3270 to exit')

    # Check the portable state, configuration directory and trace directory.
    def check_portable(self, http_port: int, expected: str, confdir: str, installdir=None):
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Portable)')
        q_portable = r.json()['result']

        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(confDir)')
        actual_confdir = r.json()['result'][0]

        if installdir is not None:
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Trace(on)')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(TraceFile)')
            tracefile = r.json()['result'][0]

        # Stop wc3270 before checking anything.
        self.stop_wc3270(http_port)

        # Check.
        self.assertEqual([expected], q_portable)
        self.assertEqual(os.path.normcase(os.path.normpath(confdir)),
            os.path.normcase(os.path.normpath(actual_confdir)))
        if installdir is not None:
            os.unlink(tracefile)
            self.assertEqual(os.path.normcase(os.path.normpath(confdir)),
                os.path.normcase(os.path.normpath(actual_confdir)))

    # Test the flag file and command-line portable mode controls.
    def wc3270_portable(self, cmdline: list[str], expected: str, portable=False, create_flagfile=False):
        source = shutil.which('wc3270.exe')
        self.assertIsNotNone(source, 'Could not find wc3270.exe in PATH')

        with tempfile.TemporaryDirectory() as installdir:
            shutil.copytree(os.path.dirname(source), installdir, dirs_exist_ok=True)
            executable = os.path.join(installdir, os.path.basename(source))
            flagfile = os.path.join(installdir, 'PORTABLE.txt')

            if create_flagfile:
                with open(flagfile, 'w'):
                    pass
            http_port = self.start_wc3270(executable, cmdline)
            self.check_portable(http_port, expected, confdir=installdir if portable else os.getcwd(), installdir=installdir if portable else None)

    def test_wc3270_portable_default(self):
        self.wc3270_portable([], 'disabled default')
    def test_wc3270_portable_flagfile(self):
        self.wc3270_portable([], 'enabled flag-file', portable=True, create_flagfile=True)
    def test_wc3270_portable_cmdline_disabled(self):
        self.wc3270_portable(['-noportable'], 'disabled command-line', create_flagfile=True)
    def test_wc3270_portable_cmdline_enabled(self):
        self.wc3270_portable(['-portable'], 'enabled command-line', portable=True)

if __name__ == '__main__':
    unittest.main()
