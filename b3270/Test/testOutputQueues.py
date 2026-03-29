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
# b3270 output queue tests

from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

@requests_timeout
class TestB3270OutputQueue(cti):

    # b3270 stdout output queue test
    def test_b3270_stdout_output_queue(self):

        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', str(hport)]), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        ts.close()
        self.check_listen(hport)

        # Feed b3270 actions until the output backs up in the stdout pipe.
        # Note that on Windows, we back up with the initialization indications.
        t0 = time.monotonic()
        while True:
            for i in range(100):
                b3270.stdin.write(b'"Query(-all)"\n')
                b3270.stdin.flush()
            time.sleep(0.5)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
            self.assertTrue(r.ok)
            fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
            queued = int(fields[2])
            if queued > 1000000:
                break
            print('queued:', queued)
            self.assertLess(time.monotonic() - t0, 5, 'Output queue did not back up')

        # Read part of stdout in chunks.
        chunk = queued // 3
        b3270.stdout.read(chunk)
        time.sleep(0.5)
        b3270.stdout.read(chunk)
        time.sleep(0.5)

        # Start draining the queue.
        pq = pipeq.pipeq(self, b3270.stdout)

        # Wait for the output queue to clear.
        t0 = time.monotonic()
        while True:
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
            self.assertTrue(r.ok)
            fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
            queued = int(fields[2])
            if queued == 0:
                break
            self.assertLess(time.monotonic() - t0, 5, 'Output queue did not clear')
            time.sleep(0.5)
        
        b3270.stdin.close()
        b3270.stdout.close()
        self.vgwait(b3270)
        pq.close()

    # b3270 callback output queue test
    def test_b3270_callback_output_queue(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', str(hport), '-callback', str(cbport)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        cbsocket, _ = listensocket.accept()
        self.check_listen(hport)
        listensocket.close()

        # Feed b3270 actions until the output backs up in the callback socket.
        t0 = time.monotonic()
        while True:
            for i in range(100):
                cbsocket.send(b'"Query(-all)"\n')
            time.sleep(0.5)
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
        self.vgwait(b3270)
        cbsocket.close()

    # b3270 stdin output backup crash test
    def test_b3270_stdin_backup_crash(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE, stderr=PIPE)
        self.children.append(b3270)

        # Feed b3270 actions until the output backs up completely on stdout.
        t0 = time.monotonic()
        while True:
            try:
                b3270.stdin.write(b'"Query(-all)"\n')
            except BrokenPipeError:
                break
            self.assertLess(time.monotonic() - t0, 5, 'b3270 did not crash')

        # Wait for the processes to exit.
        try:
            b3270.stdin.close()
        except (BrokenPipeError, OSError):
            pass
        b3270.stdout.close()
        self.vgwait(b3270, assertOnFailure=False)
        lines = b3270.stderr.readlines()
        b3270.stderr.close()
        self.assertIn(b'Unread output exceeded', lines[0])

    # b3270 stdin callback backup crash test
    def test_b3270_callack_backup_crash(self):

        # Set up a listener for the callback port.
        cbport, ts = unused_port()
        listensocket = socket.socket()
        listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listensocket.bind(('127.0.0.1', cbport))
        listensocket.listen()
        ts.close()

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-json', '-callback', str(cbport)]), stdin=DEVNULL, stdout=DEVNULL, stderr=PIPE)
        self.children.append(b3270)
        s, _ = listensocket.accept()
        listensocket.close()

        # Feed b3270 actions until the output backs up completely on the callback socket.
        t0 = time.monotonic()
        while True:
            try:
                s.send(b'"Query(-all)"\n')
            except ConnectionResetError:
                break
            self.assertLess(time.monotonic() - t0, 5, 'b3270 did not crash')

        # Wait for the processes to exit.
        s.close()
        self.vgwait(b3270, assertOnFailure=False)
        lines = b3270.stderr.readlines()
        b3270.stderr.close()
        self.assertIn(b'Unread output exceeded', lines[0])

    def b3270_oq(self, spec: str, expect: str, stderr=False):
        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', str(hport), '-set', f'outputQueues={spec}']), stdin=PIPE, stdout=DEVNULL, stderr=PIPE)
        self.children.append(b3270)
        ts.close()
        self.check_listen(hport)

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Show(OutputQueues)')
        self.assertTrue(r.ok)
        self.assertEqual(expect, r.json()['result'][0])
        b3270.stdin.close()
        self.vgwait(b3270)
        se = b3270.stderr.readlines()
        b3270.stderr.close()
        if stderr:
            self.assertIn(b'Cannot parse', se[0])
        else:
            self.assertEqual([], se)

    def test_b3270_oq_1m(self):
        self.b3270_oq('1m', 'queueing enabled limit 976KiB')
    def test_b3270_oq_bad(self):
        self.b3270_oq('1mx', 'queueing enabled limit 10MiB', stderr=True)

if __name__ == '__main__':
    unittest.main()
