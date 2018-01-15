#!/usr/bin/env python3
# Simple Python version of x3270if

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
    def __init__(self,debug=False,extra_args=[]):
        """Initialize the object.

           Args:
              debug (bool): True to log debug information to stderr.
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
            args = ['s3270' if os.name != 'nt' else 'ws3270.exe',
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
                if (os.name == 'nt'):
                    oserr += ' (ws3270.exe)'

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
