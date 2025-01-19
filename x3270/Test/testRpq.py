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
# x3270 RPQNAMES tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.rpq as rpq

class TestX3270RpqNames(cti):

    # x3270 RPQNAMES test
    def x3270_rpqnames(self, rpq: str, reply: str, stderr_count=0):

        # Start x3270.
        x3270 = Popen(vgwrap(['x3270', '-script']), stdin=PIPE, stdout=DEVNULL, stderr=PIPE)
        self.children.append(x3270)

        # Start 'playback' to read x3270's output.
        port, ts = unused_port()
        with playback(self, 's3270/Test/rpqnames.trc', port=port) as p:
            ts.close()

            # Connect x3270 to playback.
            x3270.stdin.write(f'Set(rpq,{rpq})\n'.encode())
            x3270.stdin.write(f'Open(c:127.0.0.1:{port})\n'.encode())
            x3270.stdin.flush()

            # Write to the mark in the trace and discard whatever comes back until this point.
            p.send_to_mark()

            # Send the Query WSF.
            p.send_records(1, send_tm=False)

            # Get the response.
            ret = p.send_tm()

            # Parse the response.
            prefix = ret[:10]
            self.assertTrue(prefix.startswith('88')) # QueryReply
            self.assertTrue(prefix.endswith('81a1')) # RPQ names
            ret = ret[10:]
            self.assertTrue(ret.endswith('ffeffffc06')) # IAC EOR IAC WONT TM
            ret = ret[:-10]
            self.assertEqual(reply, ret)

        # Wait for x3270 to exit.
        x3270.stdin.write(b"Quit()\n")
        x3270.stdin.flush()
        x3270.stdin.close()
        self.vgwait(x3270)

        # Verify stderr.
        stderr = x3270.stderr.readlines()
        x3270.stderr.close()
        self.assertEqual(stderr_count, len(stderr))

    # User override in EBCDIC
    def test_x3270_rpqnames_user_override_ebcdic(self):
        user = 'bob'
        self.x3270_rpqnames(f'USER={user}', rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))))

if __name__ == '__main__':
    unittest.main()
