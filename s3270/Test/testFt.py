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
# File transfer tests

from subprocess import Popen, PIPE, DEVNULL
import threading
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270ft(cti):

    # s3270 DFT-mode file transfer test
    def test_s3270_ft_dft(self):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, 's3270/Test/ft_dft.trc', port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-set', 'wrongTerminalName', f'127.0.0.1:{port}']), stdin=PIPE,
                    stdout=PIPE)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b'transfer direction=send host=tso localfile=s3270/Test/fttext hostfile=fttext\n')
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

            # Verify what it says.
            stdout = s3270.communicate()[0].decode().split('\n')
            self.assertEqual('data: Transfer complete, 19925 bytes transferred', stdout[0].strip())
            self.assertTrue('bytes/sec in DFT mode' in stdout[1])
            self.assertEqual('ok', stdout[3].strip())

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 CUT-mode file transfer test
    def ft_cut(self, trace_file: str):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, trace_file, port=port) as p:
            socket.close()

            # Start s3270.
            s3270 = Popen(vgwrap(["s3270", "-model", "2", f"127.0.0.1:{port}"]),
                    stdin=PIPE, stdout=PIPE)
            self.children.append(s3270)

            # Feed s3270 some actions.
            s3270.stdin.write(b'transfer direction=send host=vm "localfile=s3270/Test/fttext" "hostfile=ft text a"\n')
            s3270.stdin.write(b"String(logoff)\n")
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.flush()

            # Verify what s3270 does.
            p.match()

            # Verify what it says.
            stdout = s3270.communicate()[0].decode().split('\n')
            self.assertEqual('data: Transfer complete, 19580 bytes transferred', stdout[0].strip())
            self.assertTrue('bytes/sec in CUT mode' in stdout[1])
            self.assertEqual('ok', stdout[3].strip())

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_ft_cut(self):
        '''Test CUT mode with ordinary EW commands from the host.'''
        self.ft_cut('s3270/Test/ft_cut.trc')
    def test_s3270_ft_cut_ewa(self):
        '''Test CUT mode with EWA commands from the host.'''
        self.ft_cut('s3270/Test/ft_cut_ewa.trc')

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
        port, socket = unused_port()
        with playback(self, 's3270/Test/ft-double.trc', port=port) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(["s3270", '-httpd', str(sport), f"127.0.0.1:{port}"]),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Get the connection going.
            p.send_records(1)
            athread = threading.Thread(target=self.send_rest, args=[p])
            athread.start()

            # Try two file transfers in a row.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Source(s3270/Test/ft-double.txt)')
            self.assertTrue(r.ok)

            # Clean up the async thread.
            athread.join()

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def check_block(self, sport: int) -> bool:
        '''Check for a blocking Transfer() action'''
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(task)').json()['result']
        return any(['KBWAIT => Transfer' in line for line in r])

    def cancel_transfer(self, sport: int):
        '''Cancel the file transfer'''
        # Wait for the file transfer to block.
        self.try_until(lambda: (self.check_block(sport)), 2, 'Transfer() not blocking')
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Transfer(cancel)')
        self.assertTrue(r.ok)

    # s3270 file transfer cancel test
    def test_s3270_ft_cancel(self):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, 's3270/Test/ft_cut.trc', port=port) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-model', '2', '-httpd', str(sport), f'127.0.0.1:{port}']),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Get everything going.
            p.send_records(2)

            # Start a thread to cancel the transfer.
            athread = threading.Thread(target=self.cancel_transfer, args=[sport])
            athread.start()

            # Try a transfer.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Transfer(direction=send,host=vm,"localfile=s3270/Test/fttext","hostfile=ft text a")', timeout=2)
            self.assertFalse(r.ok)

            # Verify what it says.
            self.assertEqual(r.json()['result'][0], 'Transfer canceled by user')

            athread.join()
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    def disconnect_transfer(self, sport: int, p: playback):
        '''Disconnect the session'''
        # Wait for the file transfer to block.
        self.try_until(lambda: (self.check_block(sport)), 2, 'Transfer() not blocking')
        p.disconnect()

    # s3270 file transfer disconnect test
    def test_s3270_ft_disconnect(self):

        # Start 'playback' to read s3270's output.
        port, socket = unused_port()
        with playback(self, 's3270/Test/ft_cut.trc', port=port) as p:
            socket.close()

            # Start s3270.
            sport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-model', '2', '-httpd', str(sport), f'127.0.0.1:{port}']),
                    stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()

            # Get everything going.
            p.send_records(2)

            # Start a thread to disconnect the session.
            athread = threading.Thread(target=self.disconnect_transfer, args=[sport, p])
            athread.start()

            # Try a transfer.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Transfer(direction=send,host=vm,"localfile=s3270/Test/fttext","hostfile=ft text a")', timeout=2)
            self.assertFalse(r.ok)

            # Verify what it says.
            self.assertEqual(r.json()['result'][0], 'Host disconnected, transfer canceled')

            athread.join()
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
