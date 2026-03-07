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

import os
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import threading
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
from Common.Test.proxy import ProxyType, proxy_server

@unittest.skipIf(sys.platform.startswith('win'), 'Does not run on Windows')
@unittest.skipIf(sys.platform == 'cygwin', 'This does some very strange things on Cygwin')
class TestPr3287Proxy(cti):

    # pr3287 proxy test, invalid characters in the command-line proxy specification
    def test_pr3287_invalid_proxy_cmdline(self):

        # Start pr3287.
        pr3287 = Popen(vgwrap(['pr3287', '-proxy', 'abc def', '127.0.0.1:99']), stderr=PIPE)
        self.children.append(pr3287)

        # Wait for the process to exit.
        self.vgwait(pr3287, assertOnFailure=False)

        # Check output
        output = pr3287.stderr.readlines()
        pr3287.stderr.close()
        self.assertEqual('pr3287: Proxy specification contains invalid characters', output[0].decode().strip())

    # Asynchronous send.
    def async_send(self, p: playback):
        p.send_to_mark(1, send_tm=False)

    # pr3287 generic proxy test
    def pr3287_proxy(self, type: ProxyType, fail=False):

        # Start 'playback'.
        port, ts = unused_port()
        with playback(self, 'pr3287/Test/smoke.trc', port=port) as p:
            ts.close()

            # Start the proxy server.
            xport, ts = unused_port()
            ps = proxy_server(self, port, xport, type, fail)
            ps.run()
            self.check_listen(xport)
            ts.close()

            # Start pr3287.
            username = 'fred@' if type == ProxyType.socks4 or type == ProxyType.socks4a else ''
            (po_handle, po_name) = tempfile.mkstemp()
            (sy_handle, sy_name) = tempfile.mkstemp()
            pr3287 = Popen(vgwrap(['pr3287',
                                   '-command', f"cat >'{po_name}'; date >'{sy_name}'",
                                   '-proxy', f'{type.name}:{username}127.0.0.1:{xport}',
                                   f'127.0.0.1:{port}']), stderr=DEVNULL)
            self.children.append(pr3287)

            # Send output, asynchronously.
            if not fail:
                sr = threading.Thread(target=self.async_send, args=[p])
                sr.start()

                # Wait for the sync file to appear.
                self.try_until((lambda: (os.lseek(sy_handle, 0, os.SEEK_END) > 0)), 2, 'pr3287 did not produce output')

            os.close(sy_handle)
            os.unlink(sy_name)

        # Wait for the processes to exit.
        pr3287.kill()
        self.children.remove(pr3287)
        self.vgwait(pr3287, assertOnFailure=False)
        ps.close()

        # Read back the file.
        if not fail:
            os.lseek(po_handle, 0, os.SEEK_SET)
            new_printout = os.read(po_handle, 65536)
            os.close(po_handle)
            os.unlink(po_name)

            # Compare.
            with open('pr3287/Test/smoke.out', 'rb') as file:
                ref_printout = file.read()

            self.assertEqual(new_printout, ref_printout)

    def test_pr3287_passthru_proxy(self):
        self.pr3287_proxy(ProxyType.passthru)

    def test_pr3287_telnet_proxy(self):
        self.pr3287_proxy(ProxyType.telnet)

    def test_pr3287_http_proxy(self):
        self.pr3287_proxy(ProxyType.http)

    def test_pr3287_http_proxy_fail(self):
        self.pr3287_proxy(ProxyType.http, fail=True)

    def test_pr3287_socks4_proxy(self):
        self.pr3287_proxy(ProxyType.socks4)

    def test_pr3287_socks4_proxy_fail(self):
        self.pr3287_proxy(ProxyType.socks4, fail=True)

    def test_pr3287_socks4a_proxy(self):
        self.pr3287_proxy(ProxyType.socks4a)

    def pr3287_socks5_proxy(self, d=False, force_fail=False):

        # Start 'playback' to read pr3287's output.
        port, ts = unused_port()
        with playback(self, 'pr3287/Test/smoke.trc', port=port) as p:
            ts.close()

            # Start pr3287.
            proxy = 'socks5d' if d else 'socks5'
            (po_handle, po_name) = tempfile.mkstemp()
            (sy_handle, sy_name) = tempfile.mkstemp()
            pr3287 = Popen(vgwrap(['pr3287',
                                   '-command', f"cat >'{po_name}'; date >'{sy_name}'",
                                   '-proxy', f'{proxy}:127.0.0.1:{os.environ["SOCKS5"]}',
                                   f'127.0.0.1:{port}']), stderr=DEVNULL)
            self.children.append(pr3287)

            # Connect via proxy.
            # Send output, asynchronously.
            if not force_fail:
                sr = threading.Thread(target=self.async_send, args=[p])
                sr.start()

                # Wait for the sync file to appear.
                self.try_until((lambda: (os.lseek(sy_handle, 0, os.SEEK_END) > 0)), 2, 'pr3287 did not produce output')

            os.close(sy_handle)
            os.unlink(sy_name)

            # Wait for the processes to exit.
            pr3287.kill()
            self.children.remove(pr3287)
            self.vgwait(pr3287, assertOnFailure=False)

            # Read back the file.
            if not force_fail:
                os.lseek(po_handle, 0, os.SEEK_SET)
                new_printout = os.read(po_handle, 65536)
                os.close(po_handle)
                os.unlink(po_name)

                # Compare.
                with open('pr3287/Test/smoke.out', 'rb') as file:
                    ref_printout = file.read()

                self.assertEqual(new_printout, ref_printout)

        # Wait for the process to exit.
        self.vgwait(pr3287, assertOnFailure=False)

    socks5_instructions = 'Need to set up a local SOCKS5 proxy (e.g., ssh -N -D 127.0.0.1:1080 localhost) and put its listening port in SOCKS5 in the environment'

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_pr3287_socks5_proxy(self):
        self.pr3287_socks5_proxy(d=False)

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_pr3287_socks5d_proxy(self):
        self.pr3287_socks5_proxy(d=True)

    @unittest.skipUnless('SOCKS5' in os.environ, socks5_instructions)
    def test_pr3287_socks5d_proxy_fail(self):
        self.pr3287_socks5_proxy(d=True, force_fail=True)

if __name__ == '__main__':
    unittest.main()
