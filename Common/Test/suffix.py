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
# session file suffix test framework

from subprocess import Popen, PIPE, DEVNULL
import tempfile
import os
import sys
import requests
import Common.Test.cti as cti

# session file suffix test
def suffix_test(ti, program, suffix, children):
    # Create a session file.
    (handle, file) = tempfile.mkstemp(suffix=suffix)
    bprogram = program.encode('utf8')
    os.write(handle, bprogram + b'.termName: foo\n')
    pfx2 = b'w' + bprogram if sys.platform.startswith('win') else bprogram
    os.write(handle, pfx2 + b'.model: 3279-3-E\n')
    os.close(handle)

    # Start the emulator.
    port, ts = cti.unused_port()
    emu = Popen(cti.vgwrap([program, '-httpd', str(port), file]), stdout=DEVNULL)
    children.append(emu)
    cti.cti.check_listen(ti, port)
    ts.close()

    # Check the output.
    r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(TerminalName)')
    j = r.json()
    ti.assertEqual('foo', j['result'][0], 'Expecting "foo" as the terminal name')
    r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Set(model)')
    j = r.json()
    ti.assertEqual('3279-3-E', j['result'][0], 'Expecting 3279-3-E as the model')

    # Wait for the process to exit.
    requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
    cti.cti.vgwait(ti, emu)
    os.unlink(file)
