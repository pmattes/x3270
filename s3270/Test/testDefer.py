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
# s3270 Set(-defer) tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Defer(cti):

    # s3270 Set(-defer) with model
    def test_s3270_set_defer_simple(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/wrap_field.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Complete the connection.
            p.send_records(1)

            # Try to set the model. Make sure it fails.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model,2)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-4-E'], r.json()['result'][0].split())

            # Set the model with -defer. Make sure it takes.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model,2)')
            self.assertTrue(r.ok)
            #  A simple query should show the old value.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-4-E'], r.json()['result'][0].split())
            #  A -defer query should show the new value.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-2'], r.json()['result'][0].split())
            #  'model' should be the only value reported by a -defer query.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer)')
            self.assertTrue(r.ok)
            self.assertEqual(1, len(r.json()['result']))

            # Set empty oversize with -defer. (bug fix validation)
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,oversize,)')
            self.assertTrue(r.ok)
            #  A -defer query should show nothing, since nothing changed.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,oversize)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'])
            #  Nothing should be reported by a -defer query.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer)')
            self.assertTrue(r.ok)
            self.assertEqual(1, len(r.json()['result']))

            # Disconnect. Make sure it takes now.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Disconnect()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-2-E'], r.json()['result'][0].split())
            #  A -defer query should show nothing.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'])
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'])

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 Set(-defer) with multiple attributes
    def test_s3270_set_defer_multi(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/wrap_field.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Complete the connection.
            p.send_records(1)

            # Try to set the model, oversize and accept hostname. Make sure it fails.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model,2,oversize,100x100,accepthostname,fred)')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-4-E'], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(oversize)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(accepthostname)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'][0].split())


            # Set them with -defer. Make sure it takes.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model,2,oversize,100x100,accepthostname,fred)')
            self.assertTrue(r.ok)
            #  A simple query should show the old values.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-4-E'], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(oversize)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(accepthostname)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'][0].split())
            #  A -defer query should show the new value.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-2'], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,oversize)')
            self.assertTrue(r.ok)
            self.assertEqual(['100x100'], r.json()['result'][0].split())
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,accepthostname)')
            self.assertTrue(r.ok)
            self.assertEqual(['fred'], r.json()['result'][0].split())
            #  These three should be the only values reported by a -defer query.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer)')
            self.assertTrue(r.ok)
            self.assertEqual(3, len(r.json()['result']))

            # Disconnect. Make sure it takes now.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Disconnect()')
            self.assertTrue(r.ok)
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(model)')
            self.assertTrue(r.ok)
            self.assertEqual(['3279-2-E'], r.json()['result'][0].split())
            #  A -defer query should show nothing.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer,model)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'])
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(-defer)')
            self.assertTrue(r.ok)
            self.assertEqual([], r.json()['result'])

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
if __name__ == '__main__':
    unittest.main()
