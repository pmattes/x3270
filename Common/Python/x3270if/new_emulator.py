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
import subprocess
import time

from x3270if.common import _session
from x3270if.common import StartupException

class new_emulator(_session):
    """Starts a new copy of s3270"""
    def __init__(self,debug=False,emulator=None,extra_args=[]):
        """Initialize the object.

           Args:
              debug (bool): True to log debug information to stderr.
              emulator (str): Name of the emulator to start, defaults to s3270
              extra_args(list of str, optional): Extra arguments
                 to pass in the s3270 command line.
           Raises:
              StartupException: Unable to start s3270.
        """
        _session.__init__(self, debug)
        self._socket = None
        self._s3270 = None

        # Create a temporary socket to find a unique local port.
        tempsocket = socket.socket()
        tempsocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        tempsocket.bind(('127.0.0.1', 0))
        port = tempsocket.getsockname()[1]
        self._debug('Port is {0}'.format(port))

        # Create the child process.
        try:
            args = ['s3270',
                    '-utf8',
                    '-minversion', '3.6',
                    '-scriptport', str(port),
                    '-scriptportonce'] + extra_args
            oserr = None
            try:
                self._s3270 = subprocess.Popen(args,
                        stderr=subprocess.PIPE,universal_newlines=True)
            except OSError as err:
                oserr = str(err)
            if (oserr != None): raise StartupException(oserr)

            # It might take a couple of tries to connect, as it takes time to
            # start the process. We wait a maximum of half a second.
            tries = 0
            connected = False
            while (tries < 5):
                try:
                    self._socket = socket.create_connection(['127.0.0.1', port])
                    connected = True
                    break
                except:
                    time.sleep(0.1)
                    tries += 1
            if (not connected):
                errmsg = 'Could not connect to emulator'
                self._s3270.terminate()
                r = self._s3270.stderr.readline().rstrip('\r\n')
                if (r != ''): errmsg += ': ' + r
                raise StartupException(errmsg)

            self._to3270 = self._socket.makefile('w', encoding='utf-8')
            self._from3270 = self._socket.makefile('r', encoding='utf-8')
            self._debug('Connected')
        finally:
            del tempsocket

    def __del__(self):
        if (self._s3270 != None): self._s3270.terminate()
        if (self._socket != None): self._socket.close();
        _session.__del__(self)
        self._debug('new_emulator deleted')
