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

import unittest
from unittest.mock import Mock, call

from ibm3270ds import wcc
import menu
import xtwinops
from ds import sba_bytes
from target import target
from telnet_proto import telcmd, telopt
from tn3270e_proto import data_type

error = 'Unable to start test item'

class SwitchErrorTest(unittest.TestCase):
    # Verify that an error is handed to the previous menu exactly once.
    def test_revert_returns_error_once(self):
        switch = object.__new__(target)
        switch.previous_type = {'peer': 'menu-f'}
        switch.switch_to = {}
        switch.drain = {}
        switch.return_error = {}
        switch.exiting = True

        switch.revert('peer', error=error)

        self.assertEqual(switch.switch_to, {'peer': 'menu-f'})
        self.assertEqual(switch.take_error('peer'), error)
        self.assertIsNone(switch.take_error('peer'))

        switch.return_error['other'] = 'Other peer failure'
        self.assertEqual(switch.take_error('peer'), None)
        self.assertEqual(switch.take_error('other'), 'Other peer failure')

    # Verify standalone failures are printed in NVT before disconnecting.
    def test_revert_displays_error_without_previous_menu(self):
        switch = object.__new__(target)
        switch.previous_type = {}
        switch.switch_to = {}
        switch.drain = {}
        switch.return_error = {}
        switch.logger = Mock()
        switch.exiting = True
        conn = Mock()

        switch.revert('peer', conn, error=error)

        conn.assert_has_calls([
            call.send(error.encode() + b'\r\n'),
            call.close(),
        ])

class MenuErrorTest(unittest.TestCase):
    # Build a switch fixture with one pending error message.
    def make_switch(self):
        switch = Mock()
        switch.list.return_value = {'echo': 'Simple echo'}
        switch.take_error.return_value = error
        return switch

    # Verify the plain TELNET menu prints the error before its prompt.
    def test_plain_telnet_menu(self):
        page = object.__new__(menu.menu_t)
        page.conn = Mock()
        page.peername = 'peer'
        page.switch = self.make_switch()

        page.ready()

        output = page.conn.send.call_args.args[0]
        self.assertIn(error.encode() + b'\r\n==> ', output)
        page.switch.take_error.assert_called_once_with('peer')

    # Verify the TN3270E NVT menu prints the error before its prompt.
    def test_nvt_menu(self):
        page = object.__new__(menu.menu_n)
        page.termid = None
        page.peername = 'peer'
        page.switch = self.make_switch()
        page.send_host = Mock()

        page.start3270()

        output, mode = page.send_host.call_args.args
        self.assertIn(error.encode() + b'\r\n==> ', output)
        self.assertEqual(mode, data_type.nvt_data)

    # Verify SSCP-LU menu errors use EBCDIC before the prompt.
    def test_sscp_lu_menu(self):
        page = object.__new__(menu.menu_s)
        page.termid = None
        page.peername = 'peer'
        page.switch = self.make_switch()
        page.send_host = Mock()

        page.start3270()

        output, mode = page.send_host.call_args.args
        self.assertIn(error.encode('cp037') + b'\x15' + '==> '.encode('cp037'), output)
        self.assertEqual(mode, data_type.sscp_lu_data)

    # Verify the unformatted 3270 menu puts its error before the prompt text.
    def test_unformatted_3270_menu(self):
        page = object.__new__(menu.menu_u)
        page.termid = None
        page.peername = 'peer'
        page.switch = self.make_switch()
        page.send_host = Mock()

        page.start3270()

        output = page.send_host.call_args.args[0]
        error_offset = output.index(error.encode('cp037'))
        prompt_offset = output.index('Press CLEAR, type selection'.encode('cp037'))
        self.assertLess(error_offset, prompt_offset)
        self.assertEqual(output[1] & wcc.sound_alarm, wcc.sound_alarm)

    # Verify existing unformatted-menu validation errors still display.
    def test_unformatted_menu_command_error(self):
        page = object.__new__(menu.menu_u)
        page.termid = None
        page.peername = 'peer'
        page.switch = self.make_switch()
        page.send_host = Mock()

        page.display_menu(menu.to_ebc(menu.no_such('unknown', b'')))

        output = page.send_host.call_args.args[0]
        self.assertIn('No such service: unknown'.encode('cp037'), output)

    # Verify the formatted 3270 menu puts the error on the row above the prompt.
    def test_formatted_3270_menu(self):
        page = object.__new__(menu.menu_f)
        page.termid = None
        page.peername = 'peer'
        page.switch = self.make_switch()
        page.send_host = Mock()
        page.raw_cmd = b''

        page.start3270()

        output = page.send_host.call_args.args[0]
        self.assertIn(sba_bytes(21, 1, 80) + error.encode('cp037'), output)
        self.assertIn(sba_bytes(22, 1, 80) + menu.to_ebc(b'==>'), output)
        self.assertEqual(output[1] & wcc.sound_alarm, wcc.sound_alarm)

