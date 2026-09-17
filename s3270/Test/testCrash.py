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
#       notice, this list of conditions and the following disclaimer.
#     * Neither the name of Paul Mattes nor his contributors may be used to
#       endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE.
#
# s3270 crash test

import os
from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Crash(cti):

    segv = 0xc0000005 if sys.platform.startswith('win') else -11

    # Run one of the Crash(Exit) tests.
    def run_exit_test(self, action, expected_status):
        hport, socket = unused_port()
        env = os.environ.copy()
        env['CRASH'] = '1'
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']),
                stdin=DEVNULL, stdout=DEVNULL, env=env)
        self.children.append(s3270)
        self.check_listen(hport)
        socket.close()

        try:
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/{action}')
            success = True
        except:
            success = False
        if action.startswith('Crash(Async'):
            self.assertTrue(success)
        else:
            self.assertFalse(success)
        self.vgwait(s3270, assertOnFailure=False, expected_status=expected_status)
        self.children.remove(s3270)

    # s3270 crash test
    def test_s3270_crash(self):

        # Start s3270 with the unit test environment enabled.
        hport, socket = unused_port()
        env = os.environ.copy()
        env['CRASH'] = '1'
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']),
                stdin=DEVNULL, stdout=DEVNULL, env=env)
        self.children.append(s3270)
        self.check_listen(hport)
        socket.close()

        # Force it to crash.
        try:
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Crash(null)', timeout=5)
            success = True
        except:
            success = False
        self.assertFalse(success)

        # Wait for the process to exit, killed by a signal.
        self.vgwait(s3270, assertOnFailure=False, expected_status=self.segv)

        # Remove s3270 from the child list, because the cti tear_down function crashes when a
        # child is killed by a signal. This is safe to do because we have already waited for
        # the child to exit above.
        self.children.remove(s3270)

    # Crash() should not be known without -utenv and CRASH.
    def test_s3270_crash_unknown(self):

        hport, socket = unused_port()
        env = os.environ.copy()
        env.pop('CRASH', None)
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport)]),
                stdin=DEVNULL, stdout=DEVNULL, env=env)
        self.children.append(s3270)
        self.check_listen(hport)
        socket.close()

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Crash(null)')
        self.assertEqual('Unknown action: Crash', r.json()['result'][0])

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_crash_exit_code(self):
        self.run_exit_test('Crash(Exit,42)', 42)

    def test_s3270_crash_async_exit(self):
        self.run_exit_test('Crash(Async,Exit,43)', 43)

    def test_s3270_crash_async_null(self):
        self.run_exit_test('Crash(Async,Null)', self.segv)

if __name__ == '__main__':
    unittest.main()
