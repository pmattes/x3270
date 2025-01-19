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
# c3270 RPQNAMES tests

import os
import sys
if not sys.platform.startswith('win'):
    import pty
import threading
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.rpq as rpq

@unittest.skipIf(sys.platform == "darwin", "Not ready for c3270 graphic tests")
@unittest.skipIf(sys.platform.startswith('win'), "Windows uses different c3270 graphic tests")
@requests_timeout
class TestC3270RpqNames(cti):

    # Asynchronous connect from c3270 to playback.
    def async_connect(self, c3270_port: int, playback_port: int):
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Connect(c:127.0.0.1:{playback_port})')
        self.assertTrue(r.ok)

    # c3270 RPQNAMES test
    def c3270_rpqnames(self, rpq: str, reply: str):

        playback_port, pts = unused_port()

        # Fork a child process with a PTY between this process and it.
        c3270_port, cts = unused_port()
        (pid, fd) = pty.fork()
        if pid == 0:
            # Child process
            cts.close()
            env = os.environ.copy()
            env['TERM'] = 'xterm-256color'
            os.execvpe(vgwrap_ecmd('c3270'),
                vgwrap_eargs(['c3270', '-model', '2', '-utf8',
                    '-httpd', f'127.0.0.1:{c3270_port}', '-secure']), env)
            self.assertTrue(False, 'c3270 did not start')

        # Parent process.

        # Make sure c3270 started.
        self.check_listen(c3270_port)
        cts.close()

        # Start 'playback' to feed c3270.
        p = playback(self, 's3270/Test/rpqnames.trc', port=playback_port)
        pts.close()

        # Connect c3270 to playback. This has to be async, because the 'Connect()' action blocks until the
        # connection is complete, and the thing that completes it is the playback send_to_mark call.
        thread = threading.Thread(target=self.async_connect, args=[c3270_port, playback_port])
        thread.start()

        # Write to the mark in the trace and discard whatever comes back until this point.
        p.send_to_mark()
        thread.join()

        # Set the rpq value.
        r = self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Set(rpq,{rpq})')
        self.assertTrue(r.ok)

        # Send the Query WSF.
        p.send_records(1, send_tm=False)

        # Get the response.
        ret = p.send_tm()
        p.close()

        # Parse the response.
        prefix = ret[:10]
        self.assertTrue(prefix.startswith('88')) # QueryReply
        self.assertTrue(prefix.endswith('81a1')) # RPQ names
        ret = ret[10:]
        self.assertTrue(ret.endswith('ffeffffc06')) # IAC EOR IAC WONT TM
        ret = ret[:-10]
        self.assertEqual(reply, ret)

        # Wait for c3270 to exit.
        self.get(f'http://127.0.0.1:{c3270_port}/3270/rest/json/Quit()')
        self.vgwait_pid(pid)

    # User override in EBCDIC
    def test_c3270_rpqnames_user_override_ebcdic(self):
        user = 'bob'
        self.c3270_rpqnames(f'USER={user}', rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))))

if __name__ == '__main__':
    unittest.main()
