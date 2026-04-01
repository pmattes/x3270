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
# s3270 trace tests

import os
from subprocess import Popen, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270Trace(cti):

    # s3270 trace header test
    def test_s3270_trace_header(self):

        # Start s3270.
        http_port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'127.0.0.1:{http_port}']), stdin=DEVNULL, stdout=DEVNULL)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Turn off tracing, in case this was run with TRACEALL set.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Trace(off)')

        # Turn tracing back on, to a specific file.
        (handle, name) = tempfile.mkstemp()
        os.close(handle)
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Trace(on,{name})')

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Check the tracefile. There's quite a lot we could check here, but the minimum to start with is
        # that it contains a Query()-based header that includes information that wasn't in the hard-coded
        # header.
        with open(name, 'r') as file:
            lines = file.readlines()
        os.unlink(name)
        self.assertIn(' ReplyMode: field\n', lines)

        # Wait for the processes to exit.
        self.vgwait(s3270)

    # s3270 trace detail test
    def s3270_detail_trace(self, env=False, spec='sched', success=True):

        # Start s3270.
        handle, tracefile = tempfile.mkstemp()
        os.close(handle)

        http_port, ts = unused_port()
        args = ['s3270', '-httpd', f'127.0.0.1:{http_port}']
        env = os.environ.copy()
        env['X3270TOLR'] = tracefile
        if env:
            env['X3270DETAILTRACE'] = spec
        else:
            args += ['-set', f's3270.detailTrace={spec}']
        s3270 = Popen(vgwrap(args), stdin=DEVNULL, stdout=DEVNULL, env=env)
        self.children.append(s3270)
        ts.close()
        self.check_listen(http_port)

        # Stop s3270.
        self.get(f'http://127.0.0.1:{http_port}/3270/rest/json/Quit(-force))')

        # Check the tracefile.
        with open(tracefile, 'r') as file:
            lines = file.readlines()
        os.unlink(tracefile)
        addInput = [line for line in lines if 'AddInput' in line]
        if success:
            self.assertGreater(len(addInput), 0)
        else:
            self.assertEqual(len(addInput), 0)

        # Wait for the processes to exit.
        self.vgwait(s3270)

    def test_s3270_detail_trace_env(self):
        self.s3270_detail_trace(env=True)
        self.s3270_detail_trace(env=True, spec='1')
        self.s3270_detail_trace(env=True, spec='all')
        self.s3270_detail_trace(env=True, spec='a:sched:b')
        self.s3270_detail_trace(env=True, spec='foo', success=False)
    def test_s3270_detail_trace_resource(self):
        self.s3270_detail_trace(env=False)
        self.s3270_detail_trace(env=False, spec='1')
        self.s3270_detail_trace(env=False, spec='all')
        self.s3270_detail_trace(env=False, spec='a:sched:b')
        self.s3270_detail_trace(env=False, spec='foo', success=False)

if __name__ == '__main__':
    unittest.main()
