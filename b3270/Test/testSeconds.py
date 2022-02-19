#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
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
# Validation of the b3270 time indication

import unittest
from subprocess import Popen, PIPE, DEVNULL
import xml.etree.ElementTree as ET
import json
import sys
import Common.Test.ct as ct

class TestB3270Seconds(unittest.TestCase):

    # Set up procedure.
    def setUp(self):
        self.children = []

    # Tear-down procedure.
    def tearDown(self):
        # Tidy up the children.
        for child in self.children:
            child.kill()
            child.wait()

    # b3270 XML seconds test
    def test_b3270_xml_seconds(self):

        b3270 = Popen(ct.vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 an action.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': "Wait(0.1,Seconds)" })
        *first, _, _ = ct.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()

        # Get the result.
        output = b''
        while True:
            line = ct.timed_readline(b3270.stdout, 2, 'b3270 did not produce expected output')
            self.assertNotEqual(b'', line)
            output += line
            if b'run-result' in line:
                break
        output += b'</b3270-out>' # a white lie
        out = ET.fromstring(output.decode('utf8'))

        # Wait for the processes to exit.
        b3270.stdin.write(b'</b3270-in>\n')
        b3270.stdin.close()
        b3270.stdout.close()
        ct.vgwait(b3270)

        # Check.
        a = out.find('./run-result')
        self.assertTrue(a != None)
        att = a.attrib
        self.assertTrue(att['success'])
        self.assertTrue(float(att['time']) >= 0.1)

    # b3270 JSON seconds test
    def test_b3270_json_seconds(self):

        b3270 = Popen(ct.vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 an action.
        j = { "run": { "actions": { "action": "Wait", "args": [ 0.1, "Seconds" ] } } }
        b3270.stdin.write(json.dumps(j).encode('utf8') + b'\n')
        b3270.stdin.flush()

        # Get the result.
        while True:
            line = ct.timed_readline(b3270.stdout, 2, 'b3270 did not produce expected output')
            self.assertNotEqual(b'', line)
            if b'run-result' in line:
                break
        out = json.loads(line.decode('utf8'))

        # Wait for the processes to exit.
        b3270.stdin.close()
        b3270.stdout.close()
        ct.vgwait(b3270)

        # Check.
        run_result = out['run-result']
        self.assertTrue(run_result['success'])
        self.assertTrue(run_result['time'] >= 0.1)

if __name__ == '__main__':
    unittest.main()
