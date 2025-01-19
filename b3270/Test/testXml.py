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
# b3270 XML tests

import os
import re
from subprocess import Popen, PIPE, DEVNULL
import unittest
import xml.etree.ElementTree as ET

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

class TestB3270Xml(cti):

    # b3270 NVT XML test
    def test_b3270_nvt_xml_smoke(self):

        # Start 'nc' to read b3270's output.
        nc = copyserver()

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)

        # Feed b3270 some actions.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': f'Open(a:c:t:127.0.0.1:{nc.port}) String(abc) Enter() Disconnect()' })
        *first, _, _ = xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.data()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

    # b3270 XML single test
    def test_b3270_xml_single(self):

        b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 an action.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': 'Set(startTls)' })
        *first, _, _ = xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')

        # Get the result.
        out = ET.fromstring(b3270.communicate(timeout=2)[0].decode('utf8'))

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

        # Check.
        a = out.find('./run-result')
        self.assertTrue(a != None)
        att = a.attrib
        self.assertTrue(att['success'])
        self.assertEqual('true', att['text'])

    # b3270 XML multiple test
    def test_b3270_xml_multiple(self):

        b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 two actions, which it will run concurrently and complete
        # in reverse order.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'actions': 'Wait(0.1,seconds) Set(startTls) Quit()', 'r-tag': 'tls' })
        ET.SubElement(top, 'run', { 'actions': 'Set(insertMode)', 'r-tag': 'ins' })
        *first, _, _ = xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        b3270.stdin.flush()

        # Get the result.
        errmsg = 'b3270 did not produce the expected output'
        pq = pipeq.pipeq(self, b3270.stdout)
        pq.get(2, errmsg)
        pq.get(2, errmsg)
        pq.get(2, errmsg)
        out = pq.get(2, errmsg).decode('utf8')
        et_ins = ET.fromstring(out)
        out = pq.get(2, errmsg).decode('utf8')
        et_tls = ET.fromstring(out)
        self.vgwait(b3270)
        pq.close()
        b3270.stdin.close()
        b3270.stdout.close()

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

        b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 an action.
        top = ET.Element('b3270-in')
        ET.SubElement(top, 'run', { 'foo': 'bar' })
        ET.SubElement(top, 'run', { 'actions': 'Wait(0.1,seconds) Quit()' })
        *first, _, _ = xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'\n')
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.vgwait(b3270)

        # Get the result.
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
    def test_b3270_xml_syntax_error(self):

        b3270 = Popen(vgwrap(['b3270']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Feed b3270 junk.
        top = ET.Element('b3270-in')
        *first, _, _ = xml_prettify(top).split(b'\n')
        b3270.stdin.write(b'\n'.join(first) + b'<<>' b'\n')
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.vgwait(b3270, assertOnFailure=False)
        self.assertTrue(b3270.wait() != 0)

        # Get the result.
        out_err = out[-3]
        et_err = ET.fromstring(out_err)

        # Wait for the process to exit.
        b3270.stdin.close()

        # Check.
        self.assertEqual('ui-error', et_err.tag)
        attr = et_err.attrib
        self.assertEqual('true', attr['fatal'])
        self.assertTrue(attr['text'].startswith('XML parsing error'))

    # b3270 XML not-indented test
    def test_b3270_xml_default(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Grab its output.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.assertEqual(5, len(out))
        self.assertTrue('<?xml version="1.0" encoding="UTF-8"?>' in out[0])
        self.assertEqual('<b3270-out>', out[1])
        self.assertTrue(out[2].startswith('<initialize>'))
        self.assertTrue(out[2].endswith('</initialize>'))
        self.assertEqual('</b3270-out>', out[3])
        self.assertEqual('', out[4])

        self.vgwait(b3270)

    # b3270 XML not-indented, no wrapper test
    def test_b3270_xml_default(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml', '-nowrapperdoc']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Grab its output.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.assertEqual(2, len(out))
        self.assertTrue(out[0].startswith('<initialize>'))
        self.assertTrue(out[0].endswith('</initialize>'))
        self.assertEqual('', out[1])

        self.vgwait(b3270)

    # b3270 XML indented test
    def test_b3270_xml_indented(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml', '-indent']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Grab its output.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.assertTrue('<?xml version="1.0" encoding="UTF-8"?>' in out[0])
        self.assertEqual('<b3270-out>', out[1])
        self.assertEqual(' <initialize>', out[2])
        self.assertEqual(' </initialize>', out[-3])
        self.assertEqual('</b3270-out>', out[-2])
        self.assertEqual('', out[-1])

        self.vgwait(b3270)

    # b3270 XML indented, no wrapper test
    def test_b3270_xml_indented_no_wrapper(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml', '-indent', '-nowrapperdoc']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Grab its output.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split(os.linesep)
        self.assertEqual('<initialize>', out[0])
        self.assertTrue(out[1].startswith(' '))
        self.assertEqual('</initialize>', out[-2])
        self.assertEqual('', out[-1])

        self.vgwait(b3270)

    # b3270 XML no input wrapper test
    def test_b3270_xml_no_input_wrapper(self):

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml', '-nowrapperdoc']), stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        self.children.append(b3270)

        # Feed it multiple commands.
        b3270.stdin.write(b'<run actions="set monoCase"/>\n')
        b3270.stdin.write(b'<run actions="set monoCase"/>\n')
        b3270.stdin.write(b'<run actions="set monoCase"/>\n')
        b3270.stdin.flush()

        # Grab its output.
        # The primary concern here is that b3270 isn't confused by the missing wrapper
        # document and embedded newlines in its input.
        out = b3270.communicate(timeout=2)[0].decode('utf8').split('\n')
        self.assertEqual(5, len(out))
        self.assertTrue(out[1].startswith('<run-result '))
        self.assertTrue(out[2].startswith('<run-result '))
        self.assertTrue(out[3].startswith('<run-result '))
        self.assertEqual('', out[4])

        self.vgwait(b3270)

    # b3270 XML no input wrapper socket test
    def test_b3270_xml_no_input_wrapper_socket(self):

        # Listen for a connection from b3270.
        l = listenserver(self)

        # Start b3270.
        b3270 = Popen(vgwrap(['b3270', '-xml', '-nowrapperdoc', '-callback', str(l.port)]),
            stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
        self.children.append(b3270)

        # Wait for it to call back.
        l.accept(timeout=2)

        # Feed it multiple commands.
        l.send(b'<run actions="set monoCase"/>\n')
        l.send(b'<run actions="set monoCase"/>\n')
        l.send(b'<run actions="set monoCase"/>\n')

        # Grab its output.
        # The primary concern here is that b3270 isn't confused by the missing wrapper
        # document and embedded newlines in its input.
        out = l.data(timeout=2).decode('utf8').split('\n')
        self.assertEqual(5, len(out))
        self.assertTrue(out[1].startswith('<run-result '))
        self.assertTrue(out[2].startswith('<run-result '))
        self.assertTrue(out[3].startswith('<run-result '))
        self.assertEqual('', out[4])

        self.vgwait(b3270)

    # b3270 XML error output test
    def test_b3270_xml_error_output(self):

        b3270 = Popen(vgwrap(['b3270', '-xml', '-nowrapperdoc']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed b3270 an action.
        b3270.stdin.write(b'<run actions="Set(startTls) Set(trace) Set(foo)"/>\n')

        # Get the result.
        out = b3270.communicate(timeout=2)[0].decode().split('\n')[-2]

        # Wait for the process to exit.
        b3270.stdin.close()
        self.vgwait(b3270)

        # Check the 'text-err' output.
        match = re.search(r'text-err="([^"]*)"', out)
        self.assertIsNotNone(match, 'Did not find text-err in output')
        self.assertEqual('false,false,true', match.group(1))

if __name__ == '__main__':
    unittest.main()
