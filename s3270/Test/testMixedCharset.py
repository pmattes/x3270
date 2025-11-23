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
# s3270 reply mode tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270MixedCharset(cti):

    # s3270 mixed charset APL test.
    def test_s3270_mix_apl(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo_mod.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(2)

            # Try to overwrite an APL field with a non-APL character. It should fail silently.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(3,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String(x)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(cursor1)')
            self.assertTrue(r.ok)
            self.assertEqual('row 3 column 11 offset 170', r.json()['result'][0], 'cursor should not have moved')
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(3,11,1)')
            self.assertTrue(r.ok)
            self.assertEqual('J', r.json()['result'][0], 'field should be unchanged')

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 test to make sure that EraseEOF() does not change charsets in field reply mode.
    def test_s3270_erase_eof_leaves_charsets(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/dbcs_combo_mod_field_mode.trc', port=pport) as p:
            socket.close()

            # Start s3270 in DBCS mode.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-codepage', '930', f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Erase the first field, which is defined as DBCS with an SA.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/EraseEOF()')
            self.assertTrue(r.ok)

            # Try overwriting the first character (which is DBCS) with SBCS. It should lock the keyboard.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String(x)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(KeyboardLockDetail)')
            self.assertTrue(r.ok)
            self.assertEqual('oerr-dbcs', r.json()['result'][0])

            # Reset.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Reset()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(KeyboardLockDetail)')
            self.assertTrue(r.ok)
            self.assertEqual('', r.json()['result'][0])

            # Try overwriting an SBCS character with a DBCS character. It should also lock the keyboard.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,19)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String(国)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(KeyboardLockDetail)')
            self.assertTrue(r.ok)
            self.assertEqual('oerr-dbcs', r.json()['result'][0])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 test to make sure that you can't insert or delete if it would change charset SAs in field reply mode.
    def test_s3270_insert_delete_leaves_charsets(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/dbcs_combo_mod_field_mode.trc', port=pport) as p:
            socket.close()

            # Start s3270 in DBCS mode.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-codepage', '930', f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Try to delete the first character in the first field, which is defined as DBCS with an SA.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Delete()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,11,1)')
            self.assertTrue(r.ok)
            self.assertEqual('国', r.json()['result'][0])

            # Erase the last character to make room to insert.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,23)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/EraseEOF()')
            self.assertTrue(r.ok)

            # Try to insert at the front.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Insert()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String(の)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,11,1)')
            self.assertTrue(r.ok)
            self.assertEqual('国', r.json()['result'][0])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 test to make sure that you can insert or delete if it would change charset SAs in character reply mode.
    def test_s3270_insert_delete_ldbcs(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/dbcs_combo_mod.trc', port=pport) as p:
            socket.close()

            # Start s3270 in DBCS mode.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-codepage', '930', f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(2)

            # Try to delete the first character in the first field, which is defined as DBCS with an SA.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Delete()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,11,1)')
            self.assertTrue(r.ok)
            self.assertEqual('内', r.json()['result'][0])

            # Erase the last character to make room to insert.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,23)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/EraseEOF()')
            self.assertTrue(r.ok)

            # Try to insert at the front.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/MoveCursor1(2,11)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Insert()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/String(の)')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,11,1)')
            self.assertTrue(r.ok)
            self.assertEqual('の', r.json()['result'][0])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
