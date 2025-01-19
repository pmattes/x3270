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
# b3270 retry tests

import json
from subprocess import Popen, PIPE
import threading
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

@requests_timeout
class TestB3270Retry(cti):

    # b3270 retry test
    def test_b3270_retry_5s(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, ts = unused_port()
        ts.close()

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-set', 'retry', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        pq = pipeq.pipeq(self, b3270.stdout)
        pq.get(2, 'b3270 did not start')

        # Tell b3270 to connect.
        b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
        b3270.stdin.flush()

        # Wait for it to try to connect and fail.
        out_all = []
        while True:
            out = pq.get(2, 'b3270 did not fail the connection')
            out_all += [out]
            if b'connection-error' in out:
                break
        outj = json.loads(out.decode('utf8'))['popup']
        self.assertTrue(outj['retrying'])
        self.assertFalse(any(b'run-result' in o for o in out_all), 'Open action should not complete')

        # Start 'playback' to talk to b3270.
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            # Wait for b3270 to connect.
            p.wait_accept(timeout=6)

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # Wait for an input field.
    def wif(self, hport):
        self.wait_result = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Wait(InputField)').json()

    # b3270 reconnect/disconnect interference test
    # Makes sure that even if reconnect mode is set, a Wait() action still fails when the connection is broken.
    def test_b3270_reconnect_interference(self):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            hport, ts = unused_port()
            b3270 = Popen(vgwrap(['b3270', '-set', 'reconnect', '-json', '-httpd', str(hport)]), stdin=PIPE, stdout=PIPE)
            ts.close()
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Asynchronously block for an input field.
            wait_thread = threading.Thread(target=self.wif, args = [hport])
            wait_thread.start()

            # Wait for the Wait() to block.
            def wait_block():
                r = ''.join(self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Tasks)').json()['result'])
                return 'Wait("InputField")' in r
            self.try_until(wait_block, 2, 'Wait() did not block')

            # Close the connection.
            p.close()

        # Wait for the input field thread to complete.
        wait_thread.join(timeout=2)
        self.assertFalse(wait_thread.is_alive(), 'Wait thread did not terminate')

        # Check.
        self.assertIn('Host disconnected', ''.join(self.wait_result['result']))

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # b3270 reconnect test
    def test_b3270_reconnect_5s(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, ts = unused_port()
        ts.close()

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-set', 'reconnect', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        pq = pipeq.pipeq(self, b3270.stdout)
        pq.get(2, 'b3270 did not start')

        # Tell b3270 to connect.
        b3270.stdin.write(f'"open c:a:127.0.0.1:{playback_port}"\n'.encode('utf8'))
        b3270.stdin.flush()

        # Wait for it to try to connect and fail.
        out_all = []
        while True:
            out = pq.get(2, 'b3270 did not fail the connection')
            out_all += [out]
            if b'connection-error' in out:
                break
        outj = json.loads(out.decode('utf8'))['popup']
        self.assertTrue(outj['retrying'])
        self.assertFalse(any(b'run-result' in o for o in out_all), 'Open action should not complete')

        # Start 'playback' to talk to b3270.
        with playback(self, 'c3270/Test/ibmlink2.trc', port=playback_port) as p:

            # Wait for b3270 to connect.
            p.wait_accept(timeout=6)

            # Wait for the Open action to succeed.
            while True:
                out = pq.get(2, 'Open() did not succeed')
                if b'run-result' in out:
                    break
            outj = json.loads(out.decode('utf8'))['run-result']
            self.assertTrue(outj['success'])

            # Disconnect.
            p.close()

            # Wait for reconnection to start.
            while True:
                out = pq.get(2, 'Reconnect did not happen')
                if b'reconnecting' in out:
                    break
            outj = json.loads(out.decode('utf8'))['connection']
            self.assertEqual('reconnecting', outj['state'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # b3270 reconnect without disconnect test
    def test_b3270_reconnect_disconnect(self):

        # Find an unused port, but do not listen on it yet.
        playback_port, ts = unused_port()
        ts.close()

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-set', 'reconnect', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Throw away b3270's initialization output.
        pq = pipeq.pipeq(self, b3270.stdout)
        pq.get(2, 'b3270 did not start')

        # Tell b3270 to connect.
        b3270.stdin.write(f'"open c:a:127.0.0.1:{playback_port}"\n'.encode())
        b3270.stdin.flush()

        # Wait for it to try to connect and fail, twice.
        out_all = []
        resolve_count = 0
        while True:
            out = pq.get(10, 'b3270 did not fail the connection')
            out_all += [out.decode()]
            if b'tcp-pending' in out:
                resolve_count += 1
                if resolve_count > 1:
                    break

        # Make sure that b3270 never entered not-connected state.
        self.assertNotIn('not-connected', out_all, 'Should not enter not-connected state')

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()


if __name__ == '__main__':
    unittest.main()
