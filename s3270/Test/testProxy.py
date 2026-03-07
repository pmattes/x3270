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
# s3270 proxy tests

from enum import IntEnum, auto
import os
from subprocess import Popen, PIPE, DEVNULL
import threading
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
from Common.Test.proxy import ProxyType, proxy_server

@requests_timeout
class TestS3270Proxy(cti):

    # s3270 passthru proxy test, bad passthru host name (via the environment)
    def test_s3270_passthru_bad_name(self):

        # Start s3270.
        port, ts = unused_port()
        env = os.environ.copy()
        env['INTERNET_HOST'] = 'abc\x01'
        s3270 = Popen(vgwrap(["s3270", "-httpd", f'127.0.0.1:{port}']), env=env)
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Connect through the proxy.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Open(P:def)')
        self.assertFalse(r.ok)
        result = r.json()['result']
        self.assertEqual('Invalid passthru host', result[1])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 proxy test, invalid characters in the command-line proxy specification
    def test_s3270_invalid_proxy_cmdline(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(["s3270", "-httpd", f'127.0.0.1:{port}', '-proxy', 'abc def']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Connect through the proxy.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Open(def)')
        self.assertFalse(r.ok)
        result = r.json()['result']
        self.assertEqual('Proxy specification contains invalid characters', result[1])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 proxy test, invalid characters in the Set() proxy specification
    def test_s3270_invalid_proxy_set(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(["s3270", "-httpd", f'127.0.0.1:{port}']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Specify a bad proxy.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(proxy,"abc def")')
        self.assertFalse(r.ok)
        result = r.json()['result']
        self.assertEqual('Proxy specification contains invalid characters', result[0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Asynchronous send.
    def async_send(self, p: playback):
        p.send_records(5)

    # s3270 passthru test
    def test_s3270_pcolon_passthru(self):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
            ts.close()

            # Start the passthru server.
            ps = proxy_server(self, port, 3514)
            ps.run()
            self.check_listen(3514)

            # Start s3270.
            env = os.environ.copy()
            env['INTERNET_HOST'] = '127.0.0.1'
            hport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{hport}']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Paint the screen, asynchronously.
            sr = threading.Thread(target=self.async_send, args=[p])
            sr.start()

            # Connect via passthru.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(P:127.0.0.1:{port})')
            self.assertTrue(r.ok)
            sr.join()
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,2,1,8)')
            self.assertEqual('SVM0201P', r.json()['result'][0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        ps.close()

    # Wait for the Connect to block.
    def wait_block(self, port: int, delay: int):
        def test():
            j = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
            return any('CONNECT_WAIT' in line for line in j['result'])
        self.try_until(test, delay, "emulator did not block")

    # s3270 generic proxy test
    def s3270_proxy(self, type: ProxyType, fail=False, dnsdelay=0, blocking=False, server_type=None, mock_resolver=None):

        # Start 'playback' to read s3270's output.
        playback_port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Start the proxy server.
            s_type = server_type if server_type != None else type
            proxy_port, ts = unused_port()
            ps = proxy_server(self, playback_port, proxy_port, s_type, fail)
            ps.run()
            self.check_listen(proxy_port)
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            if dnsdelay != 0:
                env['DNSDELAY'] = str(dnsdelay)
            if blocking:
                env['SYNC_RESOLVER'] = '1'
            if mock_resolver != None:
                env['MOCK_ASYNC_RESOLVER'] = mock_resolver
            hport, ts = unused_port()
            username = 'fred@' if type == ProxyType.socks4 or type == ProxyType.socks4a else ''
            s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{hport}', '-proxy', f'{type.name}:{username}127.0.0.1:{proxy_port}', '-utenv']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Paint the screen, asynchronously.
            if not fail:
                sr = threading.Thread(target=self.async_send, args=[p])
                sr.start()

            # Wait for the Connect to block, asynchronously. ALlow only half the time of the DNS
            # delay, to prove that the emulator is still responsive while the proxy address is being
            # resolved.
            if dnsdelay != 0:
                wb = threading.Thread(target=self.wait_block, args=[hport, dnsdelay/2])
                wb.start()

            # Connect via proxy.
            t0 = time.time()
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(127.0.0.1:{playback_port})')
            if fail:
                self.assertFalse(r.ok)
                self.assertEqual('Connection failed:', r.json()['result'][0])
            else:
                self.assertTrue(r.ok)
                sr.join()
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,2,1,8)')
                self.assertEqual('SVM0201P', r.json()['result'][0])
            if dnsdelay != 0:
                t1 = time.time()
                self.assertGreater(t1 - t0, dnsdelay, f'Expected Connect to take at least {dnsdelay} seconds')

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
        ps.close()
        if dnsdelay != 0:
            wb.join()

    def test_s3270_passthru_proxy(self):
        self.s3270_proxy(ProxyType.passthru)

    def test_s3270_telnet_proxy(self):
        self.s3270_proxy(ProxyType.telnet)

    def test_s3270_telnet_proxy_delay(self):
        self.s3270_proxy(ProxyType.telnet, dnsdelay=2)

    def test_s3270_http_proxy(self):
        self.s3270_proxy(ProxyType.http)

    def test_s3270_socks4_proxy(self):
        self.s3270_proxy(ProxyType.socks4)

    def test_s3270_socks4_proxy_blocking(self):
        self.s3270_proxy(ProxyType.socks4, blocking=True)

    def test_s3270_socks4a_proxy(self):
        self.s3270_proxy(ProxyType.socks4a)

    def test_s3270_socks4a_proxy_blocking(self):
        self.s3270_proxy(ProxyType.socks4a, blocking=True)

    def test_s3270_socks4_fallback(self):
        self.s3270_proxy(ProxyType.socks4, server_type=ProxyType.socks4a, mock_resolver='succeed-sync=127.0.0.1;fail-sync')
        self.s3270_proxy(ProxyType.socks4, server_type=ProxyType.socks4a, mock_resolver='succeed-sync=127.0.0.1;fail-async')
        self.s3270_proxy(ProxyType.socks4, server_type=ProxyType.socks4a, mock_resolver='succeed-sync=127.0.0.1;succeed-sync=::1')
        self.s3270_proxy(ProxyType.socks4, server_type=ProxyType.socks4a, mock_resolver='succeed-sync=127.0.0.1;succeed-async=::1')

    def test_s3270_http_proxy_fail(self):
        self.s3270_proxy(ProxyType.http, fail=True)

    def test_s3270_socks4a_proxy_fail(self):
        self.s3270_proxy(ProxyType.socks4, fail=True)

    def s3270_socks5_proxy(self, d=False, force_fail=False, blocking=False):

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=port) as p:
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            if blocking:
                env['SYNC_RESOLVER'] = '1'
            hport, ts = unused_port()
            proxy = 'socks5d' if d else 'socks5'
            s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{hport}', '-proxy', f'{proxy}:127.0.0.1:{os.environ["SOCKS5"]}']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)
            ts.close()

            # Paint the screen, asynchronously.
            if not force_fail:
                sr = threading.Thread(target=self.async_send, args=[p])
                sr.start()

            # Connect via proxy.
            junk = 'xxx' if force_fail else ''
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect({junk}127.0.0.1:{port})')
            if force_fail:
                self.assertFalse(r.ok)
                self.assertEqual('Connection failed:', r.json()['result'][0])
            else:
                self.assertTrue(r.ok)
                sr.join()
                r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(1,2,1,8)')
                self.assertEqual('SVM0201P', r.json()['result'][0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    socks5_instructions = 'Need to set up a local SOCKS5 proxy (e.g., ssh -N -D 127.0.0.1:1080 localhost) and put its listening port in SOCKS5 in the environment'

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_s3270_socks5_proxy(self):
        self.s3270_socks5_proxy(d=False)

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_s3270_socks5_proxy_blocking(self):
        self.s3270_socks5_proxy(d=False, blocking=True)

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_s3270_socks5d_proxy(self):
        self.s3270_socks5_proxy(d=True)

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_s3270_socks5d_proxy_fail(self):
        self.s3270_socks5_proxy(d=True, force_fail=True)

if __name__ == '__main__':
    unittest.main()