class NvtCookedInputTest(unittest.TestCase):
    # Create a plain-TELNET menu page.
    def menu_page(self):
        conn = Mock()
        switch = Mock()
        switch.list.return_value = {'xtwinops': 'XTWINOPS'}
        return menu.menu_t(conn, None, 'peer', False, switch, None), conn, switch

    # Create an XTWINOPS page.
    def xtwinops_page(self):
        conn = Mock()
        switch = Mock()
        switch.is_switched.return_value = False
        return xtwinops.xtwinops(conn, None, 'peer', False, switch, None), conn

    # Verify menu commands wait for fragmented input and a terminator.
    def test_menu_fragmented_command(self):
        page, conn, switch = self.menu_page()

        page.process(b'xt')
        page.process(b'winops')
        conn.send.assert_not_called()
        switch.switch.assert_not_called()
        page.process(b'\r\n')

        switch.switch.assert_called_once_with('peer', 'xtwinops')
        conn.send.assert_not_called()

    # Verify CR, LF and CRLF delimit commands with no duplicate CRLF command.
    def test_xtwinops_terminators(self):
        page, _, = self.xtwinops_page()
        page.process_command = Mock()

        page.process(b'one\r')
        page.process(b'\ntwo\nthree\r\n')
        page.process(b'\r\n')

        self.assertEqual(page.process_command.call_args_list,
                         [call(b'one'), call(b'two'), call(b'three'), call(b'')])

    # Verify backspace deletes a character, including from an empty line.
    def test_xtwinops_backspace(self):
        page, conn = self.xtwinops_page()
        page.process_command = Mock()

        page.process(b'ab\x08c\r')
        page.process(b'\x08\r')

        self.assertEqual(page.process_command.call_args_list, [call(b'ac'), call(b'')])
        conn.send.assert_not_called()

    # Verify kill-line clears a line, including an already empty line.
    def test_xtwinops_kill_line(self):
        page, conn = self.xtwinops_page()
        page.process_command = Mock()

        page.process(b'ab\x15\r')
        page.process(b'\x15\r')

        self.assertEqual(page.process_command.call_args_list, [call(b''), call(b'')])
        conn.send.assert_not_called()

    # Verify rprnt retransmits the editable pending line.
    def test_xtwinops_rprnt(self):
        page, conn = self.xtwinops_page()
        page.process_command = Mock()

        page.process(b'ab\x12c\r')

        conn.send.assert_called_once_with(b'\r\nab')
        page.process_command.assert_called_once_with(b'abc')

    # Verify XTWINOPS commands are dispatched only after their terminator.
    def test_xtwinops_command_dispatch(self):
        page, _ = self.xtwinops_page()
        page.process_command = Mock()

        page.process(b'iconify')
        page.process(b'\n')

        page.process_command.assert_called_once_with(b'iconify')

    # Verify normal mode undoes the character-mode negotiation.
    def test_xtwinops_normal_mode(self):
        page, conn = self.xtwinops_page()

        page.enter_normal_mode()

        conn.send.assert_called_once_with(bytes([
            int(telcmd.IAC), int(telcmd.WONT), int(telopt.ECHO),
            int(telcmd.IAC), int(telcmd.WONT), int(telopt.SGA),
        ]))

    # Verify a report timeout restores normal TELNET mode.
    def test_xtwinops_timeout_restores_normal_mode(self):
        page, conn = self.xtwinops_page()
        page.enter_character_mode = Mock()
        page.wait_for_response = Mock(return_value=None)

        page.process_command(b'window-state')

        report = call.send(b'\033[11t')
        normal = call.send(bytes([
            int(telcmd.IAC), int(telcmd.WONT), int(telopt.ECHO),
            int(telcmd.IAC), int(telcmd.WONT), int(telopt.SGA),
        ]))
        self.assertLess(conn.mock_calls.index(report), conn.mock_calls.index(normal))
        page.wait_for_response.assert_called_once_with('window-state')

if __name__ == '__main__':
    unittest.main()
