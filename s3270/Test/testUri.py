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
# s3270 URI tests

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.tls_server as tls_server

@requests_timeout
class TestS3270Uri(cti):

    # s3270 bad URI test
    def test_s3270_bad_uri(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', 'tn3270://127.0.0.1:9999#frag']), stdin=PIPE, stdout=DEVNULL, stderr=PIPE)
        self.children.append(s3270)

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270, assertOnFailure=False)
        err = s3270.stderr.readline().decode()
        s3270.stderr.close()
        self.assertIn('URI error', err)

    # s3270 URI LU test
    def s3270_uri_lu(self, ipv6=False):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port(ipv6=ipv6)
        with playback(self, 's3270/Test/lu.trc', port=port, ipv6=ipv6) as p:
            ts.close()
            
            # Start s3270.
            loopback = '[::1]' if ipv6 else '127.0.0.1'
            env = os.environ.copy()
            env['USER'] = 'foo'
            env['NO_CODEPAGE'] = '1'
            s3270 = Popen(vgwrap(['s3270', '-utenv', f'tn3270://{loopback}:{port}?lu=foo']), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            s3270.stdin.write(b'String(logoff) Enter()\n')
            s3270.stdin.flush()

            p.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_uri_lu(self):
        self.s3270_uri_lu()
    def test_s3270_uri_lu_ipv6(self):
        self.s3270_uri_lu(ipv6=True)

    # s3270 tn3270s URI default port test.
    def test_s3270_tn3270s_uri_port(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ibmlink.trc', port) as p:
            ts.close()
            
            # Start s3270.
            # What we're trying to prove here is that the default port for tn3270s is 992, and it overrides s3270.port.
            # But we have to map 992 to something else, so we don't need to run the TLS server as root.
            env = os.environ.copy()
            env['REMAP992'] = str(port)
            s3270 = Popen(vgwrap(['s3270', '-set', 'contentionResolution=false', '-set', 'port=123', '-utenv', 'tn3270s://127.0.0.1?verifyhostcert=false']), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Do the TLS thing.
            p.wrap()

            # Get out.
            s3270.stdin.write(b'PF(3)\n')
            s3270.stdin.flush()

            p.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)
    

    # s3270 tn3270 URI default port test.
    def test_s3270_tn3270_uri_port(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port) as p:
            ts.close()
            
            # Start s3270.
            # What we're trying to prove here is that the default port for tn3270 is 23, and it overrides s3270.port.
            # But we have to map 23 to something else, so we don't need to run the TLS server as root.
            env = os.environ.copy()
            env['REMAP23'] = str(port)
            s3270 = Popen(vgwrap(['s3270', '-set', 'contentionResolution=false', '-set', 'port=123', '-utenv', 'tn3270://127.0.0.1']), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Get out.
            s3270.stdin.write(b'PF(3)\n')
            s3270.stdin.flush()

            p.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 telnets URI default port test.
    def test_s3270_telnets_uri_port(self):

        # Start a server to throw data at s3270.
        port, ts = unused_port()
        with tls_server.tls_sendserver('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, port) as s:
            ts.close()

            # Start s3270.
            # What we're trying to prove here is that the default port for telnets is 992, and it overrides s3270.port.
            # But we have to map 992 to something else, so we don't need to run the TLS server as root.
            hport, ts = unused_port()
            env = os.environ.copy()
            env['REMAP992'] = str(port)
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-set', 'port=123', '-utenv', f'telnets://127.0.0.1?verifyhostcert=false']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Set up TLS.
            s.wrap()

            # Send some text and read it back.
            s.send(b'hello')
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(hello,1)')
            self.assertTrue(r.ok)

            # Clean up.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        self.vgwait(s3270)

    # s3270 telnet URI default port test.
    def test_s3270_telnet_uri_port(self):

        # Start a server to throw data at s3270.
        s = sendserver(self)

        # Start s3270.
        # What we're trying to prove here is that the default port for telnet is 23, and it overrides s3270.port.
        # But we have to map 23 to something else, so we don't need to run the TLS server as root.
        hport, ts = unused_port()
        env = os.environ.copy()
        env['REMAP23'] = str(s.port)
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-set', 'port=123', '-utenv', f'telnet://127.0.0.1']), env=env)
        self.children.append(s3270)
        self.check_listen(hport)
        ts.close()

        # Send some text and read it back.
        s.send(b'hello')
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Expect(hello,1)')
        self.assertTrue(r.ok)

        # Clean up.
        s.close()
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
if __name__ == '__main__':
    unittest.main()
