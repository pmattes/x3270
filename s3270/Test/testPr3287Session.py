#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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
# s3270 printer session tests

import os
from subprocess import Popen
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
from Common.Test.proxy import ProxyType, proxy_server
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipIf(sys.platform.startswith('win'), 'Unix-specific test')
@requests_timeout
class TestPr3287Session(cti):

    # pr3287 IPv6 session address test.
    @unittest.skipUnless(hostsSetup, setupHosts.warning)
    def test_s3270_ipv6_pr3287_session(self):

        # Start playback to talk to s3270.
        pport, ts = unused_port(ipv6=True)
        with playback(self, 's3270/Test/ibmlink.trc', port=pport, ipv6=True) as p:
            ts.close()

            # Create an s3270 session file that starts a fake printer session.
            handle, sname = tempfile.mkstemp(suffix='.s3270')
            os.close(handle)
            handle, tname = tempfile.mkstemp()
            os.close(handle)
            with open(sname, 'w') as file:
                file.write(f's3270.hostname: {setupHosts.test_hostname}:{pport}\n')
                file.write('s3270.printerLu: .\n')
                file.write(f's3270.printer.assocCommandLine: echo "%H%" >{tname} && sleep 5\n')

            # Start s3270 with that profile.
            env = os.environ.copy()
            env['PRINTER_DELAY_MS'] = '1'
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-6', sname]), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Accept the connection and fill the screen.
            # This will cause s3270 to start up the printer session.
            p.send_records(4)

            # Make sure the printer session got started.
            self.try_until(lambda: os.path.getsize(tname) > 0, 4, 'Printer session not started')
            with open(tname, 'r') as file:
                contents = file.readlines()
            self.assertIn(f'-6 {setupHosts.test_hostname}', contents[0], 'Expected -6 and test hostname')

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit())')
            self.vgwait(s3270)

        os.unlink(sname)
        os.unlink(tname)

    # Asynchronous send.
    def async_send(self, p: playback):
        p.send_records(4)

    # pr3287 proxy session test.
    def s3270_proxy_pr3287_session(self, use_passthru=False):

        proxy_type = ProxyType.passthru if use_passthru else ProxyType.http

        # Start playback to talk to s3270.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=pport) as p:
            ts.close()

            # Start a proxy server.
            if use_passthru:
                proxy_port = 3514
            else:
                proxy_port, ts = unused_port()
            ps = proxy_server(self, pport, proxy_port, type=proxy_type)
            ps.run()
            self.check_listen(proxy_port)
            if not use_passthru:
                ts.close()

            # Create a proxy s3270 session file that starts a fake printer session.
            handle, sname = tempfile.mkstemp(suffix='.s3270')
            os.close(handle)
            handle, tname = tempfile.mkstemp()
            os.close(handle)
            with open(sname, 'w') as file:
                prefix = 'P:' if use_passthru else ''
                file.write(f's3270.hostname: {prefix}127.0.0.1:{pport}\n')
                file.write('s3270.printerLu: .\n')
                file.write(f's3270.printer.assocCommandLine: echo "%P% %H%" >{tname} && sleep 5\n')
                if not use_passthru:
                    file.write(f's3270.proxy: {proxy_type.name}:127.0.0.1:{proxy_port}')

            # Start s3270 with that profile.
            env = os.environ.copy()
            env['PRINTER_DELAY_MS'] = '1'
            if use_passthru:
                env['INTERNET_HOST'] = '127.0.0.1'
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), sname]), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Accept the connection and fill the screen.
            # This will cause s3270 to start up the printer session.
            sr = threading.Thread(target=self.async_send, args=[p])
            sr.start()

            # Make sure the printer session got started.
            self.try_until(lambda: os.path.getsize(tname) > 0, 4, 'Printer session not started')
            with open(tname, 'r') as file:
                contents = file.readlines()
            self.assertEqual(f'-proxy {proxy_type.name}:127.0.0.1:{proxy_port} 127.0.0.1:{pport}\n', contents[0])

            # Make sure the proxy is reported correctly by Query().
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Proxy)')
            self.assertTrue(r.ok)
            result = r.json()['result'][0]
            self.assertEqual(f'{proxy_type.name} 127.0.0.1 {proxy_port}', result)

            # Wait for the process to exit.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit())')
            self.vgwait(s3270)
            sr.join()
            ps.close()

        os.unlink(sname)
        os.unlink(tname)

    # pr3287 HTTP proxy session test.
    def test_s3270_proxy_pr3287_session_http(self):
        self.s3270_proxy_pr3287_session()
    # pr3287 passthru (P:) proxy session test.
    def test_s3270_proxy_pr3287_session_passthru(self):
        self.s3270_proxy_pr3287_session(use_passthru=True)

if __name__ == '__main__':
    unittest.main()
