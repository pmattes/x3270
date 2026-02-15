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
# s3270 Query() tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Query(cti):

    # s3270 Query(keyboardlock) test
    def test_s3270_query_keyboard(self, ipv6=False):

        # Start 'playback' to read s3270's output.
        playback_port, ts = unused_port(ipv6=ipv6)
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Start s3270.
            http_port, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}',
                f'127.0.0.1:{playback_port}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            ts.close()
            self.check_listen(http_port)

            # Feed x3270 some data.
            p.send_records(4)

            # Force the keyboard to lock.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Up()')
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Key(a)')

            # Verify that it is locked.
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(KeyboardLock)')
            kb = r.json()['result'][0]
            self.assertEqual('true', kb, 'keyboard should be locked')

            # Unlock it and verify that it is unlocked.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Reset())')
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(KeyboardLock)')
            kb = r.json()['result'][0]
            self.assertEqual('false', kb, 'keyboard should not be locked')

            # Stop s3270.
            self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the processes to exit.
        self.vgwait(s3270)

    # s3270 Query() test for window-specific items.
    def test_s3270_query_window(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        q_window = ['CharacterPixels', 'DisplayPixels', 'WindowLocation', 'WindowPixels', 'WindowState']
        if sys.platform.startswith('win'):
            q_window.append('WindowId')

        # Query everything and make sure it doesn't include the window items.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query()')
        result = r.json()['result']
        for query in q_window:
            matches = [query for qline in result if query in qline]
            self.assertEqual(0, len(matches), f'did not expect {matches}')

        # Query those items specifically to make sure they are merely hidden.
        for query in q_window:
            r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query({query})')
            result = r.json()['result']
            self.assertTrue(r.ok)
            self.assertEqual(1, len(result))

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query() ambiguous query test.
    def test_s3270_query_ambiguous(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query something ambiguous.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(p)')
        result = r.json()['result']
        self.assertEqual("Query(): Ambiguous parameter 'p': Prefixes, Proxies, Proxy", result[0])

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query(Dirs) test.
    @unittest.skipUnless(sys.platform.startswith('win'), 'Windows-specific test')
    def test_s3270_query_dirs(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query Dirs and make sure appdata is the current directory.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Dirs)')
        result = r.json()['result']
        appdata = [line for line in result if 'Appdata:' in line]
        self.assertEqual(1, len(appdata))
        self.assertEqual(os.getcwd().replace('/', '\\') + '\\', appdata[0].split()[1])

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query(SpecialCharacters) test.
    def test_s3270_query_special_characters(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query special characters.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(SpecialCharacters)')
        result = r.json()['result']
        self.assertEqual(['intr ^C quit ^\\ erase ^H kill ^U', 'eof ^D werase ^W rprnt ^R lnext ^V'], result)

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query(-all) test.
    def test_s3270_query_all(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query all.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(-all)')
        self.assertTrue(r.ok)
        result = r.json()['result']
        # Spot checks -- we did get Tls, we didn't get Ssl, we did get Cursor1, we didn't get Cursor.
        self.assertTrue(any([line for line in result if line.startswith('Tls:')]))
        self.assertFalse(any([line for line in result if line.startswith('Ssl:')]))
        self.assertTrue(any([line for line in result if line.startswith('Cursor1:')]))
        self.assertFalse(any([line for line in result if line.startswith('Cursor:')]))
        # On Windows, we should get WindowId and Dirs, which are hidden.
        if sys.platform.startswith('win'):
            self.assertTrue(any([line for line in result if line.startswith('WindowId:')]))
            self.assertTrue(any([line for line in result if line.startswith('Dirs:')]))

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query(Color) test.
    def test_s3270_query_color(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query color.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Color)')
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual('display color emulation color', result[0])

        # Switch to 3278 and try again.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Set(model,3278-2)')
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(Color)')
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual('display color emulation monochrome', result[0])

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

    # s3270 Query(CommandLine) test.
    def test_s3270_query_command_line(self):
        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Query the command line.
        r = self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Query(CommandLine)')
        self.assertTrue(r.ok)
        result = r.json()['result'][0].split()
        self.assertEqual(3, len(result))
        self.assertTrue(result[0].endswith('s3270') or result[0].endswith('s3270.exe'))
        self.assertEqual(['-httpd', f'127.0.0.1:{http_port}'], result[1:])

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Wait for the process to exit.
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
