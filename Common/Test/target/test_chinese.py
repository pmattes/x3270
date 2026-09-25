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

from ds import decode_address, dinfo, sba_bytes
from ibm3270ds import aid, command, fa, order
from tn3270_chinese import build_screen, chinese, poem, required_cgcsgid

# Create a minimally initialized page for input-dispatch tests.
def input_page() -> chinese:
    page = object.__new__(chinese)
    page.termid = None
    page.in3270 = True
    return page

# Create a character-set QueryReply fixture.
def charset_reply(cgcsgid: int) -> bytes:
    charsets = bytes.fromhex(
        '8e000000000000000b'
        '0000000000000004380345'
        '0100f10000000003c30136'
        '8020f80000417f'
    ) + cgcsgid.to_bytes(4, 'big')
    subfield_len = len(charsets) + 4
    subfield = subfield_len.to_bytes(2, 'big') + bytes([aid.SF_QREPLY, 0x85]) + charsets
    return bytes([aid.SF]) + subfield

class ChinesePageTest(unittest.TestCase):
    # Check that the DBCS CGCSGID is parsed from the QueryReply.
    def test_parse_dbcs_cgcsgid(self):
        info = dinfo('IBM-3278-2-E')
        self.assertEqual(info.parse_query_reply(charset_reply(required_cgcsgid)), (True, ''))
        self.assertEqual(info.cgcsgid_dbcs, required_cgcsgid)
        self.assertTrue(info.dbcs)

    # Check that a different terminal CGCSGID is retained for rejection.
    def test_parse_unexpected_dbcs_cgcsgid(self):
        info = dinfo('IBM-3278-2-E')
        other = 0x12345678
        self.assertEqual(info.parse_query_reply(charset_reply(other)), (True, ''))
        self.assertEqual(info.cgcsgid_dbcs, other)

    # Check highlighting, unmodified poem bytes and lower-right cursor position.
    def test_build_screen(self):
        text = b'\x0e\x42\x43\x0f\x25second'
        screen = build_screen(text, 24, 80)
        self.assertEqual(screen[0], command.erase_write_alternate)
        self.assertIn(sba_bytes(1, 1, 80) + bytes([order.sf, fa.protect | fa.high_sel]), screen)
        self.assertIn(b'\x0e\x42\x43\x0f', screen)
        self.assertIn(sba_bytes(2, 1, 80) + bytes([order.sf, fa.protect]), screen)
        self.assertIn(sba_bytes(24, 1, 80) + 'F3=END'.encode('cp037'), screen)
        self.assertTrue(screen.endswith(sba_bytes(24, 80, 80) + bytes([order.ic])))
        cursor_address = decode_address(screen[-3:-1])
        self.assertEqual(cursor_address, 24 * 80 - 1)
        self.assertLess(cursor_address, 24 * 80)
        self.assertTrue(build_screen(poem, 24, 80))
        larger_screen = build_screen(text, 43, 80)
        self.assertIn(sba_bytes(43, 1, 80) + 'F3=END'.encode('cp037'), larger_screen)
        self.assertEqual(decode_address(larger_screen[-3:-1]), 43 * 80 - 1)

    # Reject a poem that cannot fit without wrapping over the status row.
    def test_reject_oversized_poem(self):
        with self.assertRaises(ValueError):
            build_screen(b'x' * 80, 24, 80)

    # Check that PF3 exits through the server's normal return path.
    def test_pf3_exits_page(self):
        page = input_page()
        page.hangup = Mock()
        page.rcv_data_cooked(bytes([aid.PF3]))
        page.hangup.assert_called_once_with()

    # Reject a terminal that reports a different DBCS CGCSGID.
    def test_reject_wrong_cgcsgid(self):
        page = input_page()
        page.dinfo = SimpleNamespace(
            parse_query_reply=Mock(return_value=(True, '')),
            cgcsgid_dbcs=0x12345678,
        )
        page.fail = Mock()
        page.display_poem = Mock()
        page.rcv_data_cooked(bytes([aid.SF]))
        page.fail.assert_called_once()
        page.display_poem.assert_not_called()

    # Display the poem only when the reported DBCS CGCSGID matches.
    def test_display_for_required_cgcsgid(self):
        page = input_page()
        page.dinfo = SimpleNamespace(
            parse_query_reply=Mock(return_value=(True, '')),
            cgcsgid_dbcs=required_cgcsgid,
        )
        page.fail = Mock()
        page.display_poem = Mock()
        page.rcv_data_cooked(bytes([aid.SF]))
        page.display_poem.assert_called_once_with()
        page.fail.assert_not_called()
