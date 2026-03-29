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
# s3270 standard input tests

from subprocess import Popen, PIPE, DEVNULL
import sys
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.pipeq as pipeq

class TestS3270Stdin(cti):

    # s3270 stdin hang test
    @unittest.skipIf(sys.platform.startswith('win'), "POSIX-only test")
    def test_s3270_stdin_hang(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', f'127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            p.send_records(4)

            # Feed s3270 some actions.
            s3270.stdin.write(b"Foo")
            s3270.stdin.flush()

            # Send a timing mark (and expect one back).
            p.send_tm()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 stdin output backup test
    def test_s3270_stdin_backup(self):

        # Start s3270.
        hport, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-set', 'scriptedAlways']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)
        ts.close()

        # Feed s3270 actions until the output backs up on stdout.
        t0 = time.monotonic()
        while True:
            for i in range(100):
                s3270.stdin.write(b'Query(-all)\n')
            s3270.stdin.flush()
            time.sleep(0.5)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
            self.assertTrue(r.ok)
            fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
            queued = int(fields[2])
            if queued != 0:
                break
            self.assertLess(time.monotonic() - t0, 5, 'Output queue did not back up')

        # Read stdout.
        # Wait for the output queue to clear.
        pq = pipeq.pipeq(self, s3270.stdout)
        t0 = time.monotonic()
        while True:
            try:
                if pq.get() == None:
                    r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(OutputQueues)')
                    self.assertTrue(r.ok)
                    fields = [i for i in r.json()['result'] if i.startswith('total')][0].split()
                    queued = int(fields[2])
                    self.assertEqual(0, queued, 'Output queue did not drain')
                    break
            except:
                break
            self.assertLess(time.monotonic() - t0, 5, 'Output queue took too long to drain')

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)
        pq.close()
        s3270.stdout.close()

    # s3270 stdin output backup crash test
    def test_s3270_stdin_backup_crash(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE, stderr=PIPE)
        self.children.append(s3270)

        # Feed s3270 actions until the output backs up completely on stdout.
        t0 = time.monotonic()
        while True:
            try:
                s3270.stdin.write(b'Query(-all)\n')
            except (BrokenPipeError, OSError):
                break
            self.assertLess(time.monotonic() - t0, 10, 's3270 did not crash')

        # Wait for the processes to exit.
        try:
            s3270.stdin.close()
        except (BrokenPipeError, OSError):
            pass
        s3270.stdout.close()
        self.vgwait(s3270, assertOnFailure=False)
        lines = s3270.stderr.readlines()
        s3270.stderr.close()
        self.assertIn(b'Unread output exceeded', lines[0])

if __name__ == '__main__':
    unittest.main()
