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
# Tests for ENTER_INHIBIT behavior

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Inhibit(cti):

    # s3270 ENTER INHIBIT test for Show(KeyboardLockDetail) and PrintText(string,oia)
    def test_s3270_enter_inhibit(self):

        # Start 'playback' to read s3270's output.
        pport, psocket = unused_port()
        hport, hsocket = unused_port()
        with playback(self, 's3270/Test/sruvm.trc', port=pport) as p:
            psocket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-httpd', f':{hport}', f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            hsocket.close()

            # Get it into ENTER_INHIBIT state.
            p.send_records(1)

            # Verify the state via Show(KeyboardLockDetail).
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(KeyboardLockDetail)')
            flags = r.json()['result'][0]
            self.assertEqual('enter-inhibit', flags)

            # Verify the state via PrintText(string,oia).
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,oia)')
            state = r.json()['result'][-1][7:][:6]
            self.assertEqual('X Wait', state)

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
    def send_async(self, port: int, text: str):
        '''Send an action to s3270 via HTTP, asynchronously'''
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/{text}')

    # s3270 test for Show(KeyboardLockDetail)
    def test_s3270_keyboard_lock_detail(self):

        # Start 'playback' to read s3270's output.
        pport, psocket = unused_port()
        hport, hsocket = unused_port()
        with playback(self, 's3270/Test/sruvm.trc', port=pport) as p:
            psocket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-httpd', f':{hport}', f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            hsocket.close()

            # Paint the screen.
            p.send_records(2)

            # Verify the state via Show(KeyboardLockDetail).
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(KeyboardLockDetail)')
            flags = r.json()['result'][0]
            self.assertEqual('', flags)

            # Force an operator error.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Up() String(x)')
            self.assertFalse(r.ok)

            # Check the flags.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(KeyboardLockDetail)')
            flags = r.json()['result'][0]
            self.assertEqual('oerr-protected', flags)

            # Clear the error and send an Enter AID, asynchronously.
            x = threading.Thread(target=self.send_async, args=[hport, 'Down() Enter()'])
            x.start()

            # Check the flags.
            self.try_until(lambda: self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(KeyboardLockDetail)').json()['result'][0] == 'oia-twait oia-locked', 2, 'Did not get desired state')
            
            # Reset the keyboard lock from the host.
            p.send_records(1)
            x.join()

            # Verify the state via Show(KeyboardLockDetail).
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(KeyboardLockDetail)')
            flags = r.json()['result'][0]
            self.assertEqual('', flags)

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
