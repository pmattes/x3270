#!/usr/bin/env python3
# Simple Python version of x3270if

import io
import os
import socket
import sys
import subprocess
import time

# Abstract x3270if base class.
class _x3270if():
    def __init__(self,debug=False):
        self._quoteChars = '\\"'
        self._badChars = self._quoteChars + ' ,()'

        # Debug flag
        self._debug = debug

        # Last prompt
        self._prompt = ''

    def __del__(self):
        self.Debug('_x3270if deleted')

    @property
    def prompt(self):
        return self._prompt

    # Run method.
    #
    # Can pass just a string, which will be left untouched.
    # Can pass a command and arguments; the arguments will be quoted as needed
    #  and put in parentheses.
    # Can pass a list or a tuple; the first element will be treated as the
    #  command name and the remainder will be treated as the arguments.
    # Returns literal text from the emulator. Multi-line output is separated
    #  by newlines.
    # Raises an exception if the command fails.
    def Run(self,cmd,*args):
        # First argument (if any) is the action name, others are arguments.
        if (args == ()):
            if (isinstance(cmd, str)):
                # Just one argument, presumably already formatted.
                argstr = cmd
            else:
                # Isolate the first element, the command.
                argstr = cmd[0] + '('
                # Iterate over everything else.
                skip = True
                comma = ''
                for arg in cmd:
                    if (not skip):
                        argstr += comma + self.Quote(str(arg))
                        comma = ','
                    skip = False
                argstr += ')'
        else:
            # Iterate over the arguments.
            argstr = cmd + '('
            comma = ''
            for arg in args:
                argstr += comma + self.Quote(str(arg))
                comma = ','
            argstr += ')'
        self._to3270.write(argstr + '\n')
        self._to3270.flush()
        self.Debug('Sent ' + argstr)
        result = ''
        prev = ''
        while (True):
            text = self._from3270.readline().rstrip('\n')
            if (text == ''):
                raise Exception('Emulator exited')
            self.Debug("Got '" + text + "'")
            if (text == 'ok'):
                self._prompt = prev
                break
            if (text == 'error'):
                self._prompt = prev
                raise Exception(result)
            if (result == ''):
                result = prev.lstrip('data: ')
            else:
                result = result + '\n' + prev.lstrip('data: ')
            prev = text
        return result

    # Argument quoting.
    # Backslashes and double quotes need backslashes in front of them.
    # Other syntax markers (space, comma, paren) just need the argument in
    #  double quotes.
    def Quote(self,arg):
        anyBad = False
        for ch in self._badChars:
            if (ch in arg):
                anyBad = True
                break
        if (not anyBad):
            return arg
        ret = ''
        for ch in arg:
            if (ch in self._quoteChars):
                ret += '\\' + ch
            else:
                ret += ch
        return '"' + ret + '"'

    # Debug output.
    def Debug(self,text):
        if (self._debug):
            sys.stderr.write('[33m' + text + '[0m\n')

# x3270if child script class.
class Child(_x3270if):
    def __init__(self,debug=False):
        # Init the parent.
        self.socket = None
        self.infd = -1
        self.outfd = -1
        _x3270if.__init__(self, debug)

        # Socket or pipes
        port = os.getenv('X3270PORT')
        if (port != None):
            self.socket = socket.create_connection(['127.0.0.1',int(port)])
            self._to3270 = self.socket.makefile('w', encoding='utf-8')
            self._from3270 = self.socket.makefile('r', encoding='utf-8')
            self.Debug('Connected')
            emulatorEncoding = self.Run('Query(LocalEncoding)')
            if (emulatorEncoding != 'UTF-8'):
                self._to3270 = socket.makefile('w', emulatorEncoding)
                self._from3270 = socket.makefile('r', emulatorEncoding)
        else:
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
        if (self.socket != None):
            self.socket.close();
        if (self._infd != -1):
            os.close(self._infd)
        if (self._outfd != -1):
            os.close(self._outfd)
        _x3270if.__del__(self)
        self.Debug('Child deleted')

# x3270if peer script class (starts s3270).
class Peer(_x3270if):
    def __init__(self,debug=False,extra_args=[]):
        # Init the parent.
        self.socket = None
        self.s3270 = None
        _x3270if.__init__(self, debug)

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
            self.s3270 = subprocess.Popen(args,
                    stderr=subprocess.PIPE,universal_newlines=True)

            # It might take a couple of tries to connect, as it takes time to
            # start the process. We wait a maximum of half a second.
            tries = 0
            connected = False
            while (tries < 5):
                try:
                    self.socket = socket.create_connection(['127.0.0.1', port])
                    connected = True
                    break
                except:
                    time.sleep(0.1)
                    tries += 1
            if (not connected):
                errmsg = 'Could not connect to emulator'
                self.s3270.terminate()
                r = self.s3270.stderr.readline().rstrip('\r\n')
                if (r != ''):
                    errmsg += ': ' + r
                raise Exception(errmsg)

            self._to3270 = self.socket.makefile('w', encoding='utf-8')
            self._from3270 = self.socket.makefile('r', encoding='utf-8')
            self.Debug('Connected')
        finally:
            del tempsocket

    def __del__(self):
        if (self.s3270 != None):
            self.s3270.terminate()
        if (self.socket != None):
            self.socket.close();
        _x3270if.__del__(self)
        self.Debug('Peer deleted')
