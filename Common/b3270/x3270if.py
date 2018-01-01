#!/usr/bin/env python
# Simple Python version of x3270if

import os
import socket
import sys

# x3270if class.
class x3270if:
    def __init__(self,debug=False):
        self.quoteChars = '\\"'
        self.badChars = self.quoteChars + ' ,()'

        # Debug flag
        self.debug = debug

        # Socket or files
        self.socket = None
        port = os.getenv('X3270PORT')
        if (port != None):
            self.socket = socket.create_connection(['127.0.0.1',int(port)])
            self.to3270 = self.socket.makefile('w', encoding='utf-8')
            self.from3270 = self.socket.makefile('r', encoding='utf-8')
            self.Debug('Connected')
        else:
            infd = os.getenv('X3270INPUT');
            outfd = os.getenv('X3270OUTPUT');
            if (infd == None or outfd == None):
                raise Exception("Don't know what to connect to")
            self.to3270 = os.fdopen(int(infd), 'wt', encoding='utf-8')
            self.from3270 = os.fdopen(int(outfd), 'rt', encoding='utf-8')
            self.Debug('Pipes connected')

        # Last prompt
        self.Prompt = ""

    def __del__(self):
        if (self.socket != None):
            self.socket.close()
        self.Debug('Deleted')

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
                argstr = cmd[0] + "("
                # Iterate over everything else.
                skip = True
                comma = ""
                for arg in cmd:
                    if (not skip):
                        argstr += comma + self.Quote(str(arg))
                        comma = ","
                    skip = False
                argstr += ")"
        else:
            # Iterate over the arguments.
            argstr = cmd + "("
            comma = ""
            for arg in args:
                argstr += comma + self.Quote(str(arg))
                comma = ","
            argstr += ")"
        self.to3270.write(argstr + '\n')
        self.to3270.flush()
        self.Debug('Sent ' + argstr)
        result = ""
        prev = ""
        while (True):
            text = self.from3270.readline().rstrip('\n')
            self.Debug("Got " + text)
            if (text == "ok"):
                self.Prompt = prev
                break
            if (text == "error"):
                self.Prompt = prev
                raise Exception(result)
            if (result == ""):
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
        for ch in self.badChars:
            if (ch in arg):
                anyBad = True
                break
        if (not anyBad):
            return arg
        ret = ""
        for ch in arg:
            if (ch in self.quoteChars):
                ret += '\\' + ch
            else:
                ret += ch
        return '"' + ret + '"'

    # Debug output.
    def Debug(self,text):
        if (self.debug):
            sys.stderr.write("[33m" + text + "[0m\n")
