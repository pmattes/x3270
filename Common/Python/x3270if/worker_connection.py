#!/usr/bin/env python3
# Simple Python version of x3270if

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
