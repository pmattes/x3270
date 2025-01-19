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
# s3270 security cookie tests

import os
import os.path
import random
import string
import socket
from subprocess import Popen, PIPE, DEVNULL
import sys
import tempfile
import time
import unittest

from Common.Test.cti import *
import Common.Test.playback as playback

def x3270if(port: int, command: str, timeout=0.5) -> tuple[bool, list[str], int]:
    '''Send an s3270-protocol command to s3270'''
    s = socket.socket()
    s.settimeout(timeout)
    s.connect(('127.0.0.1', port))
    t0 = time.time()
    s.send(command.encode() + b'\n')
    result = []
    while (True):
        r = s.recv(1024).decode().split('\n')
        if r == []:
            s.close()
            return None
        for line in r:
            if line == 'ok':
                s.close()
                return (True, result, time.time() - t0)
            if line == 'error':
                s.close()
                return (False, result, time.time() - t0)
            if line.startswith('data: '):
                result.append(line[6:])

def random_word(length: int) -> str:
    '''Generate a random string'''
    letters = string.ascii_lowercase + string.ascii_uppercase + string.digits + '-_.'
    return ''.join(random.choice(letters) for i in range(length))

@requests_timeout
class TestS3270Cookie(cti):

    # s3270 s3270-mode cookie test
    def test_s3270_s3270_cookie(self):

        port, ts = unused_port()
        with tempfile.NamedTemporaryFile(delete=False) as tf:

            tf_name = tf.name

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port), '-cookiefile', tf.name]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            ts.close()

            # Make sure s3270 puts something in the file. Then read the cookie from it.
            self.try_until(lambda: os.path.getsize(tf.name) > 0, 2, 's3270 did not write to the cookie file')
            with open(tf.name, 'r') as cookiefile:
                cookie = cookiefile.read()

            # Except on Windows, make sure the file has 0400 permissions now.
            if not sys.platform.startswith('win'):
                s = os.stat(tf.name)
                self.assertEqual(0o400, (s.st_mode & 0o777))

            # Try an s3270 protocol command without specifying a cookie.
            r = x3270if(port, 'set')
            self.assertFalse(r[0])

            # Specify the wrong cookie. That takes more than a second to fail.
            r = x3270if(port, f'Cookie(abc)', timeout=3)
            self.assertFalse(r[0])
            self.assertGreater(r[2], 1.0)

            # Specify the cookie.
            r = x3270if(port, f'Cookie({cookie})')
            self.assertTrue(r[0])

            # Stop.
            x3270if(port, f'Cookie({cookie}) Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)
        os.unlink(tf_name)

    # s3270 HTTP cookie test
    def test_s3270_http_cookie(self):

        port, ts = unused_port()
        with tempfile.NamedTemporaryFile(delete=False) as tf:

            tf_name = tf.name

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(port), '-cookiefile', tf.name]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            ts.close()

            # Make sure s3270 puts something in the file. Then read the cookie from it.
            self.try_until(lambda: os.path.getsize(tf.name) > 0, 2, 's3270 did not write to the cookie file')
            with open(tf.name, 'r') as cookiefile:
                cookie = cookiefile.read()

            # Except on Windows, make sure the file has 0400 permissions now.
            if not sys.platform.startswith('win'):
                s = os.stat(tf.name)
                self.assertEqual(0o400, (s.st_mode & 0o777))

            # Try an command without specifying a cookie.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)')
            self.assertFalse(r.ok)

            # Specify the wrong cookie.
            t = time.time()
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)', cookies={"x3270-security": "foo"})
            self.assertFalse(r.ok)
            self.assertEqual(403, r.status_code)
            self.assertGreater(time.time() - t, 1.0)

            # Specify the right cookie.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(monoCase)', cookies={"x3270-security": cookie})
            self.assertTrue(r.ok)

            # Stop.
            r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()', cookies={"x3270-security": cookie})

        os.unlink(tf_name)

        # Wait for the processes to exit.
        self.vgwait(s3270)

    
    # s3270 s3270-mode cookie test with a file that does not exist yet.
    def test_s3270_s3270_cookie_create_file(self):

        port, ts = unused_port()
        tf = tempfile.NamedTemporaryFile(delete=False)

        # Start s3270.
        s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port), '-cookiefile', tf.name]), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()

        # Make sure s3270 creates and puts something in the file. Then read the cookie from it.
        self.try_until(lambda: os.path.exists(tf.name) and os.path.getsize(tf.name) > 0, 2, 's3270 did not write to the cookie file')
        with open(tf.name, 'r') as cookiefile:
            cookie = cookiefile.read()

        # Except on Windows, make sure the file has 0400 permissions now.
        if not sys.platform.startswith('win'):
            s = os.stat(tf.name)
            self.assertEqual(0o400, (s.st_mode & 0o777))

        # Try an s3270 protocol command without specifying a cookie.
        r = x3270if(port, 'set')
        self.assertFalse(r[0])

        # Specify the wrong cookie. That takes more than a second to fail.
        r = x3270if(port, f'Cookie(abc)', timeout=3)
        self.assertFalse(r[0])
        self.assertGreater(r[2], 1.0)

        # Specify the cookie.
        r = x3270if(port, f'Cookie({cookie})')
        self.assertTrue(r[0])

        # Stop.
        x3270if(port, f'Cookie({cookie}) Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)

        tf.close()
        os.unlink(tf.name)
    
    # s3270 s3270-mode cookie test with a file that we create and fill in
    def test_s3270_s3270_cookie_filled(self):

        port, ts = unused_port()
        with tempfile.NamedTemporaryFile(delete=False) as tf:

            tf_name = tf.name

            # Generate a cookie.
            cookie = random_word(10)

            # Write it into the temporary file.
            fd = os.open(tf.name, os.O_WRONLY)
            os.write(fd, cookie.encode())
            os.close(fd)

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port), '-cookiefile', tf.name]), stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(s3270)
            ts.close()
            self.check_listen(port)

            # Except on Windows, make sure the file has 0400 permissions now.
            if not sys.platform.startswith('win'):
                s = os.stat(tf.name)
                self.assertEqual(0o400, (s.st_mode & 0o777))

            # Try an s3270 protocol command without specifying a cookie.
            r = x3270if(port, 'set')
            self.assertFalse(r[0])

            # Specify the wrong cookie. That takes more than a second to fail.
            r = x3270if(port, f'Cookie(abc)', timeout=3)
            self.assertFalse(r[0])
            self.assertGreater(r[2], 1.0)

            # Specify the cookie.
            r = x3270if(port, f'Cookie({cookie})')
            self.assertTrue(r[0])

            # Stop.
            x3270if(port, f'Cookie({cookie}) Quit()')

        # Wait for the processes to exit.
        self.vgwait(s3270)
        os.unlink(tf_name)

    # s3270 s3270-mode cookie test with a file that we create and fill in with a bad cookie value.
    def s3270_s3270_cookie_filled_wrong(self, contents: str):

        port, ts = unused_port()
        with tempfile.NamedTemporaryFile(delete=False) as tf:

            tf_name = tf.name

            # Write it into the temporary file.
            fd = os.open(tf.name, os.O_WRONLY)
            os.write(fd, contents.encode())
            os.close(fd)

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-scriptport', str(port), '-cookiefile', tf.name]), stdin=DEVNULL, stdout=DEVNULL, stderr=PIPE)
            self.children.append(s3270)
            ts.close()

            # Wait for the process to exit.
            self.vgwait(s3270, assertOnFailure=False)

        errmsg = s3270.stderr.readlines()
        s3270.stderr.close()
        self.assertTrue(b'invalid cookie' in errmsg[0])
        os.unlink(tf_name)
    
    def test_s3270_s3270_cookie_filled_wrong_char(self):
        self.s3270_s3270_cookie_filled_wrong(',')
    def test_s3270_s3270_cookie_filled_wrong_whitespace(self):
        self.s3270_s3270_cookie_filled_wrong('ab cd')


if __name__ == '__main__':
    unittest.main()
