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
# pr3287 multi-host tests

import os
import re
from subprocess import Popen, DEVNULL
import sys
import tempfile
import unittest

from Common.Test.cti import *
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipIf(sys.platform == 'cygwin', 'This does some very strange things on Cygwin')
class TestPr3287MultiHost(cti):

    # pr3287 multi-host test
    @unittest.skipUnless(hostsSetup, setupHosts.warning)
    def pr3287_multi_host(self, ipv4=True, ipv6=True):

        # Start pr3287.
        handle, tracefile = tempfile.mkstemp()
        os.close(handle)
        uport, ts = unused_port()
        ts.close()
        args46 = []
        if not ipv4:
            args46 += ['-6']
        if not ipv6:
            args46 += ['-4']
        pr3287 = Popen(vgwrap(['pr3287', '-trace', '-tracefile', tracefile]
            + args46 + [f'{setupHosts.test_hostname}:{uport}']), stdout=DEVNULL, stderr=DEVNULL)
        self.children.append(pr3287)

        # Wait for the process to exit.
        self.vgwait(pr3287, assertOnFailure=False, timeout=8)

        # Make sure both are processed.
        with open(tracefile, 'r') as file:
            output = file.readlines()
        tried = []
        for line in output:
            m = re.search(r'Trying (.*), port', line)
            if m != None:
                tried += [m.group(1)]
        os.unlink(tracefile)
        if ipv4:
            self.assertIn('127.0.0.1', tried, 'Did not try IPv4')
        else:
            self.assertNotIn('127.0.0.1', tried, 'Should not try IPv4')
        if ipv6:
            self.assertIn('::1', tried, 'Did not try IPv6')
        else:
            self.assertNotIn('::1', tried, 'Should not try IPv6')

    def test_pr3287_multi_host(self):
        self.pr3287_multi_host()
    def test_pr3287_multi_host4(self):
        self.pr3287_multi_host(ipv6=False)
    def test_pr3287_multi_host6(self):
        self.pr3287_multi_host(ipv4=False)

    # pr3287 host sequence test
    def test_pr3287_multi_host_sequence(self):

        # Start a copy server.
        c = copyserver(justAccept=True)

        # Start pr3287.
        handle, tracefile = tempfile.mkstemp()
        os.close(handle)
        uport, ts = unused_port()
        ts.close()
        vport, ts = unused_port()
        ts.close()
        # Set up a mock resolver result with the unused port, the real port and the other unused port.
        env = os.environ.copy()
        env['MOCK_SYNC_RESOLVER'] = f'127.0.0.1/{uport};127.0.0.1/{c.port};127.0.0.1/{vport}'
        pr3287 = Popen(vgwrap(['pr3287', '-utenv', '-trace', '-tracefile', tracefile,
            f'foo:9999']), stdout=DEVNULL, stderr=DEVNULL, env=env)
        self.children.append(pr3287)

        # Wait for the process to exit.
        c.close(timeout=10)
        self.vgwait(pr3287, timeout=10, assertOnFailure=False)

        # Make sure only two addresses are processed.
        with open(tracefile, 'r') as file:
            output = file.readlines()
        tried = []
        for line in output:
            m = re.search(r'Trying (.*), port ([0-9]+)', line)
            if m != None:
                tried += [m.group(1) + ' ' + m.group(2)]
        os.unlink(tracefile)
        self.assertEqual(tried, [f'127.0.0.1 {uport}', f'127.0.0.1 {c.port}'])

if __name__ == '__main__':
    unittest.main()
