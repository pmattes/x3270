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
# s3270 scheduler tests

import select
import socket
from subprocess import Popen, DEVNULL
import tempfile
import threading
import time
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Sched(cti):

    def hammer_s3270_scriptport(self, port: int, script1go: bool):
        '''Send a command to s3270, optionally in pieces.'''
        with socket.socket() as s:
            s.connect(('127.0.0.1', port))
            # Send the action.
            if script1go:
                s.send(b'Wait(0.1,seconds)\n')
            else:
                s.send(b'Wait(0.1,seconds)')
                time.sleep(0.1)
                s.send(b'\n')
            # Wait for a response.
            try:
                s.shutdown(socket.SHUT_WR)
            except:
                pass
            r, _, _ = select.select([s], [], [], 10)
            self.assertIn(s, r)

    def hammer_s3270_http(self, port: int):
        '''Send an HTTP command to s3270.'''
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Wait(0.1,seconds)')
        self.assertTrue(r.ok)

    # s3270 scheduler test
    def s3270_scheduler_test(self, nthreads=256, http=False, scriptport=False, script1go=False,):

        if not http and not scriptport:
            return

        # Start s3270.
        if http:
            hport, htp = unused_port()
        if scriptport:
            sport, stp = unused_port()
        command = ['s3270']
        if http:
            command += ['-httpd', str(hport)]
        if scriptport:
            command += ['-scriptport', str(sport)]
        s3270 = Popen(vgwrap(command), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        if http:
            self.check_listen(hport)
            htp.close()
        if scriptport:
            self.check_listen(sport)
            stp.close()

        # Set up threads to send requests to it in parallel.
        threads = []
        for i in range(nthreads):
            if http:
                t = threading.Thread(target=self.hammer_s3270_http, args=[hport])
                threads.append(t)

            if scriptport:
                t = threading.Thread(target=self.hammer_s3270_scriptport, args=[sport, script1go])
                threads.append(t)
        for t in threads:
            t.start()

        # Wait for the threads to exit.
        for t in threads:
            t.join(timeout=10)

        # Wait for the process to exit successfully.
        if http:
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        else:
            with socket.socket() as s:
                s.connect(('127.0.0.1', sport))
                s.send(b'Quit()\n')
        self.vgwait(s3270)

    def test_s3270_scheduler_scriptport(self):
        self.s3270_scheduler_test(http=False, scriptport=True)
    def test_s3270_scheduler_http(self):
        self.s3270_scheduler_test(http=True, scriptport=False)
    def test_s3270_scheduler_both(self):
        self.s3270_scheduler_test(http=True, scriptport=True, nthreads=128, script1go=True)

    def blast_s3270(self, b: threading.Barrier, port: int, query: str, count: int):
        '''Blast s3270 with 10000 queries in one segment'''
        payload = (f'set {query}\n' * count).encode('utf8')
        with socket.create_connection(('127.0.0.1', port)) as sock:
            b.wait()
            sock.sendall(payload)
            time.sleep(7)

    def check_pairs(self, items: list[str], a: str, b: str) -> float:
        '''Check how many pairs in a sequence are a followed by b'''
        total_pairs = len(items) - 1
        ab_pairs = sum(
                1 for i in range(total_pairs)
                if items[i] == a and items[i + 1] == b
                )
        return (ab_pairs / (len(items) / 2)) * 100

    # Test interleaved behavior when there are two sockets pushing huge amounts of requests in one go
    # in parallel.
    def test_s3270_scheduler_interleave(self):
        sport, stp = unused_port()
        handle, tracefile = tempfile.mkstemp()
        os.close(handle)
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(sport), '-trace', '-tracefile', tracefile]),
                      stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        self.check_listen(sport)
        stp.close()

        # Set up threads to send requests to it in parallel.
        threads = []
        barrier = threading.Barrier(2)
        count = 2000 if 'VALGRIND' in os.environ else 10000
        for i in range(2):
            t = threading.Thread(target=self.blast_s3270, args=[barrier, sport, 'unlockDelayMs' if i == 0 else 'oerrLock', count])
            threads.append(t)
        for t in threads:
            t.start()

        # Wait for the threads to exit.
        for t in threads:
            t.join(timeout=10)

        # Wait for the process to exit successfully.
        with socket.socket() as s:
            s.connect(('127.0.0.1', sport))
            s.send(b'Quit()\n')
        self.vgwait(s3270)

        # Now check out the trace.
        with open(tracefile, 'r') as f:
            lines = f.read().splitlines()
        filtered = [line for line in lines if 'Output for Macro' in line]
        result = [line.split()[-1].replace("'", '') for line in filtered]
        self.assertEqual(2 * count, len(filtered))
        percent_runs = self.check_pairs(result, '350', 'true')
        self.assertGreater(percent_runs, 60)
        os.unlink(tracefile)

if __name__ == '__main__':
    unittest.main()
