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
# Tests for new Wait() options

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import threading
import time
import re
import sys
import TestCommon

class TestNewWait(unittest.TestCase):

    def new_wait(self, port, initial_eors, second_actions, wait_params):
        popen_port = port
        s3270_port = port + 1

        # Start 'playback' to drive s3270.
        playback = Popen(["playback", "-w", "-p", str(popen_port),
            "s3270/Test/ibmlink_help.trc"], stdin=PIPE, stdout=DEVNULL)
        TestCommon.check_listen(popen_port)

        # Start s3270 with a webserver.
        s3270 = Popen(["s3270", "-httpd", f"127.0.0.1:{s3270_port}", f"127.0.0.1:{popen_port}"])
        TestCommon.check_listen(s3270_port)

        # Step until the login screen is visible.
        for i in range(initial_eors):
            playback.stdin.write(b'r\n')
        playback.stdin.write(b"c done with initialization\n")
        playback.stdin.flush()
        TestCommon.check_push(playback, s3270_port, 1)

        # Wait for the initial screen.
        requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Wait(InputField)', timeout=2)

        # Perform the additional actions.
        for action in second_actions:
            requests.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/{action}', timeout=2)

        # In the background, wait for the Wait() action to block, then
        # push out another record.
        x = threading.Thread(target=TestCommon.to_playback, args=(playback, s3270_port, b"r\n"))
        x.start()

        # Wait for the change.
        r = requests.get(
                f'http://127.0.0.1:{s3270_port}/3270/rest/json/Wait({wait_params})',
                timeout=2)
        self.assertEqual(r.status_code, requests.codes.ok)

        # Wait for the processes to exit.
        x.join(timeout=2)
        playback.stdin.close()
        playback.kill()
        playback.wait(timeout=2)
        s3270.kill()
        s3270.wait(timeout=2)

    # Generic flavor of CursorAt test.
    def test_cursor_at(self):
        self.new_wait(10003, 4, ['Up()'], 'CursorAt,21,13')
    def test_cursor_at_offset(self):
        self.new_wait(10001, 4, ['Up()'], 'CursorAt,1612')

    # Generic flavor of StringAt test.
    def test_string_at(self):
        self.new_wait(9999, 4, ['String("xxx")'], 'StringAt,21,13,"__"')
    def test_string_at_offset(self):
        self.new_wait(9997, 4, ['String("xxx")'], 'StringAt,1612,"__"')

    # Generic flavor of InputFieldAt test.
    def test_input_field_at(self):
        self.new_wait(9995, 6, [], 'InputFieldAt,21,13')
    def test_input_field_at_offset(self):
        self.new_wait(9993, 6, [], 'InputFieldAt,1612')

    # Simple negative test framework.
    def simple_negative_test(self, action, message):
        # Send the action to s3270.
        r = requests.get(f"http://127.0.0.1:9992/3270/rest/json/{action}")
        self.assertEqual(r.status_code, requests.codes.bad)
        self.assertTrue(message in r.json()['result'][0])

    # Some basic negative tests.
    def test_simple_negatives(self):
        # Syntax tests.
        s3270 = Popen(["s3270", "-httpd", "127.0.0.1:9992"])
        TestCommon.check_listen(9992)

        # Syntax tests.
        self.simple_negative_test('Wait(CursorAt)', 'requires')
        self.simple_negative_test('Wait(CursorAt,1,2,3)', 'requires')
        self.simple_negative_test('Wait(CursorAt,fred,joe)', 'Invalid')
        self.simple_negative_test('Wait(CursorAt,9999999)', 'Invalid')
        self.simple_negative_test('Wait(CursorAt,300,300)', 'Invalid')
        self.simple_negative_test('Wait(StringAt)', 'requires')
        self.simple_negative_test('Wait(StringAt,1,2,3,4)', 'requires')
        self.simple_negative_test('Wait(InputFieldAt)', 'requires')
        self.simple_negative_test('Wait(InputFieldAt,1,2,3)', 'requires')

        # Not-connected tests.
        self.simple_negative_test('Wait(CursorAt,0,0)', 'connected')
        self.simple_negative_test('Wait(StringAt,0,0,"Hello")', 'connected')
        self.simple_negative_test('Wait(InputFieldAt,0,0)', 'connected')

        # Clean up.
        s3270.kill()
        s3270.wait(timeout=2)

    # Run an action that succeeds immediately.
    def nop(self, p, action):
        r = requests.get(f"http://127.0.0.1:9990/3270/rest/json/{action}", timeout=2)
        self.assertEqual(r.status_code, requests.codes.ok)

    # No-op tests (things that don't block).
    def test_nops(self):
        # Start 'playback' to drive s3270.
        playback = Popen(["playback", "-w", "-p", "9991",
            "s3270/Test/ibmlink_help.trc"], stdin=PIPE, stdout=DEVNULL)
        TestCommon.check_listen(9991)

        # Start s3270 with a webserver.
        s3270 = Popen(["s3270", "-httpd", "127.0.0.1:9990", "127.0.0.1:9991"])
        TestCommon.check_listen(9990)

        # Get to the login screen.
        playback.stdin.write(b'r\nr\nr\nr\n')
        playback.stdin.flush()

        self.nop(playback, 'Wait(CursorAt,21,13)')
        self.nop(playback, 'Wait(InputFieldAt,21,13)')
        self.nop(playback, 'Wait(StringAt,21,13,"___")')

        playback.stdin.close()
        playback.kill()
        playback.wait(timeout=2)
        s3270.kill()
        s3270.wait(timeout=2)

if __name__ == '__main__':
    unittest.main()
