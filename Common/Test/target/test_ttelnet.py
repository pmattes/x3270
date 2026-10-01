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
#     documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
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

# x3270 test target, TELNET unit tests.

import unittest
from unittest.mock import Mock, call

from telnet_proto import telcmd, telopt
from ttelnet import ttelnet

class TelnetConsumer:
    # Initialize the test TELNET consumer.
    def __init__(self):
        self.data = []

    # Record application data.
    def rcv_data(self, data):
        self.data.append(data)

    # Reject unsolicited client options.
    def rcv_will(self, option):
        return False

    # Accept client option disablement.
    def rcv_wont(self, option):
        return True

    # Reject unsolicited server options.
    def rcv_do(self, option):
        return False

    # Accept server option disablement.
    def rcv_dont(self, option):
        return True

    # Ignore subnegotiation.
    def rcv_sb(self, option, data):
        pass

    # Ignore TELNET commands.
    def rcv_cmd(self, command):
        pass

class TelnetTest(unittest.TestCase):
    # Build a TELNET parser with a recording consumer.
    def make_telnet(self):
        conn = Mock()
        consumer = TelnetConsumer()
        telnet = ttelnet(conn, Mock(), 'peer', consumer, Mock())
        return telnet, conn, consumer

    # Verify cooked input is delivered only as complete edited lines.
    def test_cooked_input(self):
        telnet, conn, consumer = self.make_telnet()
        telnet.set_cooked_input(True)

        telnet.process(b'ab\x08c\x15xy\x12z\r\n')

        self.assertEqual(consumer.data, [b'xyz'])
        conn.send.assert_called_once_with(b'\r\nxy')

    # Verify uncooked input preserves existing byte delivery.
    def test_uncooked_input(self):
        telnet, _, consumer = self.make_telnet()

        telnet.process(b'ab')

        self.assertEqual(consumer.data, [b'a', b'b'])

    # Verify character mode tracks parsed client acknowledgements.
    def test_character_mode_acknowledgement(self):
        telnet, conn, _ = self.make_telnet()

        telnet.enter_character_mode()
        telnet.process(bytes([int(telcmd.IAC), int(telcmd.DO), int(telopt.ECHO),
                              int(telcmd.IAC), int(telcmd.DO), int(telopt.SGA)]))

        self.assertTrue(telnet.character_mode_acked())
        telnet.enter_normal_mode()
        self.assertEqual(conn.send.call_args_list, [
            call(bytes([int(telcmd.IAC), int(telcmd.WILL), int(telopt.ECHO)])),
            call(bytes([int(telcmd.IAC), int(telcmd.WILL), int(telopt.SGA)])),
            call(bytes([int(telcmd.IAC), int(telcmd.WONT), int(telopt.ECHO)])),
            call(bytes([int(telcmd.IAC), int(telcmd.WONT), int(telopt.SGA)])),
        ])

if __name__ == '__main__':
    unittest.main()
