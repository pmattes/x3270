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
# s3270 multi-line output tests

import json
import select
import socket
from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270MultiLine(cti):

    # s3270 multi-line output stdin test
    def test_s3270_multiline_stdin(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Feed it some actions and see what it says.
        stdout = s3270.communicate(b'capabilities errd\nSet(tlsMaxProtocol,foo)\nSet(monocase) Connect(127.0.0.1)\n')[0].splitlines()

        # We expect two ok's and one error.
        self.assertEqual(2, len([line for line in stdout if line == b'ok']))
        self.assertEqual(1, len([line for line in stdout if line == b'error']))

        # Check the output from the third line of input.
        last = stdout[stdout.index(b'ok') + 1:][stdout.index(b'ok') + 1:]
        data = [line for line in last if line.startswith(b'data') or line.startswith(b'errd')]
        self.assertEqual(4, len(data))
        self.assertEqual(b'data: false', data[0])
        self.assertEqual(b'errd: Connection failed:', data[1])
        self.assertTrue(data[2].startswith(b'errd: TLS: Invalid maximum protocol'))
        self.assertTrue(data[3].startswith(b'errd: Valid protocols are'))

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 multi-line output stdin test, JSON mode
    def test_s3270_multiline_stdin_json(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Feed it some actions and see what it says.
        stdout = s3270.communicate(b'capabilities errd\nSet(tlsMaxProtocol,foo)\n"Set(monocase) Connect(127.0.0.1)"\n')[0].splitlines()

        # We expect two ok's.
        self.assertEqual(2, len([line for line in stdout if line == b'ok']))

        # Check the output from the third line of input.
        last = stdout[stdout.index(b'ok') + 1:][stdout.index(b'ok') + 1:]
        third = json.loads(last[0])
        result = third['result']
        self.assertEqual(4, len(result))
        self.assertEqual('false', result[0])
        self.assertEqual('Connection failed:', result[1])
        self.assertTrue(result[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(result[3].startswith('Valid protocols are'))
        self.assertEqual([False, True, True, True], third['result-err'])

        # Wait for the process to exit.
        s3270.stdin.close()
        self.vgwait(s3270)
    
    # s3270 multi-line output scriptport test
    def test_s3270_multiline_scriptport(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
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
        self.vgwait(s3270)
    
    # s3270 multi-line output scriptport test, JSON mode
    def test_s3270_multiline_scriptport_json(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
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
        self.vgwait(s3270)

    # s3270 multi-line output httpd test, text mode
    def test_s3270_multiline_httpd_text(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(monoCase)Connect(127.0.0.1)')
        self.assertFalse(r.ok)
        reply = r.text.splitlines()

        # Check the output.
        self.assertEqual('false', reply[0])
        self.assertEqual('Connection failed:', reply[1])
        self.assertTrue(reply[2].startswith('TLS: Invalid maximum protocol'))
        self.assertTrue(reply[3].startswith('Valid protocols are'))

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(s3270)
    
    # s3270 multi-line output httpd test, HTML mode
    def test_s3270_multiline_httpd_html(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/html/Set(monoCase)Connect(127.0.0.1)')
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
        self.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(s3270)
    
    # s3270 multi-line output httpd test, JSON mode
    def test_s3270_multiline_httpd_json(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(port)

        # Feed it some actions and see what it says.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/text/Set(tlsMaxProtocol,foo)')
        self.assertTrue(r.ok)
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)Connect(127.0.0.1)')
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
        self.get(f'http://127.0.0.1:{port}/3270/rest/text/Quit')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
