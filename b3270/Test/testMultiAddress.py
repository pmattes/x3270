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
# b3270 multi-host tests

import json
import requests
from subprocess import Popen, PIPE, DEVNULL
import unittest
import Common.Test.cti as cti
import Common.Test.playback as playback
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

class TestB3270MultiHost(cti.cti):

    # b3270 multi-host test
    @unittest.skipUnless(hostsSetup, setupHosts.warning)
    def b3270_multi_host(self, ipv4=True, ipv6=True):

        # Start b3270.
        args46 = []
        if not ipv4:
            args46 += ['-6']
        if not ipv6:
            args46 += ['-4']
        hport, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-json', '-httpd', str(hport)] + args46), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        ts.close()

        # Drain the first line of output. On Windows, this will cause b3270 to block on the pipe.
        b3270.stdout.readline()

        # Feed b3270 some actions.
        uport, ts = cti.unused_port()
        requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open({setupHosts.test_hostname}:{uport})')
        ts.close()
        requests.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

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

        # Wait for the process to exit.
        self.vgwait(b3270)

    def test_b3270_multi_host(self):
        self.b3270_multi_host()
    def test_b3270_multi_host4(self):
        self.b3270_multi_host(ipv6=False)
    def test_b3270_multi_host6(self):
        self.b3270_multi_host(ipv4=False)

if __name__ == '__main__':
    unittest.main()
