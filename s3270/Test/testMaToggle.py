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
# s3270 multi-address toggle tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

class TestS3270MultiAddressToggle(cti):

    def s3270_prefer_toggles(self, ipv4_cl: bool, ipv6_cl: bool):
        hport, ts = unused_port()
        opt_46 = []
        if ipv4_cl:
            opt_46 += ['-4']
        if ipv6_cl:
            opt_46 += ['-6']
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport)] + opt_46), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(hport)

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4)')
        self.assertEqual(str(ipv4_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Toggle(preferIpv4)')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4)')
        self.assertEqual(str(not ipv4_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4,{str(ipv4_cl)})')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4)')
        self.assertEqual(str(ipv4_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv4,Foo)')
        self.assertFalse(r.ok)

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6)')
        self.assertEqual(str(ipv6_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Toggle(preferIpv6)')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6)')
        self.assertEqual(str(not ipv6_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6,{str(ipv6_cl)})')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6)')
        self.assertEqual(str(ipv6_cl).lower(), r.json()['result'][0])
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Set(preferIpv6,Foo)')
        self.assertFalse(r.ok)

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Test various combinations of command-line options and set commands.
    def test_s3270_prefer_toggles(self):
        for case in [[False, False], [False, True], [True, False], [False, False]]:
            self.s3270_prefer_toggles(case[0], case[1])
        

if __name__ == '__main__':
    unittest.main()
