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
# s3270 extendedDataStream tests

import unittest
from subprocess import Popen
import os

from Common.Test.cti import *

@requests_timeout
class TestS3270ExtendedDataStream(cti):

    # s3270 extended data stream test
    def test_s3270_extended_data_stream(self):

        # Start s3270.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'{port}']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Set the model with just a digit.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model,3) Set(model) Show(TerminalName)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(2, len(result))
        self.assertEqual('3279-3-E', result[0])
        self.assertEqual('IBM-3279-3-E', result[1])

        # Change extendedDataStream and make sure the model and terminal name change.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(extendedDataStream,false) Set(model) Show(TerminalName)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(2, len(result))
        self.assertEqual('3279-3', result[0])
        self.assertEqual('IBM-3279-3', result[1])

        # Change extendedDataStream back and make sure the model and terminal name change back.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(extendedDataStream,true) Set(model) Show(TerminalName)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(2, len(result))
        self.assertEqual('3279-3-E', result[0])
        self.assertEqual('IBM-3279-3-E', result[1])

        # Clear extendedDataStream, then set the model explicitly with -E, and make sure it disappears.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(extendedDataStream,false,model,3279-3-E) Set(model) Show(TerminalName)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(2, len(result))
        self.assertEqual('3279-3', result[0])
        self.assertEqual('IBM-3279-3', result[1])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 mixed-case test model test
    def test_s3270_mixed_case_model(self):

        # Start s3270 with a mixed-case IBM- model on the command line.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'{port}', '-model', 'iBm-3279-2']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Verify the model is right.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(1, len(result))
        self.assertEqual('3279-2-E', result[0])

        # Try a model name with a different-cased IBM- at the front and a lowercase -E.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model,IbM-3278-3-e) Set(model)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(1, len(result))
        self.assertEqual('3278-3-E', result[0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    
    # s3270 mixed-case oversize test
    def test_s3270_mixed_case_oversize(self):

        # Start s3270 with an uppercase and leading-zero oversize on the command line.
        port, ts = unused_port()
        s3270 = Popen(vgwrap(['s3270', '-httpd', f'{port}', '-oversize', '0100X100']))
        self.children.append(s3270)
        self.check_listen(port)
        ts.close()

        # Verify oversize is right.
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(oversize)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(1, len(result))
        self.assertEqual('100x100', result[0])

        # Try an uppercase-X oversize with a Set().
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(oversize,99X99) Set(oversize)')
        s = r.json()
        self.assertTrue(r.ok)
        result = r.json()['result']
        self.assertEqual(1, len(result))
        self.assertEqual('99x99', result[0])

        # Wait for the process to exit.
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        self.vgwait(s3270)

if __name__ == '__main__':
    unittest.main()
