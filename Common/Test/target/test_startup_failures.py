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

import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from ibm3270ds import aid
from tn3270_colortest import colortest
from tn3270_sruvm import sruvm
from ttelnet import ttelnet

class StartFailureTest(unittest.TestCase):
    # Verify SRUVM model rejection returns an error without drawing a screen.
    def test_sruvm_model_failure(self):
        page = object.__new__(sruvm)
        page.termid = None
        page.dinfo = SimpleNamespace(ttype='IBM-3278-2-E')
        page.debug = Mock()
        page.hangup = Mock()
        page.send_host = Mock()

        page.start3270()

        page.hangup.assert_called_once_with('Plain model 4 required.')
        page.send_host.assert_not_called()

    # Verify color terminal-type rejection returns an error without a screen.
    def test_color_terminal_type_failure(self):
        page = object.__new__(colortest)
        page.termid = None
        page.dinfo = SimpleNamespace(ttype='VT100')
        page.debug = Mock()
        page.hangup = Mock()
        page.send_host = Mock()

        page.start3270()

        page.hangup.assert_called_once_with(
            'Need an extended (-E) model of 3270 or IBM-DYNAMIC terminal type.')
        page.send_host.assert_not_called()

    # Verify color size rejection after QueryReply uses the error handoff.
    def test_color_screen_size_failure(self):
        page = object.__new__(colortest)
        page.termid = None
        page.in3270 = True
        page.dinfo = SimpleNamespace(
            parse_query_reply=Mock(return_value=(True, '')),
            alt_rows=24,
            alt_columns=80,
            rpqnames=None,
        )
        page.debug = Mock()
        page.error = Mock()
        page.hangup = Mock()
        page.send_host = Mock()

        page.rcv_data_cooked(bytes([aid.SF]))

        page.hangup.assert_called_once_with(
            'Need 43 rows x 80 columns at minimum.')
        page.send_host.assert_not_called()

    # Verify switched failures undo TELNET before passing the message back.
    def test_hangup_returns_error_to_previous_menu(self):
        server = object.__new__(ttelnet)
        server.peername = 'peer'
        server.switch = Mock()
        server.switch.is_switched.return_value = True
        server.debug = Mock()
        server.undo = Mock()

        server.hangup('Startup failed')

        server.undo.assert_called_once_with()
        server.switch.revert.assert_called_once_with(
            'peer', error='Startup failed')

    # Verify standalone failures pass the connection and error to the switch.
    def test_hangup_sends_standalone_error(self):
        server = object.__new__(ttelnet)
        server.peername = 'peer'
        server.switch = Mock()
        server.switch.is_switched.return_value = False
        server.undo = Mock()
        server.conn = Mock()

        server.hangup('Startup failed')

        server.undo.assert_called_once_with()
        server.switch.revert.assert_called_once_with(
            'peer', server.conn, error='Startup failed')

if __name__ == '__main__':
    unittest.main()
