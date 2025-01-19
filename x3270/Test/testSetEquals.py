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
# x3270 -set foo=bar tests

import os
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
import Common.Test.playback as playback

@unittest.skipIf(os.system('xset q >/dev/null 2>&1') != 0, "X11 server needed for tests")
@requests_timeout
class TestX3270SetEquals(cti):

    # x3270 -set model= test
    def test_x3270_set_equals(self):

        # Start x3270.
        hport, hts = unused_port()
        hts.close()
        x3270 = Popen(vgwrap(['x3270', '-set', 'model=3278', '-httpd', str(hport)]))
        self.children.append(x3270)
        self.check_listen(hport)

        r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Query(terminalname)')
        j = r.json()
        self.assertEqual('IBM-3278-4-E', j['result'][0], 'Expecting "IBM-3278-4-E" as the terminal name')

        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/quit')
        self.vgwait(x3270)

if __name__ == '__main__':
    unittest.main()
