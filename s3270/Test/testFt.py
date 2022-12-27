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
# File transfer tests

import requests
from subprocess import Popen, PIPE, DEVNULL
import threading
import time
import unittest
import Common.Test.playback as playback
import Common.Test.cti as cti

class TestS3270ft(cti.cti):

    # s3270 DFT-mode file transfer test
    def test_s3270_ft_dft(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ft_dft.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(cti.vgwrap(["s3270", f"127.0.0.1:{port}"]), stdin=PIPE,
                    stdout=DEVNULL)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b'transfer direction=send host=tso localfile=s3270/Test/fttext hostfile=fttext\n')
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 CUT-mode file transfer test
    def test_s3270_ft_cut(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ft_cut.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(cti.vgwrap(["s3270", "-model", "2", f"127.0.0.1:{port}"]),
                    stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b'transfer direction=send host=vm "localfile=s3270/Test/fttext" "hostfile=ft text a"\n')
            s3270.stdin.write(b"String(logoff)\n")
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # Send the rest of the file to the emulator, after a brief delay, and absorb broken pipe errors,
    # which can happen if the emulator fails.
    def send_rest(self, p: playback):
        time.sleep(0.5)
        try:
            p.send_to_mark()
        except BrokenPipeError:
            return
        time.sleep(0.5)
        try:
            p.send_to_mark()
        except BrokenPipeError:
            return

    # s3270 file transfer blocking test
    def test_s3270_ft_block(self):

        # Start 'playback' to read s3270's output.
        port, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ft-double.trc', port=port) as p:
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(["s3270", '-httpd', str(sport), f"127.0.0.1:{port}"]),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Get the connection going.
            p.send_records(1)
            athread = threading.Thread(target=self.send_rest, args=[p])
            athread.start()

            # Try two file transfers in a row.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Source(s3270/Test/ft-double.txt)')
            self.assertTrue(r.ok)

            # Clean up the async thread.
            athread.join()

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
