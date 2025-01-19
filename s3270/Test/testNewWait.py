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
# Tests for new Wait() options

from subprocess import Popen
import threading
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestNewWait(cti):

    def to_playback(self, port: int, second_actions, p: playback = None, n=0):
        '''Write a string to playback after verifying the emulator is blocked'''
        # Wait for the action to block.
        def test():
            j = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
            return any('Wait(' in line for line in j['result'])
        self.try_until(test, 2, "emulator did not block")
        # Perform the additional actions.
        for action in second_actions:
            self.get(f'http://127.0.0.1:{port}/3270/rest/json/{action}')
        # Push additional records.
        if n > 0:
            p.send_records(1, send_tm=False)

    def new_wait(self, initial_eors, second_actions, wait_params, p: playback = None, n: int=0):

        # Start 'playback' to drive s3270.
        playback_port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Start s3270 with a webserver.
            s3270_port, ts = unused_port()
            s3270 = Popen(vgwrap(["s3270", "-httpd", f"127.0.0.1:{s3270_port}", f"127.0.0.1:{playback_port}"]))
            self.children.append(s3270)
            self.check_listen(s3270_port)
            ts.close()

            # Step until the login screen is visible.
            p.send_records(initial_eors)

            # In the background, wait for the Wait() action to block, then perform the additional actions.
            x = threading.Thread(target=self.to_playback, args=(s3270_port, second_actions, p, n))
            x.start()

            # Wait for the change.
            r = self.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Wait({wait_params})')
            self.assertTrue(r.ok)

        # Wait for the processes to exit.
        x.join(timeout=2)
        self.get(f'http://127.0.0.1:{s3270_port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Generic flavor of CursorAt test.
    def test_cursor_at(self):
        self.new_wait(4, ['Up()'], 'CursorAt,20,13')
    def test_cursor_at_offset(self):
        self.new_wait(4, ['Up()'], 'CursorAt,1532')

    # Generic flavor of StringAt test.
    def test_string_at(self):
        self.new_wait(4, ['String("xxx")'], 'StringAt,21,13,"xx"')
    def test_string_at_offset(self):
        self.new_wait(4, ['String("xxx")'], 'StringAt,1612,"xx"')

    # Generic flavor of InputFieldAt test.
    def test_input_field_at(self):
        self.new_wait(3, [], 'InputFieldAt,21,13', playback, 1)
    def test_input_field_at_offset(self):
        self.new_wait(3, [], 'InputFieldAt,1612', playback, 1)

    # Simple negative test framework.
    def simple_negative_test(self, port, action, message):
        # Send the action to s3270.
        r = self.get(f"http://127.0.0.1:{port}/3270/rest/json/{action}")
        self.assertFalse(r.ok);
        self.assertTrue(message in r.json()['result'][0])

    # Some basic negative tests.
    def test_simple_negatives(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(["s3270", "-httpd", f"127.0.0.1:{port}"]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Syntax tests.
        self.simple_negative_test(port, 'Wait(CursorAt)', 'requires')
        self.simple_negative_test(port, 'Wait(CursorAt,1,2,3)', 'requires')
        self.simple_negative_test(port, 'Wait(CursorAt,fred,joe)', 'Invalid')
        self.simple_negative_test(port, 'Wait(CursorAt,9999999)', 'Invalid')
        self.simple_negative_test(port, 'Wait(CursorAt,300,300)', 'Invalid')
        self.simple_negative_test(port, 'Wait(StringAt)', 'requires')
        self.simple_negative_test(port, 'Wait(StringAt,1,2,3,4)', 'requires')
        self.simple_negative_test(port, 'Wait(InputFieldAt)', 'requires')
        self.simple_negative_test(port, 'Wait(InputFieldAt,1,2,3)', 'requires')

        # Not-connected tests.
        self.simple_negative_test(port, 'Wait(CursorAt,0,0)', 'connected')
        self.simple_negative_test(port, 'Wait(StringAt,0,0,"Hello")', 'connected')
        self.simple_negative_test(port, 'Wait(InputFieldAt,0,0)', 'connected')

        # Clean up.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Run an action that succeeds immediately.
    def nop(self, port, action):
        r = self.get(f"http://127.0.0.1:{port}/3270/rest/json/{action}")
        self.assertTrue(r.ok)

    # No-op tests (things that don't block).
    def test_nops(self):

        # Start 'playback' to drive s3270.
        pport, pts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=pport) as p:
            pts.close()

            # Start s3270 with a webserver.
            sport, sts = unused_port()
            s3270 = Popen(vgwrap(["s3270", "-httpd", f"127.0.0.1:{sport}",
                f"127.0.0.1:{pport}"]))
            self.children.append(s3270)
            self.check_listen(sport)
            sts.close()

            # Get to the login screen.
            p.send_records(4)

            self.nop(sport, 'Wait(CursorAt,21,13)')
            self.nop(sport, 'Wait(InputFieldAt,21,13)')
            self.nop(sport, 'Wait(StringAt,21,13,"___")')

        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
