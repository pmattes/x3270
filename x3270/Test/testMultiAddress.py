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
# x3270 multi-host tests

import os
import re
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipIf(os.system('xset q >/dev/null 2>&1') != 0, "X11 server needed for tests")
@unittest.skipUnless(hostsSetup, setupHosts.warning)
class TestX3270MultiHost(cti):

    # x3270 multi-host test
    def x3270_multi_host(self, ipv4=True, ipv6=True):

        # Start x3270.
        handle, tracefile = tempfile.mkstemp()
        os.close(handle)
        args46 = []
        if not ipv4:
            args46 += ['-6']
        if not ipv6:
            args46 += ['-4']
        hport, ts = unused_port()
        x3270 = Popen(vgwrap(['x3270', '-httpd', str(hport), '-trace', '-tracefile', tracefile,
            '-xrm', 'x3270.traceMonitor: false'] + args46))
        self.children.append(x3270)
        self.check_listen(hport)
        ts.close()

        # Feed x3270 some actions.
        uport, ts = unused_port()
        ts.close()
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open({setupHosts.test_hostname}:{uport})')
        self.assertTrue(self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()').ok, 'Quit failed')

        # Wait for the process to exit.
        self.vgwait(x3270)

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

    def test_x3270_multi_host(self):
        self.x3270_multi_host()
    def test_x3270_multi_host4(self):
        self.x3270_multi_host(ipv6=False)
    def test_x3270_multi_host6(self):
        self.x3270_multi_host(ipv4=False)

if __name__ == '__main__':
    unittest.main()
