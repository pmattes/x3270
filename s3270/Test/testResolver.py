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
# s3270 hostname resolver tests.

import os
from subprocess import Popen
import threading
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Resolver(cti):

    # Check for a task waiting on a connection.
    def is_blocked(self, port: int):
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Show(Tasks)')
        result = r.json()['result']
        k = [line for line in result if 'CONNECT_WAIT' in line]
        return len(k) > 0

    # Close <n> connections. Do a reset on the second.
    def closer(self, port: int, num_tries: int):
        for i in range(num_tries):
            self.try_until(lambda: self.is_blocked(port), 2, 'No blocked task found')
            reset = '-reset' if i == 1 else ''
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Disconnect({reset})')
            self.assertTrue(r.ok)

    # Check for <count> canceled requests.
    def are_canceled(self, port: int, count: int) -> bool:
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Resolver)')
        self.assertTrue(r.ok)
        result = r.json()['result'][0]
        return f'complete 0 canceled {count} pending 0 min/avg/max -s/-s/-s' == result

    # s3270 asynchronous resolver test.
    def test_s3270_async_resolver(self):

        # Get an unused port to use as a target.
        tport, ts = unused_port()
        ts.close()

        hport, ts = unused_port()
        ts.close()

        # Start s3270 with an artificial delay on DNS lookups.
        env = os.environ.copy()
        dnsdelay = 2
        env['DNSDELAY'] = str(dnsdelay)
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']), env=env)
        self.children.append(s3270)
        self.check_listen(hport)

        # Start a thread to disconnect running sessions.
        num_tries = 3
        close_thread = threading.Thread(target=self.closer, args=[hport, num_tries])
        close_thread.start()

        # Start connections that the closer thread will break.
        for i in range(num_tries):
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Open(127.0.0.1:{tport})')
            self.assertFalse(r.ok)

            # Make sure the new request is listed as pending.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Resolver)')
            result = r.json()['result'][0]
            self.assertEqual(f'complete 0 canceled 0 pending {i+1} min/avg/max -s/-s/-s', result)

        # Wait for those pending requests to turn into canceled requests.
        self.try_until(lambda: self.are_canceled(hport, num_tries), dnsdelay + 1, 'Requests did not cancel')

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 Query(Resolver) test.
    def test_s3270_query_resolver(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            hport, ts = unused_port()
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), f'127.0.0.1:{pport}']),)
            self.children.append(s3270)
            self.check_listen(hport)

            # Fill in the screen, which includes accepting the connection and verifying that s3270
            # processed the data.
            p.send_records(4)

        # There should be one complete request and its min/avg/max times should be equal.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(Resolver)')
        result = r.json()['result'][0].split()
        prefix = ' '.join(result[0:7])
        self.assertEqual(f'complete 1 canceled 0 pending 0 min/avg/max', prefix)
        times = result[-1].split('/')
        self.assertEqual(3, len(times))
        self.assertTrue(times[0] == times[1] and times[0] == times[2])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Asynchronous connection accept.
    def async_accept(self, p: playback):
        p.send_records(4)

    # s3270 mock async resolver test 1.
    def test_s3270_mock_resolver1(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            hport, ts = unused_port()
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            env['MOCK_ASYNC_RESOLVER'] = 'fail-sync;fail-async;succeed-sync=127.0.0.1'
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)

            # Connect three times. The first two should fail.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(127.0.0.1:{pport})')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(127.0.0.1:{pport})')
            self.assertFalse(r.ok)

            # The third should succeed. What's more, it should succeed even though it specifies an IPv6
            # address, because we have specified an IPv4 resolution.
            at = threading.Thread(target=self.async_accept, args=[p])
            at.start()
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect([::1]:{pport})')
            self.assertTrue(r.ok)
            at.join()

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 mock async resolver test 2.
    def test_s3270_mock_resolver2(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            hport, ts = unused_port()
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            env['MOCK_ASYNC_RESOLVER'] = 'succeed-async=127.0.0.1'
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)

            # Connect. It should succeed even though it specifies an IPv6
            # address, because we have specified an IPv4 resolution.
            at = threading.Thread(target=self.async_accept, args=[p])
            at.start()
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect([::1]:{pport})')
            self.assertTrue(r.ok)
            at.join()

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 mock async resolver test 3.
    def test_s3270_mock_resolver3(self):

        pport, socket = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            hport, ts = unused_port()
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            env['MOCK_ASYNC_RESOLVER'] = 'fail-async'
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(hport), '-utenv']), env=env)
            self.children.append(s3270)
            self.check_listen(hport)

            # Connect. The first two attempts should fail, one because the mock spec says to,
            # the second because it is IPv6.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect([::1]:{pport})')
            self.assertFalse(r.ok)
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect([::1]:{pport})')
            self.assertFalse(r.ok)

            # The third should succeed because is it IPv4 and the mock spec has run out.
            at = threading.Thread(target=self.async_accept, args=[p])
            at.start()
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(127.0.0.1:{pport})')
            self.assertTrue(r.ok)
            at.join()

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
