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
# s3270 Attn() tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Attn(cti):

    def send_enter(self, port: int):
        '''Send an Enter() to the emulator'''
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Enter()')
    
    def kbwait(self, port: int) -> bool:
        '''Test for KBWAIT'''
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Show(Tasks)')
        result = r.json()['result']
        k = [line for line in result if 'KBWAIT' in line]
        return len(k) > 0

    # s3270 TN3270 Attn() test
    def test_s3270_tn3270_attn(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/sruvm-attn.trc', port=port) as p:
            ts.close()

            # Start s3270.
            http_port, hts = unused_port()
            s3270 = Popen(vgwrap(["s3270", '-httpd', str(http_port), f'127.0.0.1:{port}']), stdin=DEVNULL, stdout=DEVNULL)
            hts.close()
            self.children.append(s3270)

            # Draw the screen (TN3270 mode).
            p.send_records(2)

            # Send it an Enter() asynchronously, which will block.
            attn_thread = threading.Thread(target=self.send_enter, args=[http_port])
            attn_thread.start()

            # Wait for the Enter to block.
            self.try_until(lambda: self.kbwait(http_port), 2, 'Enter() did not block')

            # Send the Attn(), then a Reset() to get the Enter() to unblock.
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Attn()')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Reset()')
            attn_thread.join()

            # Make sure the emulator does what we expect.
            p.match()

        # Wait for the processes to exit.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
