#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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
# s3270 insert mode tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import Common.Test.cti as cti
import Common.Test.playback as playback

class TestS3270Insert(cti.cti):

    # s3270 insert overflow test
    # Verifies a fix for a bug that allowed one character to be overwritten
    # when insert overflow occurred.
    def test_s3270_insert_overflow(self):

        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            self.check_listen(pport)
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(5)

            # Fill the first field and go back to its beginning. Then set insert mode.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(ffffffff)')
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Home()')
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(insertMode,true)')

            # Now try inserting into the field. This should fail, because it is coming
            # from a script.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Key(x)')
            self.assertFalse(r.ok, 'Expected Key() to fail')

            # Make sure the field has not been modified.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/AsciiField()')
            self.assertTrue(r.ok)
            self.assertEqual('ffffffff', r.json()['result'][0])

            # Reset the keyboard and try again, with nofailonerror set, which simulates
            # the action coming from a keymap. The action should succeed, but they keyboard
            # should lock, and the field should not be modified.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Reset()')
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(insertMode,true)')
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Key(nofailonerror,x)')
            self.assertTrue(r.ok, 'Expected Key(nofailonerror) to succeed')
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/PrintText(string,oia)')
            self.assertTrue(r.ok, 'Expected PrintText()) to succeed')
            self.assertIn('X Overflow', r.json()['result'][-1], 'Expected overflow')

            # Make sure the field has not been modified.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/AsciiField()')
            self.assertTrue(r.ok)
            self.assertEqual('ffffffff', r.json()['result'][0])

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
