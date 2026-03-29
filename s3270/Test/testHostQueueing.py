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
# s3270 host queueing tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270HostQueueing(cti):

    # s3270 host queueing test
    def test_s3270_host_queueing(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
            ts.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-utf8', '-httpd', str(hport), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL, stderr=PIPE)
            self.children.append(s3270)
            socket.close()

            # Paint the screen.
            p.send_records(4)

            # Set character reply mode.
            # tn3270e 3270-data
            #  wsf.set-reply-mode 00 character charset
            #  telnet.eor
            p.send_literal('000001000211000609000243ffef')

            # Blast until it hangs.
            # tn3270e 3270e-data
            #  cmd.rb
            #  telnet eor
            # 000001000302ffef
            t0 = time.monotonic()
            done = False
            while True:
                self.assertLess(time.monotonic() - t0, 20, 'Host output did not block')
                for i in range(300):
                    try:
                        p.send_literal('000001000302ffef')
                    except (BlockingIOError, ConnectionResetError):
                        done = True
                        break
                if done:
                    break
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(ConnectionState)')
                result = r.json()['result'][0]
                if result == 'not-connected':
                    break
            #print(f'Took {time.monotonic()-t0} seconds')

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        stderr = s3270.stderr.readlines()
        self.assertTrue(stderr[0].startswith(b'Connection failed:'))
        self.assertTrue(stderr[1].startswith(b'Socket write:'))
        s3270.stderr.close()

if __name__ == '__main__':
    unittest.main()
