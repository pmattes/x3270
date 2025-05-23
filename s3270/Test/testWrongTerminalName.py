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
# s3270 wrong terminal name tests

from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270WrongTerminalName(cti):

    # Run the test in one of two modes.
    def run_wtn(self, override: bool):

        # Start s3270.
        sport, socket = unused_port()
        socket.close()
        args = ['s3270', '-httpd', f':{sport}']
        if override:
            args.append('-set')
            args.append('wrongTerminalName')
        s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(sport)

        # Query the terminal name.
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(terminalName)').json()['result'][0]

        # Make sure it works.
        if override:
            self.assertEqual(j, 'IBM-3279-4-E')
        else:
            self.assertEqual(j, 'IBM-3278-4-E')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Test default behavior.
    def test_s3270_right_terminal_name(self):
        self.run_wtn(False)

    # Test override (pre-4.3) behavior.
    def test_s3270_wrong_terminal_name(self):
        self.run_wtn(True)

    # Test switching modes.
    def test_s3270_change_terminal_name_mode(self):

        # Start s3270.
        sport, socket = unused_port()
        socket.close()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f':{sport}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(sport)

        # Query the terminal name and model.
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(terminalName)').json()['result'][0]
        self.assertEqual(j, 'IBM-3278-4-E')
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model))').json()['result'][0]
        self.assertEqual(j, '3279-4-E')

        # Switch modes.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(wrongTerminalName,true)')

        # Check again.
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(terminalName)').json()['result'][0]
        self.assertEqual(j, 'IBM-3279-4-E')
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model))').json()['result'][0]
        self.assertEqual(j, '3279-4-E')

        # Switch modes back.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(wrongTerminalName,false)')

        # Check again.
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(terminalName)').json()['result'][0]
        self.assertEqual(j, 'IBM-3278-4-E')
        j = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model))').json()['result'][0]
        self.assertEqual(j, '3279-4-E')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
