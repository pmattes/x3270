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
import select
import os
import unittest
import Common.Test.valpass as valpass

# Pretty-print an XML document.
def xml_prettify(elem):
    rough_string = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ").encode('UTF8')

# Generate an unused port to listen on.
# When the listen is done, close the socket.
def unused_port(ipv6=False):
    s = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('::' if ipv6 else '0.0.0.0', 0))
    return (s.getsockname()[1], s)

# Simple socket copy server.
class copyserver():

    port = 0
    loopback = '127.0.0.1'
    qloopback = '127.0.0.1'

    # Initialization.
    def __init__(self, port=0, ipv6=False):
        self.listensocket = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.loopback = '::1' if ipv6 else '127.0.0.1'
        self.qloopback = '[::1]' if ipv6 else '127.0.0.1'
        self.listensocket.bind((self.loopback, port))
        if port == 0:
            self.port = self.listensocket.getsockname()[1]
        else:
            self.port = port
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
        assert(not self.thread.is_alive())
        return self.result

# Simple socket listen / accept / receive all.
class listenserver():

    port = 0
    loopback = '127.0.0.1'
    qloopback = '127.0.0.1'
    __tc = None

    # Initialization.
    def __init__(self, tc, ipv6=False):
        self.__tc = tc
        self.listensock = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.loopback = '::1' if ipv6 else '127.0.0.1'
        self.qloopback = '[::1]' if ipv6 else '127.0.0.1'
        self.listensock.bind((self.loopback, 0))
        self.port = self.listensock.getsockname()[1]
        self.listensock.listen()

    # Accept a connection.
    def accept(self, timeout=0):
        if timeout != 0:
            r, _, _ = select.select([ self.listensock ], [], [], timeout)
            self.__tc.assertNotEqual([], r, 'Accept timed out')

        (self.iosock, _) = self.listensock.accept()
        self.listensock.close()

    # Send data.
    def send(self, bytes):
        self.iosock.send(bytes)
    
    # Get data.
    def data(self, timeout=0):
        # Handle an apparent bug in Windows when a loopback connection is shut for writing
        # when the receive side does not have a select() or recv() up. The FD_CLOSE indication
        # is lost. If we pause here, it gives the reader time to re-post their select/recv.
        if sys.platform.startswith('win'):
            time.sleep(0.1)
        self.iosock.shutdown(socket.SHUT_WR)
        out = b''
        while True:
            if timeout != 0:
                r, _, _ = select.select([ self.iosock ], [], [], timeout)
                self.__tc.assertNotEqual([], r, 'Input timed out')
            try:
                chunk = self.iosock.recv(65536)
            except ConnectionResetError:
                # As Windows is wont to do.
                break
            if chunk == b'':
                break
            out += chunk
        self.iosock.close()
        return out

# Simple socket send server.
class sendserver():

    port = 0
    loopback = '127.0.0.1'
    qloopback = '127.0.0.1'
    conn = None
    __tc = None

    # Initialization.
    def __init__(self, tc, port=0, ipv6=False):
        self.__tc = tc
        self.listensocket = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM, 0)
        self.listensocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.loopback = '::1' if ipv6 else '127.0.0.1'
        self.qloopback = '[::1]' if ipv6 else '127.0.0.1'
        self.listensocket.bind((self.loopback, port))
        if port == 0:
            self.port = self.listensocket.getsockname()[1]
        else:
            self.port = port
        self.listensocket.listen()
        self.result = b''
        self.thread = threading.Thread(target=self.process)
        self.thread.start()

    # Accept a connection asynchronously.
    def process(self):
        (self.conn, _) = self.listensocket.accept()
        self.listensocket.close()

    def send(self, data):
        cti.try_until(self.__tc, lambda: (self.conn != None), 2, 'emulator did not connect')
        self.conn.send(data)

    def close(self):
        self.conn.close()
        self.conn = None
        self.thread.join(timeout=2)

