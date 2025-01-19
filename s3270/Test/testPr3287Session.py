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
# s3270 printer session tests

import json
import os
from subprocess import Popen
import sys
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.setupHosts as setupHosts

hostsSetup = setupHosts.present()

@unittest.skipIf(sys.platform.startswith('win'), 'Unix-specific test')
@unittest.skipUnless(hostsSetup, setupHosts.warning)
@requests_timeout
class TestPr3287Session(cti):

    # pr3287 IPv6 session address test.
    def test_ipv6_pr3287_session(self):

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

if __name__ == '__main__':
    unittest.main()
