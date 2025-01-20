#!/usr/bin/env python3
#
# Copyright (c) 2025 Paul Mattes.
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
# b3270 protocol error tests

import json
from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

class TestB3270ProtoErrors(cti):

    # b3270 invalid SBA test
    def invalid_address(self, order: str):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, f'b3270/Test/invalid_{order}.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json', '-clear', 'contentionResolution']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send an invalid screen to b3270 and make sure it complains about it.
            p.match()

            # Check for a pop-up about the bad address.
            while True:
                out = pq.get(2, 'b3270 did not produce pop-up')
                if b'popup' in out:
                    break
            outj = json.loads(out.decode('utf8'))['popup']
            self.assertEqual('error', outj['type'])
            self.assertIn(f'{order.upper()} address', outj['text'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # b3270 invalid SBA test
    def test_b3270_invalid_sba(self):
        self.invalid_address('sba')

    # b3270 invalid RA test
    def test_b3270_invalid_ra(self):
        self.invalid_address('ra')

    # b3270 invalid EUA test
    def test_b3270_invalid_eua(self):
        self.invalid_address('eua')
    
    # b3270 ignore EOR test
    def test_b3270_ignore_eor(self):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, 'b3270/Test/ignore_eor.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json', '-clear', 'contentionResolution']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send junk to b3270, including data without being in 3270 mode.
            p.match()

            # Check for a pop-up about ignoring EOR.
            while True:
                out = pq.get(2, 'b3270 did not produce pop-up')
                if b'popup' in out:
                    break
            outj = json.loads(out.decode('utf8'))['popup']
            self.assertEqual('error', outj['type'])
            self.assertIn('EOR received when not in 3270 mode', outj['text'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()
    
    # b3270 invalid command test
    def test_b3270_invalid_command(self):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, 'b3270/Test/invalid_command.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json', '-clear', 'contentionResolution']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send a bad command to b3270.
            p.match(nrecords=2,disconnect=False)

            # Check for a pop-up about the bad command.
            while True:
                out = pq.get(2, 'b3270 did not produce pop-up')
                if b'popup' in out:
                    break
            outj = json.loads(out.decode('utf8'))['popup']
            self.assertEqual('error', outj['type'])
            self.assertIn('Unknown 3270 Data Stream command', outj['text'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()
    
    # b3270 short write test
    def too_short(self, trc: str):

        # Start 'playback' to talk to b3270.
        playback_port, ts = unused_port()
        with playback(self, f'b3270/Test/{trc}.trc', port=playback_port) as p:
            ts.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270', '-json', '-clear', 'contentionResolution']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)

            # Throw away b3270's initialization output.
            pq = pipeq.pipeq(self, b3270.stdout)
            pq.get(2, 'b3270 did not start')

            # Tell b3270 to connect.
            b3270.stdin.write(f'"open 127.0.0.1:{playback_port}"\n'.encode('utf8'))
            b3270.stdin.flush()

            # Wait for b3270 to connect.
            p.wait_accept()

            # Send a bad command to b3270.
            p.match()

            # Check for a pop-up about the bad command.
            while True:
                out = pq.get(2, 'b3270 did not produce pop-up')
                if b'popup' in out:
                    break
            outj = json.loads(out.decode('utf8'))['popup']
            self.assertEqual('error', outj['type'])
            self.assertIn('Record too short', outj['text'])

        # Clean up.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()
    
    # Test a write with no flags field.
    def test_b3270_short_write(self):
        self.too_short('no_flags')
    
    # Test a write with a truncated SF.
    def test_b3270_short_sf(self):
        self.too_short('short_sf')
    
    # Test a write with a truncated SBA.
    def test_b3270_short_sba(self):
        self.too_short('short_sba')
    
    # Test a write with a truncated RA (no address).
    def test_b3270_short_ra_addr(self):
        self.too_short('short_ra_addr')
    
    # Test a write with a truncated RA (no character).
    def test_b3270_short_ra_char(self):
        self.too_short('short_ra_char')
    
    # Test a write with a truncated RA (no GE character).
    def test_b3270_short_ra_ge(self):
        self.too_short('short_ra_ge')
    
    # Test a write with a truncated EUA.
    def test_b3270_short_eua(self):
        self.too_short('short_eua')
    
    # Test a write with a truncated GE.
    def test_b3270_short_ge(self):
        self.too_short('short_ge')
    
    # Test a write with a truncated MF (count).
    def test_b3270_short_mf_count(self):
        self.too_short('short_mf_count')
    
    # Test a write with a truncated MF (attribute).
    def test_b3270_short_mf_attr(self):
        self.too_short('short_mf_attr')
    
    # Test a write with a truncated SA.
    def test_b3270_short_sa(self):
        self.too_short('short_sa')
    
    # Test a write with a truncated SFE (count).
    def test_b3270_short_sfe_count(self):
        self.too_short('short_sfe_count')
    
    # Test a write with a truncated SFE (attribute).
    def test_b3270_short_sfe_attr(self):
        self.too_short('short_sfe_attr')

if __name__ == '__main__':
    unittest.main()
