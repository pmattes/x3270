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
# b3270 multi-address tests

import json
from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipUnless(hostsSetup, setupHosts.warning)
@requests_timeout
class TestB3270MultiAddress(cti):

    def check_result(self, b3270: Popen, ipv4: bool, ipv6: bool):
        # Make sure both are processed.
        output = b3270.communicate()[0].decode('utf8').split('\n')
        tried = []
        for line in output:
            if line != '':
                j = json.loads(line)
                if 'connect-attempt' in j:
                    tried += [j['connect-attempt']['host-ip']]
        if ipv4:
            self.assertIn('127.0.0.1', tried, 'Did not try IPv4')
        else:
            self.assertNotIn('127.0.0.1', tried, 'Should not try IPv4')
        if ipv6:
            self.assertIn('::1', tried, 'Did not try IPv6')
        else:
            self.assertNotIn('::1', tried, 'Should not try IPv6')

    # b3270 multi-address test
    def b3270_multi_address(self, ipv4=True, ipv6=True):

        # Start b3270.
        args46 = []
        if not ipv4:
            args46 += ['-6']
        if not ipv6:
            args46 += ['-4']
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', str(hport)] + args46), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        ts.close()
        self.check_listen(hport)

        # Drain the first line of output. On Windows, unless this is done, b3270
        # will block on the pipe.
        b3270.stdout.readline()

        # Feed b3270 some actions.
        uport, ts = unused_port()
        ts.close()
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open({setupHosts.test_hostname}:{uport})')
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        self.check_result(b3270, ipv4, ipv6)

        # Wait for the process to exit.
        self.vgwait(b3270)

    def test_b3270_multi_address(self):
        self.b3270_multi_address()
    def test_b3270_multi_address4(self):
        self.b3270_multi_address(ipv6=False)
    def test_b3270_multi_address6(self):
        self.b3270_multi_address(ipv4=False)

    def b3270_ma_switch_test(self, ipv4=True, ipv6=True):
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-json', '-httpd', str(hport)]), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        ts.close()
        self.check_listen(hport)

        # Drain the first line of output. On Windows, unless this is done, b3270
        # will block on the pipe.
        b3270.stdout.readline()

        # Feed b3270 some actions.
        uport, ts = unused_port()
        ts.close()
        if ipv4:
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4,true)')
        if ipv6:
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6,true)')
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open({setupHosts.test_hostname}:{uport})')
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        self.check_result(b3270, ipv4, ipv6)

        # Wait for the process to exit.
        self.vgwait(b3270)

    def test_b3270_ma_switch(self):
        self.b3270_ma_switch_test()
    def test_b3270_ma_switch4(self):
        self.b3270_ma_switch_test(ipv6=False)
    def test_b3270_ma_switch6(self):
        self.b3270_ma_switch_test(ipv4=False)

if __name__ == '__main__':
    unittest.main()
