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
# b3270 XML tests

import unittest
from subprocess import Popen, PIPE, DEVNULL
import xml.etree.ElementTree as ET
import TestCommon

class TestB3270Json(unittest.TestCase):

    # b3270 NVT XML smoke test
    def test_b3270_nvt_xml_smoke(self):

        # Start 'nc' to read b3270's output.
        nc = Popen(["python3", "Common/Test/nc1.py", "127.0.0.1", "9991"],
                stdout=PIPE)
        TestCommon.check_listen(9991)

        # Start b3270.
        b3270 = Popen(['b3270'], stdin=PIPE, stdout=DEVNULL)

        # Feed b3270 some actions.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': 'Open(a:c:t:127.0.0.1:9991) String(abc) Enter() Disconnect()' })
        *first, _, _ = TestCommon.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.stdout.read()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        b3270.stdin.close()
        b3270.wait(timeout=2)
        nc.stdout.close()
        nc.wait(timeout=2)

    # b3270 XML single test
    def test_b3270_xml_single(self):

        b3270 = Popen(['b3270'], stdin=PIPE, stdout=PIPE)

        # Feed b3270 an action.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': 'Set(startTls)' })
        *first, _, _ = TestCommon.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')

        # Get the result.
        out = ET.fromstring(b3270.communicate(timeout=2)[0].decode('utf8'))

        # Wait for the process to exit.
        b3270.stdin.close()
        b3270.wait(timeout=2)

        # Check.
        a = out.find('./run-result')
        self.assertTrue(a != None)
        att = a.attrib
        self.assertTrue(att['success'])
        self.assertEqual('true', att['text'])

    # b3270 XML multiple test
    def test_b3270_xml_multiple(self):

        b3270 = Popen(['b3270'], stdin=PIPE, stdout=PIPE)

        # Feed b3270 two actions, which it will run concurrently and complete
        # in reverse order.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': 'Wait(0.1,seconds) Set(startTls) Quit()', 'r-tag': 'tls' })
        ET.SubElement(top, 'run', { 'actions': 'Set(insertMode)', 'r-tag': 'ins' })
        *first, _, _ = TestCommon.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()
        b3270.wait(timeout=2)

        # Get the result.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split('\n')
        *_, out_ins, out_tls, _, _ = out
        et_ins = ET.fromstring(out_ins)
        et_tls = ET.fromstring(out_tls)

        # Check.
        self.assertEqual('run-result', et_ins.tag)
        att_ins = et_ins.attrib
        self.assertEqual('ins', att_ins['r-tag'])
        self.assertEqual('true', att_ins['success'])
        self.assertEqual('false', att_ins['text'])

        self.assertEqual('run-result', et_tls.tag)
        att_tls = et_tls.attrib
        self.assertEqual('tls', att_tls['r-tag'])
        self.assertEqual('true', att_tls['success'])
        self.assertEqual('true', att_tls['text'])

    # b3270 XML semantic error test
    def test_b3270_xml_semantic_error(self):

        b3270 = Popen(['b3270'], stdin=PIPE, stdout=PIPE)

        # Feed b3270 an action.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'foo': 'bar' })
        ET.SubElement(top, 'run', { 'actions': 'Wait(0.1,seconds) Quit()' })
        *first, _, _ = TestCommon.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()
        b3270.wait(timeout=2)

        # Get the result.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split('\n')
        out_err = out[-4]
        et_err = ET.fromstring(out_err)

        # Check.
        self.assertEqual('ui-error', et_err.tag)
        attr = et_err.attrib
        self.assertEqual('false', attr['fatal'])
        self.assertEqual('missing attribute', attr['text'])
        self.assertEqual('run', attr['element'])
        self.assertEqual('actions', attr['attribute'])

    # b3270 XML syntax error test
    #@unittest.skip('always')
    def test_b3270_xml_syntax_error(self):

        b3270 = Popen(['b3270'], stdin=PIPE, stdout=PIPE, stderr=DEVNULL)

        # Feed b3270 junk.
        top = ET.Element('b3270-in')
        *first, _, _ = TestCommon.xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'<<>' b'\n')
        b3270.stdin.flush()
        rc = b3270.wait(timeout=2)
        self.assertTrue(rc != 0)

        # Get the result.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split('\n')
        out_err = out[-3]
        et_err = ET.fromstring(out_err)

        # Wait for the process to exit.
        b3270.stdin.close()

        # Check.
        self.assertEqual('ui-error', et_err.tag)
        attr = et_err.attrib
        self.assertEqual('true', attr['fatal'])
        self.assertTrue(attr['text'].startswith('XML parsing error'))

if __name__ == '__main__':
    unittest.main()
