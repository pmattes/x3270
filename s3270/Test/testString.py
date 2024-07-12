#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# s3270 String() tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests
import Common.Test.cti as cti
import Common.Test.playback as playback

class TestS3270String(cti.cti):

    # s3270 field overflow (no) margin test.
    # Verifying a bug fix for String() accidentally applying margined paste mode.
    def test_s3270_string_no_margin(self):

        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/wrap.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(sport),
                    f'127.0.0.1:{pport}']), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(2)

            # Fill the first field, almost. Then fill it with one more byte.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(ffffff)')
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String(f)')

            # Make sure the cursor lands in the right spot.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Query(cursor1)')
            self.assertEqual(requests.codes.ok, r.status_code)
            result = r.json()['result'][0]
            _, row, _, column, *_ = result.split()
            self.assertEqual(8, int(row), 'Cursor is on the wrong row')
            self.assertEqual(36, int(column), 'Cursor is on the wrong coluumn')

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 String() \u bugfix test.
    def test_s3270_string_slash_u(self):

        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(sport), f'127.0.0.1:{pport}', '-codepage', '424']),
                            stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(4)

            # Enter a Unicode character above U+00FF.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("\\u0061\\u05E9\\u0062")')

            # Make sure it was interpreted correctly.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/ascii1(21,13,1,3)')
            self.assertEqual(requests.codes.ok, r.status_code)
            result = r.json()['result'][0]
            self.assertEqual("aשb", result, 'Expected text is wrong')

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Second s3270 String() \u bugfix test.
    def test_s3270_string_slash_u_at_end(self):

        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/ibmlink.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(sport), f'127.0.0.1:{pport}', '-codepage', '424']),
                            stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(4)

            # Enter a Unicode character above U+00FF.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("\\u0061\\u05E9")')

            # Make sure it was interpreted correctly.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/ascii1(21,13,1,2)')
            self.assertEqual(requests.codes.ok, r.status_code)
            result = r.json()['result'][0]
            self.assertEqual("aש", result, 'Expected text is wrong')

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # EBCDIC (\e) version of the second s3270 String() bugfix test.
    def test_s3270_string_slash_e_dbcs_at_end(self):

        pport, socket = cti.unused_port()
        with playback.playback(self, 's3270/Test/target.trc', pport) as p:
            socket.close()

            # Start s3270.
            sport, socket = cti.unused_port()
            s3270 = Popen(cti.vgwrap(['s3270', '-httpd', str(sport), f'127.0.0.1:{pport}', '-codepage', '935']),
                            stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            self.check_listen(sport)
            socket.close()

            # Fill in the screen.
            p.send_records(2)

            # Enter a DBCS character in EBCDIC.
            requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("\\e57f0")')

            # Make sure it was interpreted correctly.
            r = requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/ascii1(23,6,1,2)')
            self.assertEqual(requests.codes.ok, r.status_code)
            result = r.json()['result'][0]
            self.assertEqual("务", result, 'Expected text is wrong')

        # Wait for the processes to exit.
        requests.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
