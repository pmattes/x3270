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
# b3270 multi-line output tests

import json
import os
import requests
import select
import socket
from subprocess import Popen, PIPE, DEVNULL
import unittest
import xml.etree.ElementTree as ET
import Common.Test.cti as cti
import Common.Test.pipeq as pipeq

class TestB3270MultiLine(cti.cti):

    # b3270 multi-line output stdin test, XML mode
    def test_b3270_multiline_stdin_xml(self):

        # Start b3270.
        b3270 = Popen(cti.vgwrap(['b3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed it some actions and see what it says.
        pq = pipeq.pipeq(self, b3270.stdout)
        b3270.stdin.write(b'<run actions="Set(tlsMaxProtocol,foo) Set(monocase) Connect(127.0.0.1)"/>\n')
        b3270.stdin.flush()
        while True:
            out = pq.get(2, 'b3270 did not complete the actions')
            if out.startswith(b'<run-result'):
                break

        # Check the resut.
        attrib = ET.fromstring(out).attrib
        text = attrib['text'].splitlines()
        self.assertEqual(4, len(text))
        self.assertEqual('false', text[0])
        self.assertEqual('Connection failed:', text[1])
        self.assertTrue(text[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(text[3].startswith('Valid protocols are'))

        text_err = attrib['text-err'].splitlines()[0]
        self.assertEqual('false,true,true,true', text_err)

        # Wait for the process to exit.
        b3270.stdin.write(b'<run actions="quit"/>\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()

    # b3270 multi-line output stdin test, JSON mode
    def test_b3270_multiline_stdin_json(self):

        # Start b3270.
        b3270 = Popen(cti.vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Feed it some actions and see what it says.
        pq = pipeq.pipeq(self, b3270.stdout)
        b3270.stdin.write(b'"Set(tlsMaxProtocol,foo) Set(monocase) Connect(127.0.0.1)"\n')
        b3270.stdin.flush()
        while True:
            out = pq.get(2, 'b3270 did not complete the actions')
            if b'run-result' in out:
                break

        # Check the result.
        result = json.loads(out)
        text = result['run-result']['text']
        self.assertEqual(4, len(text))
        self.assertEqual('false', text[0])
        self.assertEqual('Connection failed:', text[1])
        self.assertTrue(text[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(text[3].startswith('Valid protocols are'))

        text_err = result['run-result']['text-err']
        self.assertEqual([False, True, True, True], text_err)

        # Wait for the process to exit.
        b3270.stdin.write(b'"quit"\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        self.vgwait(b3270)
        pq.close()
        b3270.stdout.close()
    
    # b3270 multi-line output scriptport test
    def test_b3270_multiline_scriptport(self):

        # Start b3270.
        port, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-scriptport', str(port)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.send(b'capabilities errd\nSet(tlsMaxProtocol,foo)\nSet(monocase) Connect(127.0.0.1)\nQuit()\n')
        output = b''
        while True:
            r, _, _ = select.select([s], [], [], 5)
            self.assertNotEqual([], r)
            try:
                blob = s.recv(1024)
            except ConnectionResetError:
                blob = b''
            if len(blob) == 0:
                break
            output += blob
        s.close()
        reply = output.splitlines()

        # We expect three ok's and one error.
        self.assertEqual(3, len([line for line in reply if line == b'ok']))
        self.assertEqual(1, len([line for line in reply if line == b'error']))

        # Check the output from the third line of input.
        last = reply[reply.index(b'ok') + 1:][reply.index(b'ok') + 1:]
        data = [line for line in last if line.startswith(b'data') or line.startswith(b'errd')]
        self.assertEqual(4, len(data))
        self.assertEqual(b'data: false', data[0])
        self.assertEqual(b'errd: Connection failed:', data[1])
        self.assertTrue(data[2].startswith(b'errd: TLS: Invalid maximum protocol'))
        self.assertTrue(data[3].startswith(b'errd: Valid protocols are'))

        # Wait for the process to exit.
        self.vgwait(b3270)
        b3270.stdin.close()
    
    # b3270 multi-line output scriptport test, JSON mode
    def test_b3270_multiline_scriptport_json(self):

        # Start b3270.
        port, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-scriptport', str(port)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.send(b'capabilities errd\nSet(tlsMaxProtocol,foo)\n"Set(monocase) Connect(127.0.0.1)"\nQuit()\n')
        output = b''
        while True:
            r, _, _ = select.select([s], [], [], 5)
            self.assertNotEqual([], r)
            try:
                blob = s.recv(1024)
            except ConnectionResetError:
                blob = b''
            if len(blob) == 0:
                break
            output += blob
        s.close()
        reply = output.splitlines()

        # We expect three ok's.
        self.assertEqual(3, len([line for line in reply if line == b'ok']))

        # Check the output from the third line of input.
        last = reply[reply.index(b'ok') + 1:][reply.index(b'ok') + 1:]
        third = json.loads(last[0])
        result = third['result']
        self.assertEqual(4, len(result))
        self.assertEqual('false', result[0])
        self.assertEqual('Connection failed:', result[1])
        self.assertTrue(result[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(result[3].startswith('Valid protocols are'))
        self.assertEqual([False, True, True, True], third['result-err'])

        # Wait for the process to exit.
        self.vgwait(b3270)
        b3270.stdin.close()

    # b3270 multi-line output httpd test, text mode
    def test_b3270_multiline_httpd_text(self):

        # Start b3270.
        port, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-httpd', str(port)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(monoCase)Connect(127.0.0.1)')
        self.assertFalse(r.ok)
        reply = r.text.splitlines()

        # Check the output.
        self.assertEqual('false', reply[0])
        self.assertEqual('Connection failed:', reply[1])
        self.assertTrue(reply[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(reply[3].startswith('Valid protocols are'))

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(b3270)
        b3270.stdin.close()
    
    # b3270 multi-line output httpd test, HTML mode
    def test_b3270_multiline_httpd_html(self):

        # Start b3270.
        port, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-httpd', str(port)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/html/Set(monoCase)Connect(127.0.0.1)')
        self.assertFalse(r.ok)
        reply = r.text.splitlines()

        # Check the output.
        indices = [t[0] for t in enumerate(reply) if t[1].endswith('false') or t[1].startswith('Connection failed:') or t[1].startswith('TLS: Invalid maximum') or t[1].startswith('Valid protocols are')]
        self.assertEqual(4, len(indices))
        self.assertTrue(reply[indices[0]].endswith('false'))
        self.assertTrue(reply[indices[1]].startswith('Connection failed:'))
        self.assertTrue(reply[indices[2]].startswith('TLS: Invalid maximum'))
        self.assertTrue(reply[indices[3]].startswith('Valid protocols are'))

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(b3270)
        b3270.stdin.close()
    
    # b3270 multi-line output httpd test, JSON mode
    def test_b3270_multiline_httpd_json(self):

        # Start b3270.
        port, ts = cti.unused_port()
        b3270 = Popen(cti.vgwrap(['b3270', '-httpd', str(port)]), stdin=PIPE, stdout=DEVNULL)
        self.children.append(b3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)Connect(127.0.0.1)')
        self.assertFalse(r.ok)
        reply = r.json()

        # Check the output.
        self.assertEqual(4, len(reply['result']))
        self.assertEqual('false', reply['result'][0])
        self.assertTrue(reply['result'][1].startswith('Connection failed:'))
        self.assertTrue(reply['result'][2].startswith('TLS: Invalid maximum'))
        self.assertTrue(reply['result'][3].startswith('Valid protocols are'))
        self.assertEqual([False,True,True,True], reply['result-err'])

        # Wait for the process to exit.
        requests.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(b3270)
        b3270.stdin.close()

if __name__ == '__main__':
    unittest.main()
