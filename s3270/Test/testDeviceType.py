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
# s3270 TN3270E DEVICE-TYPE tests

import os
from subprocess import Popen, PIPE
import tempfile
from typing import List
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

class TestS3270DeviceType(cti):

    def s3270_device_type(self, opt: List[str], device_type: str, prefix=None):
        '''s3270 TN3270E device-type test'''

        # Create a trace file on the fly.
        (handle, file_name) = tempfile.mkstemp()
        os.close(handle)
        with open(file_name, 'w') as f:
            f.write('< 0x0   fffd28\n') # RCVD DO TN3270E
            f.write('> 0x0   fffb28\n') # SENT WILL TN3270E
            f.write('< 0x0   fffa280802fff0\n') # RCVD SB TN3270E SEND DEVICE-TYPE SE
            f.write('> 0x0   fffa280207' + device_type.encode().hex() + 'fff0\n') # SENT SB TN3270E DEVICE-TYPE REQUEST IBM-3278-4-E SE
            f.write('# trigger comparison\n')

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, file_name, port=port) as p:
            ts.close()

            # Start s3270.
            command = ['s3270']
            command += opt
            host = f'127.0.0.1:{port}'
            if prefix != None:
                host = prefix + host
            command.append(host)
            s3270 = Popen(vgwrap(command), stdin=PIPE)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

        # Wait for the process to exit.
        #s3270.stdin.write(b'Quit()\n')
        #s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270, assertOnFailure=False)
        os.unlink(file_name)

    # Default behavior.
    def test_s3270_device_type_basic(self):
        self.s3270_device_type([], 'IBM-3278-4-E')
    # Model 2.
    def test_s3270_device_type_mod2(self):
        self.s3270_device_type(['-model', '2'], 'IBM-3278-2-E')
    # Oversize.
    def test_s3270_device_type_dynamic(self):
        self.s3270_device_type(['-oversize', '100x100'], 'IBM-DYNAMIC')
    # Override terminal name (no change).
    def test_s3270_device_type_tn(self):
        self.s3270_device_type(['-tn', 'foo'], 'IBM-3278-4-E')
    # Turn off extended data stream with a resource.
    def test_s3270_device_type_no_e(self):
        self.s3270_device_type(['-set', 'extendedDataStream=false'], 'IBM-3278-4')
    # Turn off extended data stream with a prefix.
    def test_s3270_device_type_s_prefix(self):
        self.s3270_device_type([], 'IBM-3278-4', prefix='S:')
    # Set wrongTerminalName (report impossible terminal types) -- no change.
    def test_s3270_device_type_wrong_name(self):
        self.s3270_device_type(['-set', 'wrongTerminalName=True'], 'IBM-3278-4-E')

if __name__ == '__main__':
    unittest.main()