def vgwrap(command):
    '''Wrap a command in valgrind'''
    if 'VALGRIND' in os.environ:
        return ['valgrind', '--leak-check=full', '--log-file=/tmp/valgrind.%p', '--child-silent-after-fork=yes'] + command
    else:
        return command

def vgwrap_ecmd(command):
    '''Wrap an execvp command in valgrind'''
    return 'valgrind' if 'VALGRIND' in os.environ else command

def vgwrap_eargs(args):
    '''Wrap execvp arguments in valgrind'''
    if 'VALGRIND' in os.environ:
        return ['valrgind', '--leak-check=full', '--log-file=/tmp/valgrind.%p',
            '--child-silent-after-fork=yes'] + args
    else:
        return args

def sa_try_until(f, seconds, errmsg):
    '''Try f periodically until seconds elapse'''
    start = time.monotonic_ns()
    while True:
        now = time.monotonic_ns()
        if now - start > seconds * 1e9:
            print(errmsg, file=sys.stderr)
            assert False
        if f():
            return
        time.sleep(0.1)

# Common test infrastructure class.
class cti(unittest.TestCase):
    def setUp(self):
        '''Common set-up procedure'''
        self.children = []

    def tearDown(self):
        '''Common tear-down procedure'''
        for child in self.children:
            child.kill()
            child.wait()

    def try_until(self, f, seconds, errmsg):
        '''Try f periodically until seconds elapse'''
        start = time.monotonic_ns()
        while True:
            now = time.monotonic_ns()
            self.assertLess(now - start, seconds * 1e9, errmsg)
            if f():
                return
            time.sleep(0.1)

    def check_listen(self, port, ipv6=False):
        '''Check for a particular port being listened on'''
        if sys.platform == 'darwin':
            r = re.compile(rf'\.{port} .* LISTEN')
        else:
            r = re.compile(rf':{port} .* LISTEN')
        if sys.platform.startswith("win") and ipv6:
            cmd = 'netstat -an -p TCPv6'
        elif sys.platform.startswith("win") or sys.platform == 'darwin':
            cmd = 'netstat -an -p TCP'
        else:
            cmd = 'netstat -ant'
        def test():
            netstat = Popen(cmd, shell=True, stdout=PIPE)
            stdout = netstat.communicate()[0].decode('utf8').split('\n')
            return any(r.search(line) for line in stdout)
        self.try_until(test, 2, f"Port {port} is not bound")

    def to_playback(self, playback, port, s):
        '''Write a string to playback after verifying the emulator is blocked'''
        # Wait for the action to block.
        def test():
            j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
            return any('Wait(' in line for line in j['result'])
        self.try_until(test, 2, "emulator did not block")
        
        # Trigger the host output.
        playback.stdin.write(s)
        playback.stdin.flush()

    def check_push(self, p, port, count):
        '''Check for pushed data'''
        # Send a timing mark.
        p.stdin.write(b"t\n")
        p.stdin.flush()
        def test():
            j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(TimingMarks)').json()
            return j['result'][0] == str(count)
        self.try_until(test, 2, "emulator did not accept the data")

    def check_dash_v(self, prog, with_w=False):
        '''Make sure the "-v" option works'''
        p = Popen([prog, '-v'], stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        p.wait(timeout=2)
        vprog = 'w' + prog if with_w else prog
        self.assertTrue(stderr[0].startswith(vprog + ' '), '-v does not start with program name')
        self.assertTrue(any('Copyright' in line for line in stderr), 'Copyright not found')

    def check_help(self, prog):
        '''Make sure the "--help" option works'''
        p = Popen([prog, '--help'], stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        p.wait(timeout=2)
        self.assertTrue(stderr[0].startswith('Usage: ' + prog + ' '), '--help does not start with Usage')

    def check_bad(self, prog, extra=None):
        '''Make sure a bad option fails'''
        args = [prog]
        if extra != None:
            args.append(extra)
        args.append('-foo')
        p = Popen(args, stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        rc = p.wait(timeout=2)
        self.assertNotEqual(0, rc, 'program should fail')
        self.assertTrue(stderr[0].startswith("Unknown or incomplete option: '-foo'"), 'Bad unknown option message')
        self.assertTrue(any(line.startswith('Usage: ') for line in stderr), 'Missing Usage message')
        self.assertTrue(any('Use --help' in line for line in stderr), 'Missing --help prompt')

    def check_bad2(self, prog, extra=None):
        '''Make sure many bad options fail'''
        args = [prog]
        if extra != None:
            args.append(extra)
        args += ['-foo', '-bar', '-baz']
        p = Popen(args, stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        rc = p.wait(timeout=2)
        self.assertNotEqual(0, rc, 'Program should fail')
        self.assertTrue(stderr[0].startswith("Unknown or incomplete option: '-foo'"), 'Bad unknown option message')
        self.assertTrue(any(line.startswith('Usage: ') for line in stderr), 'Missing Usage message')
        self.assertTrue(any('Use --help' in line for line in stderr), 'Missing --help prompt')

    def check_toomany(self, prog, extra=None):
        '''Make sure too many command-line options fail'''
        args = [prog]
        if extra != None:
            args.append(extra)
        args += ['able', 'baker', 'charlie']
        p = Popen(args, stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        rc = p.wait(timeout=2)
        self.assertNotEqual(0, rc, 'Program should fail')
        self.assertTrue(stderr[0].startswith("Too many command-line options"), 'Missing "Too many" error')
        self.assertTrue(any(line.startswith('Usage: ') for line in stderr), 'Missing Usage message')
        self.assertTrue(any('Use --help' in line for line in stderr), 'Missing --help prompt')

    def check_incomplete(self, prog, extra=None):
        '''Make sure an incomplete option fails'''
        args = [prog]
        if extra != None:
            args.append(extra)
        args.append('-codepage')
        p = Popen(args, stderr=PIPE)
        stderr = p.communicate(timeout=2)[1].decode('utf8').split('\n')
        rc = p.wait(timeout=2)
        self.assertNotEqual(0, rc, 'Program should fail')
        self.assertTrue(stderr[0].startswith("Unknown or incomplete option: '-codepage'") or stderr[0].startswith("Missing value for '-codepage'"), 'Wrong "missing" message')
        self.assertTrue(any(line.startswith('Usage: ') for line in stderr), 'Missing Usage message')
        self.assertTrue(any('Use --help' in line for line in stderr), 'Missing --help prompt')

    def vgcheck(self, pid, rc, assertOnFailure):
        '''Check a valgrind log file'''
        isVal = 'VALGRIND' in os.environ
        valLog = f'/tmp/valgrind.{pid}'
        if isVal:
            self.assertTrue(valpass.valpass().check(valLog), f'Valgrind error(s) found, see {valLog}')
        if (assertOnFailure):
            self.assertEqual(0, rc, 'Program failed')
        if isVal:
            os.unlink(valLog)

    def vgwait(self, p, timeout=2, assertOnFailure=True):
        '''Wait for a subprocess with a timeout, optionally assert on failure, and clean up the valgrind log file'''
        pid = p.pid
        rc = p.wait(timeout=timeout)
        self.vgcheck(pid, rc, assertOnFailure)

    def vgwait_pid(self, pid, timeout=2, assertOnFailure=True):
        '''Wait for a process with a timeout, optionally assert on failure, and clean up the valgrind log file'''
        (_, status) = os.waitpid(pid, 0) # xxx: should be timed
        if os.WIFSIGNALED(status):
            self.assertTrue(False, f'Process killed by signal {os.WTERMSIG(status)}')
        rc = os.WEXITSTATUS(status)
        self.vgcheck(pid, rc, assertOnFailure)

    def timed_readline(self, p, timeout, errmsg):
        '''Do a readline from a pipe with a timeout'''
        self.try_until(lambda: (p.readable()), timeout, errmsg)
        return p.readline()
