#!/usr/bin/env python3
# Simple Python version of x3270if

"""Python interface to the x3270 family"""

import io
import os
import socket
import sys
import subprocess
import time

_backslashChars = '\\"'
_quoteChars = _backslashChars + ' ,()'

def Quote(arg):
    """Quote an argument in action syntax"""
    # Backslashes and double quotes need backslashes in front of them.
    # Other syntax markers (space, comma, paren) just need the argument in
    #  double quotes.
    if (not any(ch in arg for ch in _quoteChars)): return arg
    return '"' + ''.join('\\' + ch if ch in _backslashChars else ch for ch in arg) + '"'

class _x3270if():
    """Abstract x3270if base class"""
    def __init__(self,debug=False):

        # Debug flag
        self._debug = debug

        # Last prompt
        self._prompt = ''

        # File streams to/from the emulator
        self._to3270 = None
        self._from3270 = None

    def __del__(self):
        self.Debug('_x3270if deleted')

    @property
    def prompt(self):
        """Gets the last emulator prompt"""
        return self._prompt

    def Run(self,cmd,*args):
        """Run method: Send an action to the emulator"""
        # Can pass just a string, which will be left untouched.
        # Can pass a command and arguments; the arguments will be quoted as
        #  needed and put in parentheses.
        # Can pass a list or a tuple; the first element will be treated as the
        #  command name and the remainder will be treated as the arguments.
        # Returns literal text from the emulator. Multi-line output is
        #  separated by newlines.
        # Raises an exception if the command fails.
        # First argument (if any) is the action name, others are arguments.
        if (args == ()):
            if (isinstance(cmd, str)):
                argstr = cmd
            else:
                argstr = cmd[0] + '(' + ','.join(Quote(arg) for arg in cmd[1:]) + ')'
        else:
            argstr = cmd + '(' + ','.join(Quote(arg) for arg in args) + ')'
        self._to3270.write(argstr + '\n')
        self._to3270.flush()
        self.Debug('Sent ' + argstr)
        result = ''
        prev = ''
        while (True):
            text = self._from3270.readline().rstrip('\n')
            if (text == ''): raise Exception('Emulator exited')
            self.Debug("Got '" + text + "'")
            if (text == 'ok'):
                self._prompt = prev
                break
            if (text == 'error'):
                self._prompt = prev
                raise Exception(result)
            if (result == ''): result = prev.lstrip('data: ')
            else: result = result + '\n' + prev.lstrip('data: ')
            prev = text
        return result

    def Debug(self,text):
        """Debug output"""
        if (self._debug): sys.stderr.write('[33m' + text + '[0m\n')

class Child(_x3270if):
    """x3270if child script class (script is a child of the emulator process)"""
    def __init__(self,debug=False):
        # Init the parent.
        _x3270if.__init__(self, debug)
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
            self.Debug('Connected')
            emulatorEncoding = self.Run('Query(LocalEncoding)')
            if (emulatorEncoding != 'UTF-8'):
                self._to3270 = socket.makefile('w', emulatorEncoding)
                self._from3270 = socket.makefile('r', emulatorEncoding)
        else:
            # Talk to pipe file descriptors.
            infd = os.getenv('X3270INPUT')
            outfd = os.getenv('X3270OUTPUT')
            if (infd == None or outfd == None):
                raise Exception("Don't know what to connect to")
            self._infd = int(infd)
            self._to3270 = io.open(self._infd, 'wt', encoding='utf-8',
                    closefd=False)
            self._outfd = int(outfd)
            self._from3270 = io.open(self._outfd, 'rt', encoding='utf-8',
                    closefd=False)
            self.Debug('Pipes connected')
            emulatorEncoding = self.Run('Query(LocalEncoding)')
            if (emulatorEncoding != 'UTF-8'):
                self._to3270 = io.open(self._infd, 'wt',
                        encoding=emulatorEncoding, closefd=False)
                self._from3270 = io.open(self._outfd, 'rt',
                        encoding=emulatorEncoding, closefd=False)

    def __del__(self):
        if (self._socket != None): self._socket.close();
        if (self._infd != -1): os.close(self._infd)
        if (self._outfd != -1): os.close(self._outfd)
        _x3270if.__del__(self)
        self.Debug('Child deleted')

class Peer(_x3270if):
    """x3270if peer script class (starts a copy of s3270)"""
    def __init__(self,debug=False,extra_args=[]):
        _x3270if.__init__(self, debug)
        self._socket = None
        self._s3270 = None

        # Create a temporary socket to find a unique local port.
        tempsocket = socket.socket()
        tempsocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        tempsocket.bind(('127.0.0.1', 0))
        port = tempsocket.getsockname()[1]
        self.Debug('Port is {0}'.format(port))

        # Create the child process.
        try:
            args = ['s3270' if os.name != 'nt' else 'ws3270.exe',
                    '-utf8',
                    '-minversion', '3.6',
                    '-scriptport', str(port),
                    '-scriptportonce'] + extra_args
            self._s3270 = subprocess.Popen(args,
                    stderr=subprocess.PIPE,universal_newlines=True)

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
                raise Exception(errmsg)

            self._to3270 = self._socket.makefile('w', encoding='utf-8')
            self._from3270 = self._socket.makefile('r', encoding='utf-8')
            self.Debug('Connected')
        finally:
            del tempsocket

    def __del__(self):
        if (self._s3270 != None): self._s3270.terminate()
        if (self._socket != None): self._socket.close();
        _x3270if.__del__(self)
        self.Debug('Peer deleted')
