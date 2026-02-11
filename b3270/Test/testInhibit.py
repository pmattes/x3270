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
# Tests for ENTER_INHIBIT behavior

from subprocess import Popen, PIPE
import unittest
import xml.etree.ElementTree as ET

from Common.Test.cti import *
import Common.Test.pipeq as pipeq
from Common.Test.playback import playback

@requests_timeout
class TestB3270Inhibit(cti):

    # b3270 ENTER INHIBIT test.
    def test_b3270_enter_inhibit(self):

        # Start 'playback' to read b3270's output.
        pport, psocket = unused_port()
        with playback(self, 's3270/Test/sruvm.trc', port=pport) as p:
            psocket.close()

            # Start b3270.
            b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
            self.children.append(b3270)
            pq = pipeq.pipeq(self, b3270.stdout)

            # Connect to playback.
            top = ET.Element('b3270-in')
            ET.SubElement(top, 'run', { 'actions': f'Open(localhost:{pport})' })
            *first, _, _ = xml_prettify(top).split(b'\n')
            b3270.stdin.write(b'\n'.join(first) + b'\n')
            b3270.stdin.flush()

            # Get it into ENTER_INHIBIT state, which is displayed on the OIA as X WAIT.
            p.send_records(1)

            # Wait for the processes to exit.
            b3270.stdin.write(b'</b3270-in>\n')
            b3270.stdin.flush()
            b3270.stdin.close()
            self.vgwait(b3270)
            pq.close()

            # Get the output.
            sout = []
            while True:
                line = pq.get(timeout=0)
                sout.append(line)
                if line == b'</b3270-out>':
                    break

            out = ET.fromstring((b'\n'.join(sout)).decode('utf8'))
            b3270.stdout.close()

            # We should see the OIA go through three states:
            #  Waiting for a field
            #  Time wait (X WAIT/clock)
            #  Not connected
            x = [oia.attrib['value'] for oia in out.findall('oia') if oia.attrib['field'] == 'lock']
            self.assertSequenceEqual(['field', 'twait', 'not-connected'], x)

if __name__ == '__main__':
    unittest.main()
