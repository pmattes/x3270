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
# s3270 underscored alphabetic tests

from subprocess import Popen, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270AplUnderscore(cti):

    # s3270 APL underscored alphabetics test, Ascii1() action
    def test_s3270_apl_underscore_ascii1(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-utf8', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Check it.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(2,13,9)')
            self.assertEqual('ABCDáãåHI', r.json()['result'][0])
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Ascii1(3,13,9)')
            self.assertEqual('JKLMNOPQR', r.json()['result'][0])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
    # s3270 APL underscored alphabetics test, PrintText(html) action
    def test_s3270_apl_underscore_printtext_html(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-utf8', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Check it.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string,html)')
            want = '</span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:none"> Using SA   </span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:underline">ABCD</span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:none">áãå</span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:underline">HI</span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:none">                                                           '
            self.assertEqual(want, r.json()['result'][6])
            want7 = ' Using SFE  </span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:underline">JKLMNOPQR</span><span style="color:deepSkyBlue;background:black;font-weight:normal;font-style:normal;text-decoration:none">                                                           '
            self.assertEqual(want7, r.json()['result'][7])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 APL underscored alphabetics test, PrintText() action
    def test_s3270_apl_underscore_printtext(self):

        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-utf8', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Check it.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/PrintText(string)')
            self.assertEqual('ABCDáãåHI', r.json()['result'][1][12:21])
            self.assertEqual('JKLMNOPQR', r.json()['result'][2][12:21])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def s3270_apl_underscore_readbuffer(self, mode: str, want1: str, want2: str):
        # Start 'playback' to emulate the host.
        pport, socket = unused_port()
        with playback(self, 's3270/Test/apl_combo.trc', port=pport) as p:
            socket.close()

            # Start s3270.
            hport, socket = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-utf8', '-httpd', str(hport), f'127.0.0.1:{pport}']),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            socket.close()
            self.check_listen(hport)

            # Paint the screen.
            p.send_records(1)

            # Check it.
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/ReadBuffer({mode})')
            self.assertEqual(want1, r.json()['result'][1])
            self.assertEqual(want2, r.json()['result'][2])

        # Wait for s3270 to exit.
        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 APL underscored alphabetics test, ReadBuffer(ascii) action
    def test_s3270_apl_underscore_readbuffer_ascii(self):
        self.s3270_apl_underscore_readbuffer(
            'ascii',
            'SF(c0=e0) 55 73 69 6e 67 20 53 41 20 SF(c0=e1) SA(43=f1) 20 41 42 43 44 SA(43=00) c3a1 c3a3 c3a5 SA(43=f1) 48 49 00 00 00 00 00 00 SF(c0=e0) SA(43=00) 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00',
            'SF(c0=e0) 55 73 69 6e 67 20 53 46 45 SF(c0=e1,43=f1) 00 4a 4b 4c 4d SA(43=f1) 4e 4f 50 SA(43=00) 51 52 00 00 00 00 00 00 SF(c0=e0) 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00')

    # s3270 APL underscored alphabetics test, ReadBuffer(ebcdic) action
    def test_s3270_apl_underscore_readbuffer_ebcdic(self):
        self.s3270_apl_underscore_readbuffer(
            'ebcdic',
            'SF(c0=e0) e4 a2 89 95 87 40 e2 c1 40 SF(c0=e1) SA(43=f1) 40 41 42 43 44 SA(43=00) 45 46 47 SA(43=f1) 48 49 4a 4b 4c 4d 4e 4f SF(c0=e0) SA(43=00) 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00',
            'SF(c0=e0) e4 a2 89 95 87 40 e2 c6 c5 SF(c0=e1,43=f1) 50 51 52 53 54 SA(43=f1) 55 56 57 SA(43=00) 58 59 5a 5b 5c 5d 5e 5f SF(c0=e0) 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00')

    # s3270 APL underscored alphabetics test, ReadBuffer(unicode) action
    def test_s3270_apl_underscore_readbuffer_unicode(self):
        self.s3270_apl_underscore_readbuffer(
            'unicode',
            'SF(c0=e0) 0055 0073 0069 006e 0067 0020 0053 0041 0020 SF(c0=e1) SA(43=f1) 0020 0041 0042 0043 0044 SA(43=00) 00e1 00e3 00e5 SA(43=f1) 0048 0049 0000 0000 0000 0000 0000 0000 SF(c0=e0) SA(43=00) 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000',
            'SF(c0=e0) 0055 0073 0069 006e 0067 0020 0053 0046 0045 SF(c0=e1,43=f1) 0000 004a 004b 004c 004d SA(43=f1) 004e 004f 0050 SA(43=00) 0051 0052 0000 0000 0000 0000 0000 0000 SF(c0=e0) 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000')

if __name__ == '__main__':
    unittest.main()
