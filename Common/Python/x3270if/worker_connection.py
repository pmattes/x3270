#!/usr/bin/env python3
# Simple Python version of x3270if
#
# Copyright (c) 2017-2018 Paul Mattes.
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

"""Python interface to x3270 emulators"""

import io
import os
import socket
import sys

from x3270if.common import _session
from x3270if.common import StartupException

class worker_connection(_session):
    """Connection to the emulator from a worker script invoked via the Script() action"""
    def __init__(self,debug=False):
        """Initialize the object.

           Args:
              debug (bool): True to log debug information to stderr.

           Raises:
              StartupException: Insufficient information in the environment to
              connect to the emulator.
        """
        _session.__init__(self, debug)
        self._socket = None
        self._infd = -1
        self._outfd = -1

        # Socket or pipes
        port = os.getenv('X3270PORT')
        if (port != None):
            # Connect to a TCP port.
            self._socket = socket.create_connection(['127.0.0.1',int(port)])
            self._to3270 = self._socket.makefile('w', encoding='utf-8')
            self._from3270 = self._socket.makefile('r', encoding='utf-8')
            self._debug('Connected')
            emulator_encoding = self.run_action('Query(LocalEncoding)')
            if (emulator_encoding != 'UTF-8'):
                self._to3270 = self._socket.makefile('w',
                        encoding=emulator_encoding)
                self._from3270 = self._socket.makefile('r',
                        encoding=emulator_encoding)
        else:
            # Talk to pipe file descriptors.
            infd = os.getenv('X3270INPUT')
            outfd = os.getenv('X3270OUTPUT')
            if (infd == None or outfd == None):
                raise StartupException("No X3270PORT, X3270INPUT or X3270OUTPUT defined")
            self._infd = int(infd)
            self._to3270 = io.open(self._infd, 'wt', encoding='utf-8',
                    closefd=False)
            self._outfd = int(outfd)
            self._from3270 = io.open(self._outfd, 'rt', encoding='utf-8',
                    closefd=False)
            self._debug('Pipes connected')
            emulator_encoding = self.run_action('Query(LocalEncoding)')
            if (emulator_encoding != 'UTF-8'):
                self._to3270 = io.open(self._infd, 'wt',
                        encoding=emulator_encoding, closefd=False)
                self._from3270 = io.open(self._outfd, 'rt',
                        encoding=emulator_encoding, closefd=False)

    def __del__(self):
        if (self._socket != None): self._socket.close();
        if (self._infd != -1): os.close(self._infd)
        if (self._outfd != -1): os.close(self._outfd)
        _session.__del__(self)
        self._debug('worker_connection deleted')
