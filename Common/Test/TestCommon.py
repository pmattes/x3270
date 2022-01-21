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
# Common functions for integration testing

from subprocess import Popen, PIPE, DEVNULL
import requests
import time
import re
import sys
import xml.etree.ElementTree as ET
from xml.dom import minidom
import socket
import threading

# Try f periodically until seconds elapse.
def try_until(f, seconds, errmsg):
    start = time.monotonic_ns()
    while True:
        now = time.monotonic_ns()
        if now - start > seconds * 1e9:
            print(f"***** {errmsg}", file=sys.stderr, flush=True)
            assert False
        if f():
            return
        time.sleep(0.1)

# Check for a particular port being listened on.
def check_listen(port):
    if sys.platform == 'darwin':
        r = re.compile(rf'\.{port} .* LISTEN')
    else:
        r = re.compile(rf':{port} .* LISTEN')
    if sys.platform.startswith("win") or sys.platform == 'darwin':
        cmd = 'netstat -an -p TCP'
    else:
        cmd = 'netstat -ant'
    def test():
        netstat = Popen(cmd, shell=True, stdout=PIPE)
        stdout = netstat.communicate()[0].decode('utf8').split('\n')
        return any(r.search(line) for line in stdout)
    try_until(test, 2, f"Port {port} is not bound")

# Write a string to playback after verifying the emulator is blocked.
def to_playback(p, port, s):
    # Wait for the CursorAt command to block.
    def test():
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
        return any('Wait(' in line for line in j['result'])
    try_until(test, 2, "emulator did not block")
    
    # Trigger the host output.
    p.stdin.write(s)
    p.stdin.flush()

# Check for pushed data.
def check_push(p, port, count):
    # Send a timing mark.
    p.stdin.write(b"t\n")
    p.stdin.flush()
    def test():
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(TimingMarks)').json()
        return j['result'][0] == str(count)
    try_until(test, 2, "emulator did not accept the data")

# Make sure the "-v" option works.
def check_dash_v(prog):
    p = Popen([prog, '-v'], stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    p.wait(timeout=2)
    assert stderr[0].startswith(prog + ' ')
    assert any('Copyright' in line for line in stderr)

# Make sure the "--help" option works.
def check_help(prog):
    p = Popen([prog, '--help'], stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    p.wait(timeout=2)
    assert stderr[0].startswith('Usage: ' + prog + ' ')

# Make sure a bad option fails.
def check_bad(prog, extra=None):
    args = [prog]
    if extra != None:
        args.append(extra)
    args.append('-foo')
    p = Popen(args, stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    rc = p.wait(timeout=2)
    assert rc != 0
    assert stderr[0].startswith("Unknown or incomplete option: '-foo'")
    assert any(line.startswith('Usage: ') for line in stderr)
    assert any('Use --help' in line for line in stderr)

# Make sure many bad options fail.
def check_bad2(prog, extra=None):
    args = [prog]
    if extra != None:
        args.append(extra)
    args.append('-foo')
    args.append('-bar')
    args.append('-baz')
    p = Popen(args, stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    rc = p.wait(timeout=2)
    assert rc != 0
    assert stderr[0].startswith("Unknown or incomplete option: '-foo'")
    assert any(line.startswith('Usage: ') for line in stderr)
    assert any('Use --help' in line for line in stderr)

# Make sure too many command-line options fail.
def check_toomany(prog, extra=None):
    args = [prog]
    if extra != None:
        args.append(extra)
    args.append('able')
    args.append('baker')
    args.append('charlie')
    p = Popen(args, stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    rc = p.wait(timeout=2)
    assert rc != 0
    assert stderr[0].startswith("Too many command-line options")
    assert any(line.startswith('Usage: ') for line in stderr)
    assert any('Use --help' in line for line in stderr)

# Make sure an incomplete option fails.
def check_incomplete(prog, extra=None):
    args = [prog]
    if extra != None:
        args.append(extra)
    args.append('-codepage')
    p = Popen(args, stderr=PIPE)
    stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
    rc = p.wait(timeout=2)
    assert rc != 0
    assert stderr[0].startswith("Unknown or incomplete option: '-codepage'") or stderr[0].startswith("Missing value for '-codepage'")
    assert any(line.startswith('Usage: ') for line in stderr)
    assert any('Use --help' in line for line in stderr)

# Pretty-print an XML document.
def xml_prettify(elem):
    rough_string = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ").encode('UTF8')

# Generate an unused port to listen on.
# When the listen is done, close the socket.
def unused_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', 0))
    return (s.getsockname()[1], s)

# Simple socket copy server.
class copyserver():

    # Initialization.
    def __init__(self, port):
        self.listensocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listensocket.bind(('127.0.0.1', port))
        self.listensocket.listen()
        self.result = b''
        self.thread = threading.Thread(target=self.process)
        self.thread.start()

    # Accept a connection and read to EOF.
    def process(self):
        (conn, _) = self.listensocket.accept()
        self.listensocket.close()
        while True:
            rdata = conn.recv(1024)
            if (len(rdata) == 0):
                break
            self.result += rdata
        conn.close()

    # Return what we got.
    def data(self):
        self.thread.join(timeout=2)
        return self.result

