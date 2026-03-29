#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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
# s3270 output queue tests

from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

@requests_timeout
class TestS3270OutputQueue(cti):

    # s3270 basic callback test
    def test_s3270_callback_basic(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-callback', str(cbport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        cbsocket, _ = listensocket.accept()
        self.check_listen(hport)
        listensocket.close()

        # Send an action and read the response.
        cbsocket.send(b'Query(Formatted)\n')
        res = b''
        while True:
            r = cbsocket.recv(1)
            res += r
            if r == b'\n':
                break

        self.assertEqual('data: unformatted', res.decode('utf8').strip())

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        cbsocket.close()

    # s3270 callback output queue backup test
    def test_s3270_callback_output_queue(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-callback', str(cbport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        cbsocket, _ = listensocket.accept()
        self.check_listen(hport)
        listensocket.close()

        # Feed s3270 actions until the output backs up in the callback socket.
        t0 = time.monotonic()
        while True:
            for i in range(10):
                cbsocket.send(f'Query(-all) ignore({i})\n'.encode('utf8'))
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
            self.assertTrue(r.ok)
            fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
            queued = int(fields[2])
            total = int(fields[6])
            if queued != 0:
                break
            self.assertLess(time.monotonic() - t0, 5, 'Output queue did not back up')

        # Read the socket in chunks.
        # Wait for the output queue to clear.
        total_chunk = total // 3
        queued_chunk = queued // 2
        this_chunk = queued_chunk
        t0 = time.monotonic()
        while True:
            cbsocket.recv(this_chunk)
            this_chunk = total_chunk
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
            self.assertTrue(r.ok)
            fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
            queued = int(fields[2])
            if queued == 0:
                break
            self.assertLess(time.monotonic() - t0, 5, 'Output queue did not clear')
            time.sleep(0.5)

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        cbsocket.close()

    # s3270 callback output queue backup crash test
    def test_s3270_callback_output_queue_crash(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', '-callback', str(cbport)]), stdin=DEVNULL, stdout=DEVNULL, stderr=PIPE)
        self.children.append(s3270)
        cbsocket, _ = listensocket.accept()
        listensocket.close()

        # Feed s3270 actions until the output backs up in the callback socket.
        t0 = time.monotonic()
        while True:
            try:
                cbsocket.send(b'Query(-all)\n')
            except ConnectionResetError:
                break
            self.assertLess(time.monotonic() - t0, 10, 's3270 did not crash')

        # Wait for the processes to exit.
        cbsocket.close()
        self.vgwait(s3270, assertOnFailure=False)
        lines = s3270.stderr.readlines()
        s3270.stderr.close()
        self.assertIn(b'Unread output exceeded', lines[0])

    # s3270 callback output queue backup hang test
    def test_s3270_callback_output_queue_hang(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', '-callback', str(cbport), '-set', 'outputQueues=block']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        cbsocket, _ = listensocket.accept()
        listensocket.close()

        # Feed s3270 actions until the output backs up in the callback socket and hangs s3270.
        # We can tell, because it stops draining input on it.
        t0 = time.monotonic()
        while True:
            _, wfds, _ = select.select([], [cbsocket], [], 2)
            if wfds == []:
                break
            cbsocket.send(b'Query(-all)\n')
            self.assertLess(time.monotonic() - t0, 10, 'Socket did not back up')

        # Wait for the processes to exit.
        cbsocket.close()
        s3270.kill()
        self.vgwait(s3270, assertOnFailure=False)
        self.children.remove(s3270)

if __name__ == '__main__':
    unittest.main()
