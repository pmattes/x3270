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
# s3270 TLS tests

import os
from subprocess import Popen, PIPE, DEVNULL
import sys
import threading
import unittest

from Common.Test.cti import *
import Common.Test.setupCert as setupCert
import Common.Test.tls_server as tls_server

@unittest.skipUnless(setupCert.present(), setupCert.warning)
@requests_timeout
class TestS3270Tls(cti):

    # s3270 TLS smoke test
    def s3270_tls_smoke(self, uri=False):

        # Start a server to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'l:a:c:t:127.0.0.1:{port}=TEST' if not uri else f'telnets://127.0.0.1:{port}?accepthostname=TEST?waitoutput=false?')
            s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Do the TLS thing.
            server.wrap()

            # Feed s3270 some actions.
            s3270.stdin.write(b"String(abc)\n")
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.write(b"Disconnect()\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure they are passed through.
            out = server.recv_to_end()
            self.assertEqual(b"abc\r\n", out)

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_tls_smoke(self):
        self.s3270_tls_smoke()
    def test_s3270_tls_smoke_uri(self):
        self.s3270_tls_smoke(uri=True)

    # s3270 STARTTLS test
    def test_s3270_starttls(self):

        # Start a server to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ibmlink.trc', port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270', '-xrm', 's3270.contentionResolution: false']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'127.0.0.1:{port}=TEST')
            s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Make sure it all works.
            server.starttls()
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()
            server.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 TLS minimum version test
    @unittest.skipUnless(sys.platform == 'linux', 'Linux-only test') # Linux-only for now.
    def test_s3270_tls_min(self):

        # Start a server to read s3270's output.
        server_port, server_ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, server_port) as server:
            server_ts.close()

            # Start s3270, requiring TLS 1.3.
            http_port, http_ts = unused_port()
            args = ['s3270', '-httpd', f':{http_port}', '-tlsminprotocol', 'tls1.3' ]
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args += [ f'l:a:c:t:127.0.0.1:{server_port}=TEST' ]
            s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
            self.children.append(s3270)
            self.check_listen(http_port)
            http_ts.close()

            # Disallow TLS 1.3 on the server.
            server.limit_tls13()

            # Accept the connection, which should fail, and should cause s3270 to exit.
            got_exception = False
            try:
                server.wrap()
            except Exception as e:
                # print(f'Exception: {e}')
                got_exception = True

            self.assertTrue(got_exception, 'Expected exception when accepting connection')

        # Wait for the process to exit.
        self.vgwait(s3270, assertOnFailure=False)

    # s3270 TLS maximum version test
    @unittest.skipUnless(sys.platform == 'linux', 'Linux-only test') # Linux-only for now.
    def test_s3270_tls_max(self):

        # Start a server to read s3270's output.
        server_port, server_ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, server_port) as server:
            server_ts.close()

            # Start s3270, requiring TLS 1.2.
            http_port, http_ts = unused_port()
            args = ['s3270', '-httpd', f':{http_port}', '-tlsmaxprotocol', 'tls1.2' ]
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args += [ f'l:a:c:t:127.0.0.1:{server_port}=TEST' ]
            s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
            self.children.append(s3270)
            self.check_listen(http_port)
            http_ts.close()

            # Accept the connection.
            server.wrap()

            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(tlssession)').json()
            self.assertEqual(r['result'][0], 'Version: TLSv1.2', 'Expected TLS 1.2 session')

            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 TLS junk version test
    @unittest.skipUnless(sys.platform == 'linux', 'Linux-only test') # Linux-only for now.
    def test_s3270_tls_junk_protocol(self):

        # Start s3270 with a junk TLS protocol version.
        http_port, http_ts = unused_port()
        args = ['s3270', '-httpd', f':{http_port}', '-tlsmaxprotocol', 'fred' ]
        if sys.platform != 'darwin' and not sys.platform.startswith('win'):
            args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
        s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
        self.children.append(s3270)
        self.check_listen(http_port)
        http_ts.close()

        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Open(l:a:c:t:127.0.0.1:123=TEST)')
        self.assertEqual(r.status_code, 400, 'Expected HTTP 400 failure')
        self.assertTrue(any(['Invalid maximum protocol' in x for x in r.json()['result']]))

        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 TLS contradictory version test
    @unittest.skipUnless(sys.platform == 'linux', 'Linux-only test') # Linux-only for now.
    def test_s3270_tls_contradictory_protocols(self):

        # Start s3270 with contradictory TLS protocol versions.
        http_port, http_ts = unused_port()
        args = ['s3270', '-httpd', f':{http_port}', '-tlsmaxprotocol', 'tls1.2', '-tlsminprotocol', 'tls1.3' ]
        if sys.platform != 'darwin' and not sys.platform.startswith('win'):
            args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
        s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
        self.children.append(s3270)
        self.check_listen(http_port)
        http_ts.close()

        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Open(l:a:c:t:127.0.0.1:123=TEST)')
        self.assertEqual(r.status_code, 400, 'Expected HTTP 400 failure')
        self.assertTrue(any(['Minimum protocol > maximum protocol' in x for x in r.json()['result']]))

        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')

        # Wait for the process to exit.
        self.vgwait(s3270)

    def do_wrap(self, server: tls_server.tls_server):
        '''Do an asynchronous wrap operation on a TLS server'''
        server.wrap()

    # s3270 TLS basic min/max test (all platforms)
    def test_s3270_tls_basic_min_max(self):

        # Start a server to read s3270's output.
        server_port, server_ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, server_port) as server:
            server_ts.close()

            # Start s3270.
            http_port, http_ts = unused_port()
            args = ['s3270', '-httpd', f':{http_port}' ]
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
            self.children.append(s3270)
            self.check_listen(http_port)
            http_ts.close()

            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(tlsminprotocol,tls1.2,tlsmaxprotocol,tls1.2)')
            self.assertEqual(r.status_code, 200, 'Expected HTTP success for Set()')
            x = threading.Thread(target=self.do_wrap, args=[server])
            x.start()
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Open(l:a:c:t:127.0.0.1:{server_port}=TEST)')
            self.assertEqual(r.status_code, 200, 'Expected HTTP success for Open()')

            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(tlssession)').json()
            self.assertTrue('1.2' in r['result'][0], 'Expected TLS 1.2 session')

            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')
            x.join(timeout=2)

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 file transfer crash validation
    def s3270_ft_crash(self, uri=False):

        # Start a server to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ft-crash.trc', port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270', '-set', 'wrongTerminalName', f'l:y:127.0.0.1:{port}=TEST' if not uri else f'tn3270s://127.0.0.1:{port}?accepthostname=TEST?verifyhostcert=false']
            s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Do the TLS thing.
            server.wrap()

            # Feed s3270 some actions.
            s3270.stdin.write(b'Transfer(direction=send,localfile=s3270/Test/short.bin,hostfile=test,mode=binary)\n')
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.write(b"Disconnect()\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure the right thing happens.
            server.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_ft_crash(self):
        self.s3270_ft_crash()
    def test_s3270_ft_crash_uri(self):
        self.s3270_ft_crash(uri=True)

    # s3270 blocking connect test
    def test_s3270_blocking_connect(self):

        # Start a server to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, 's3270/Test/ibmlink.trc', port) as server:
            ts.close()

            # Start s3270.
            args = ['s3270', '-xrm', 's3270.contentionResolution: false', '-utenv']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'l:127.0.0.1:{port}=TEST')

            # Simulate a specific environment where hostname resolution and connect operations are synchronous.
            env = os.environ.copy()
            env['SYNC_RESOLVER'] = '1'
            env['BLOCKING_CONNECT'] = '1'
            s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Make sure it all works.
            server.wrap()
            s3270.stdin.write(b"PF(3)\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()
            server.match()

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 tls992 test
    def test_s3270_tls_992(self):

        # Start a server to read s3270's output.
        port, ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, port) as server:
            ts.close()

            # Start s3270, pointing to port 992 without a TLS tunnel and without STARTTLS support.
            # Remap 992 to the server's port, which happens after the automatic TLS tunnel is chosen.
            args = ['s3270', '-set', 'startTls=false', '-utenv']
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            args.append(f'a:c:t:127.0.0.1:992=TEST')
            env = os.environ.copy()
            env['REMAP992'] = str(port)
            s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Do the TLS thing.
            server.wrap()

            # Feed s3270 some actions.
            s3270.stdin.write(b"String(abc)\n")
            s3270.stdin.write(b"Enter()\n")
            s3270.stdin.write(b"Disconnect()\n")
            s3270.stdin.write(b"Quit()\n")
            s3270.stdin.flush()

            # Make sure they are passed through.
            out = server.recv_to_end()
            self.assertEqual(b"abc\r\n", out)

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 tls992 disable test
    def test_s3270_tls_992_disable(self):

        # Start a server to read s3270's output.
        nc = copyserver()

        # Start s3270, pointing to port 992 without an explicit TLS tunnel, without STARTTLS support and
        # (what's being verified here) without automatic TLS tunnels on port 992.
        # Remap 992 to the server's port, which happens after the automatic TLS tunnel is chosen.
        args = ['s3270', '-set', 'startTls=false', '-set', 'tls992=false', '-utenv']
        args.append(f'a:c:t:127.0.0.1:992')
        env = os.environ.copy()
        env['REMAP992'] = str(nc.port)
        s3270 = Popen(vgwrap(args), stdin=PIPE, stdout=DEVNULL, env=env)
        self.children.append(s3270)

        # Feed s3270 some actions.
        s3270.stdin.write(b"String(abc)\n")
        s3270.stdin.write(b"Enter()\n")
        s3270.stdin.write(b"Disconnect()\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.data()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 TLS security level test
    @unittest.skipIf(sys.platform.startswith('win'), 'security level is POSIX-specific')
    def test_s3270_tls_security_level(self):

        # Start a server to read s3270's output.
        server_port, server_ts = unused_port()
        with tls_server.tls_server('Common/Test/tls/TEST.crt', 'Common/Test/tls/TEST.key', self, None, server_port) as server:
            server_ts.close()

            # Start s3270.
            http_port, http_ts = unused_port()
            args = ['s3270', '-httpd', f':{http_port}' ]
            if sys.platform != 'darwin' and not sys.platform.startswith('win'):
                args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
            s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
            self.children.append(s3270)
            self.check_listen(http_port)
            http_ts.close()

            wanted_level = 1
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(tlsSecurityLevel,{wanted_level})')
            self.assertEqual(r.status_code, 200, 'Expected HTTP success for Set(tlsSecurityLevel)')
            x = threading.Thread(target=self.do_wrap, args=[server])
            x.start()
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Open(l:a:c:t:127.0.0.1:{server_port}=TEST)')
            self.assertEqual(r.status_code, 200, 'Expected HTTP success for Open()')

            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Show(tlsSessionInfo)')
            self.assertEqual(r.status_code, 200, 'Expected HTTP success for Get()')
            level = int([line for line in r.json()['result'] if line.startswith('Security level')][0].split(':')[1])
            self.assertEqual(wanted_level, level)

            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')
            x.join(timeout=2)

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 TLS security level test failure
    @unittest.skipIf(sys.platform.startswith('win'), 'security level is POSIX-specific')
    def test_s3270_tls_security_level_fail(self):

        # Start a server to read s3270's output.
        server_port, server_ts = unused_port()
        server_ts.close()

        # Start s3270.
        http_port, http_ts = unused_port()
        args = ['s3270', '-httpd', f':{http_port}' ]
        if sys.platform != 'darwin' and not sys.platform.startswith('win'):
            args += [ '-cafile', 'Common/Test/tls/myCA.pem' ]
        s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
        self.children.append(s3270)
        self.check_listen(http_port)
        http_ts.close()

        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(tlsSecurityLevel,fred)')
        self.assertEqual(r.status_code, 200, 'Expected HTTP success for Set(tlsSecurityLevel)')
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Open(l:a:c:t:127.0.0.1:{server_port}=TEST)')
        self.assertFalse(r.ok, 'Expected HTTP failure for Open()')
        self.assertTrue(any(['Invalid tlsSecurityLevel' in x for x in r.json()['result']]))

        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force)')

        # Wait for the process to exit.
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
