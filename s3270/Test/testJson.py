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
# s3270 auto-JSON tests

import json
import select
import socket
from subprocess import Popen, PIPE, DEVNULL
import os
import pathlib
import sys
import time
import tempfile
import unittest

from Common.Test.cti import *

@requests_timeout
class TestS3270Json(cti):

    # Check a JSON-formatted result.
    def check_result_json(self, out):
        j = json.loads(out)
        self.assertEqual('true', j['result'][0])
        self.assertTrue(j['success'])
        self.assertTrue(j['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

    # Check an s3270-formatted result.
    def check_result_s3270(self, out):
        self.assertEqual('data: true', out[0])
        self.assertTrue(out[1].startswith('L U U N N 4 24 80 0 0 0x0 '))
        self.assertEqual('ok', out[2])

    # Read from a socket until EOF.
    def recv_to_eof(self, s, timeout=0):
        if sys.platform.startswith('win'):
            # There appears to be a Windows bug that causes a 'shutdown' to be lost
            # on a loopback connection if the other side does not nave a recv or select
            # posted. Waiting 0.1s gives s3270 time to do that.
            time.sleep(0.1)
        s.shutdown(socket.SHUT_WR)
        result = b''
        while True:
            if timeout != 0:
                rfds, _, _ = select.select([ s ], [], [], timeout)
            self.assertNotEqual([], rfds, 'Socket read timed out')
            r = s.recv(1024)
            if len(r) == 0:
                break
            result += r
        return result.decode('utf8')

    # s3270 basic stdin JSON test
    def test_s3270_stdin_json(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a JSON-formatted command at it.
        command = json.dumps({'action':'Set','args':['startTls']}).encode('utf8') + b'\n'
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8')

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(stdout)

    # s3270 basic socket JSON test
    def test_s3270_socket_json(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push a JSON-formatted command at it.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        command = json.dumps({'action':'Set','args':['startTls']}).encode('utf8') + b'\n'
        s.sendall(command)

        # Decode the result.
        result = self.recv_to_eof(s, 2)
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(result)

    # s3270 multi-line socket JSON test
    def test_s3270_socket_json_multi(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push a JSON-formatted command at it, in two pieces.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        command = json.dumps({'action':'Set','args':['startTls']}).replace(' ', '\n').encode('utf8') + b'\n'
        s.sendall(command[0:15])
        time.sleep(0.2)
        s.sendall(command[15:])

        # Decode the result.
        result = self.recv_to_eof(s, 2)
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(result)

    # s3270 multi-line stdin JSON test
    def test_s3270_stdin_multiline_json(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a JSON-formatted command at it.
        command = json.dumps({'action':'Set','args':['startTls']}).replace(' ', '\n').encode('utf8') + b'\n'
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8')

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(stdout)

    # s3270 basic stdin JSON string test
    def test_s3270_stdin_json_string(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a JSON-formatted command at it.
        command = json.dumps('Set(startTls)').encode('utf8') + b'\n'
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8')

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(stdout)

    # s3270 socket JSON string test
    def test_s3270_socket_json_string(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push a JSON-formatted command at it.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        command = json.dumps('Set(startTls)').encode('utf8') + b'\n'
        s.sendall(command)

        # Decode the result.
        result = self.recv_to_eof(s, 2)
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.check_result_json(result)

    # s3270 JSON semantic error test
    def test_s3270_stdin_json_bad_semantics(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a bad JSON-formatted command at it.
        command = json.dumps({'foo':'bar'}).encode('utf8') + b'\n'
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8')

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output, which is in JSON format.
        j = json.loads(stdout)
        self.assertEqual("Missing object member 'action'", j['result'][0])
        self.assertFalse(j['success'])
        self.assertTrue(j['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

    # s3270 socket JSON semantic error test
    def test_s3270_socket_json_semantic_error(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push a JSON-formatted command at it.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        command = json.dumps({'foo':'bar'}).encode('utf8') + b'\n'
        s.sendall(command)

        # Decode the result.
        result = self.recv_to_eof(s, 2)
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        j = json.loads(result)
        self.assertEqual("Missing object member 'action'", j['result'][0])
        self.assertFalse(j['success'])
        self.assertTrue(j['status'].startswith('L U U N N 4 24 80 0 0 0x0 '))

    # s3270 JSON syntax error test
    def test_s3270_stdin_json_syntax(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a bad syntax JSON-formatted command at it.
        s3270.stdin.write(b'{"foo"}\n')

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8').split(os.linesep)

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output, which is in s3270 format.
        # This is because JSON parsing is inferred from the first
        # non-whitespace character on the line, and that guess might be wrong.
        self.assertEqual(4, len(stdout))
        self.assertTrue(stdout[0].startswith('data: JSON parse error'))
        self.assertTrue(stdout[1].startswith('L U U N N 4 24 80 0 0 0x0 '))
        self.assertEqual('error', stdout[2])
        self.assertEqual('', stdout[3])

    # s3270 socket JSON syntax error test
    def test_s3270_socket_json_syntax_error(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push a JSON-formatted command at it.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'{"foo"}\n')

        # Decode the result.
        result = self.recv_to_eof(s, 2).split('\n')
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output, which is in s3270 format.
        # This is because JSON parsing is inferred from the first
        # non-whitespace character on the line, and that guess might be wrong.
        self.assertEqual(4, len(result))
        self.assertTrue(result[0].startswith('data: JSON parse error'))
        self.assertTrue(result[1].startswith('L U U N N 4 24 80 0 0 0x0 '))
        self.assertEqual('error', result[2])
        self.assertEqual('', result[3])

    # s3270 JSON/s3270 mode-switching test.
    def test_s3270_stdin_mode_switch(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push s3270, then JSON, then s3270, then JSON at it.
        s3270.stdin.write(('Set(startTls)' + os.linesep).encode('utf8'))
        command = (json.dumps({'action':'Set','args':['startTls']}).replace(' ', os.linesep) + os.linesep).encode('utf8')
        s3270.stdin.write(command)
        s3270.stdin.write(('Set(startTls)' + os.linesep).encode('utf8'))
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode('utf8').split(os.linesep)

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        # First three lines are s3270, next line is JSON, next three are s3270,
        # last is JSON.
        self.check_result_s3270(stdout[0:3])
        self.check_result_json(stdout[3])
        self.check_result_s3270(stdout[4:7])
        self.check_result_json(stdout[7])

    # s3270 socket JSON mode-switch test
    def test_s3270_socket_json_mode_switch(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push s3270, then JSON, then s3270, then JSON at it.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'Set(startTls)\n')
        command = json.dumps({'action':'Set','args':['startTls']}).encode('utf8') + b'\n'
        s.sendall(command)
        s.sendall(b'Set(startTls)\n')
        s.sendall(command)

        # Decode the result.
        result = self.recv_to_eof(s, 2).split('\n')
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        # First three lines are s3270, next line is JSON, next three are s3270,
        # last is JSON.
        self.check_result_s3270(result[0:3])
        self.check_result_json(result[3])
        self.check_result_s3270(result[4:7])
        self.check_result_json(result[7])

    # Verify that HTTPD JSON output is all on one line (GET).
    def test_s3270_http_json_one_line_get(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Send a request.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Query()')
        self.assertTrue(r.ok)
        out = r.text
        while out.endswith('\r') or out.endswith('\n'):
            out = out[0:-1]
        self.assertFalse('\n' in out)

        # Clean up.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Verify that HTTPD JSON output is all on one line (POST).
    def test_s3270_http_json_one_line_post(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Send a request.
        r = self.post(f'http://127.0.0.1:{port}/3270/rest/post', json="Query")
        self.assertTrue(r.ok)
        out = r.text
        while out.endswith('\r') or out.endswith('\n'):
            out = out[0:-1]
        self.assertFalse('\n' in out)

        # Clean up.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Test JSON with child pipe I/O (POSIX only).
    def s3270_pipechild(self, gulp=False, suffix=''):
        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Run a script that will push requests (ending in Quit()) to s3270, and copy the output to a temp file.
        (handle, outfile) = tempfile.mkstemp()
        os.close(handle)
        gulp_opt = ',-gulp' if gulp else ''
        try:
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Script(python3,s3270/Test/pipescript.py{gulp_opt},s3270/Test/pipescript{suffix}.in,{outfile})')
        except:
            pass

        # Wait for s3270.
        self.vgwait(s3270)

        # Verify the output.
        # We have to wait explicitly for the output file to appear, because s3270 exits before the script writes it.
        self.try_until(lambda: pathlib.Path(outfile).stat().st_size != 0, 2, 'outfile not updated')
        self.assertEqual(pathlib.Path(outfile).read_text(), pathlib.Path(f's3270/Test/pipescript{suffix}.out').read_text())
        os.unlink(outfile)

    @unittest.skipIf(sys.platform.startswith('win'), "Windows does not support FD pipes")
    def test_s3270_pipechild(self):
        self.s3270_pipechild()
    @unittest.skipIf(sys.platform.startswith('win'), "Windows does not support FD pipes")
    def test_s3270_pipechild_gulp(self):
        self.s3270_pipechild(gulp=True)
    @unittest.skipIf(sys.platform.startswith('win'), "Windows does not support FD pipes")
    def test_s3270_pipechild_syntax_error(self):
        self.s3270_pipechild(suffix='-syntax-error')

    # Verify stdin JSON result-err.
    def test_s3270_stdin_json_result_err(self):

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270']), stdin=PIPE, stdout=PIPE)
        self.children.append(s3270)

        # Push a JSON-formatted command at it.
        command = json.dumps('Set(startTls)Set(trace)Set(foo)').encode() + b'\n'
        s3270.stdin.write(command)

        # Decode the result.
        stdout = s3270.communicate()[0].decode()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        out = json.loads(stdout)
        self.assertEqual([False, False, True], out['result-err'])

    # Verify HTTPD JSON result-err.
    def test_s3270_http_json_result_err(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Send a request.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(startTls)Set(trace)Set(foo)')
        self.assertFalse(r.ok)
        out = json.loads(r.text)
        self.assertEqual([False, False, True], out['result-err'])

        # Clean up.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
    # Verify socket JSON result-err
    def test_s3270_socket_json_result_err(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port)]))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Push actions in JSON, where the last fails.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'"Set(startTls)Set(trace)Set(foo)"\n')

        # Decode the result.
        result = json.loads(self.recv_to_eof(s, 2))
        s.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.sendall(b'quit\n')
        s.close()

        # Wait for the process to exit successfully.
        self.vgwait(s3270)

        # Test the output.
        self.assertEqual([False, False, True], result['result-err'])

    # Test JSON with child pipe I/O (POSIX only).
    @unittest.skipIf(sys.platform.startswith('win'), "Windows does not support FD pipes")
    def test_s3270_pipechild_json_result_err(self):
        self.s3270_pipechild(self, suffix='-triple')
    
    # Test JSON with child pipe I/O (POSIX only).
    @unittest.skipIf(sys.platform.startswith('win'), "Windows does not support FD pipes")
    def test_s3270_pipechild_json_result_spaces(self):
        self.s3270_pipechild(self, suffix='-spaces')

if __name__ == '__main__':
    unittest.main()
