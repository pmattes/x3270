#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Paul Mattes nor his contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES OR HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# tcl3270-pkg smoke tests

import filecmp
import os
import sys
from subprocess import Popen, DEVNULL
import tempfile
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback


@unittest.skipIf(sys.platform == "darwin", "macOS does not like tcl")
class TestTcl3270PackageSmoke(cti):

    # tcl3270-pkg Tcl wrapper smoke test
    def test_tcl3270_pkg_smoke(self):

        # Start 'playback' to feed data to the Tcl wrapper.
        playback_port, ts = unused_port()
        with playback(self, 's3270/Test/ibmlink.trc', port=playback_port) as p:
            ts.close()

            # Create a temporary file.
            (handle, name) = tempfile.mkstemp()
            os.close(handle)

            # Start the Tcl wrapper.
            tcl3270_pkg = Popen(vgwrap(["tclsh",
                "tcl3270-pkg/Test/smoke-tcl3270-pkg.tcl", name,
                f"127.0.0.1:{playback_port}"]),
                stdin=DEVNULL, stdout=DEVNULL)
            self.children.append(tcl3270_pkg)

            # Send a screenful to s3270.
            p.send_records(4)

            # Wait for the file to show up.
            def Test():
                return os.path.getsize(name) > 0
            self.try_until(Test, 2, "Tcl wrapper did not produce a file")

        # Wait for the Tcl wrapper to exit.
        self.children.remove(tcl3270_pkg)
        self.vgwait(tcl3270_pkg)

        # Compare the files.
        self.assertTrue(filecmp.cmp(name, 'tcl3270/Test/smoke.txt'))
        os.unlink(name)


if __name__ == '__main__':
    unittest.main()
