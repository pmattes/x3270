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
# b3270 JSON pass-through tests

import json
from subprocess import Popen, PIPE
import unittest

from Common.Test.cti import *
import Common.Test.pipeq as pipeq

class TestB3270PassthruJson(cti):

    # b3270 passthru Json test
    def test_b3270_passthru_json(self):

        b3270 = Popen(vgwrap(['b3270', '-json']), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)

        # Get the initial dump.
        b3270.stdout.readline().decode('utf8')

        # Register the Foo action and run it.
        b3270.stdin.write(b'{"register":{"name":"Foo"}}\n')
        b3270.stdin.flush()
        b3270.stdin.write(b'{"run":{"r-tag":"abc","actions":{"action":"Foo","args":["a","b"]}}}\n')
        b3270.stdin.flush()

        # Get the result.
        pq = pipeq.pipeq(self, b3270.stdout)
        out = json.loads(pq.get(2, 'b3270 did not produce expected output').decode('utf8'))
        self.assertTrue('passthru' in out)
        passthru = out['passthru']
        self.assertTrue('action' in passthru)
        action = passthru['action']
        self.assertEqual('Foo',action)
        self.assertTrue('p-tag' in passthru)
        p_tag = passthru['p-tag']
        self.assertTrue('parent-r-tag' in passthru)
        self.assertEqual('abc', passthru['parent-r-tag'])
        self.assertTrue('args' in passthru)
        self.assertEqual(['a','b'], passthru['args'])

        # Make the action succeed.
        succeed = { "succeed": { "p-tag": p_tag, "text": [ "hello", "there" ] } }
        b3270.stdin.write(json.dumps(succeed).encode('utf8') + b'\n')
        b3270.stdin.flush()

        # Get the result of that.
        out = json.loads(pq.get(2, 'b3270 did not produce expected output').decode('utf8'))
        self.assertTrue('run-result' in out)
        run_result = out['run-result']
        self.assertTrue('r-tag' in run_result)
        self.assertEqual('abc', run_result['r-tag'])
        self.assertTrue('success' in run_result)
        self.assertTrue(run_result['success'])
        self.assertTrue('text' in run_result)
        self.assertEqual(['hello', 'there'], run_result['text'])

        # Wait for the process to exit.
        b3270.stdin.close()
        b3270.stdout.close()
        self.vgwait(b3270)
        pq.close()

if __name__ == '__main__':
    unittest.main()
