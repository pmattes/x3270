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

_quote_chars = ' ,()'

def quote(arg):
    """Quote an argument in action syntax

       Args:
          arg (str): Argument to format
       Returns:
          str: Formatted argument
    """

    # If the argument is empty, contains a space, comma or paren, or starts
    # with a double quote, it needs to be put in double quotes. (For an empty
    # argument it is not strictly necessary, but it makes traces easier to
    # read.)
    if (not (arg == '' or any(ch in arg for ch in _quote_chars) or arg.startswith('"'))):
        return arg

    # A double quote inside the argument needs a backslash in front of it.
    # A backslash at the end of the argument needs to be doubled.
    x = arg.replace('"', '\\"')
    if (x.endswith('\\')): x = x + '\\'
    return '"' + x + '"'

class ActionFailException(Exception):
    """x3270if action failure"""
    def __init__(self,msg):
        Exception.__init__(self,msg)

class StartupException(RuntimeError):
    """x3270if unable to start s3270"""
    def __init__(self,msg):
        RuntimeError.__init__(self,msg)

class _session():
    """Abstract x3270if session base class"""
    def __init__(self,debug=False):
        """Initialize an instance

           Args:
              debug (bool): True to trace debug info to stderr.
       """

        # Debug flag
        self._debug_enabled = debug

        # Last prompt
        self._prompt = ''

        # File streams to/from the emulator
        self._to3270 = None
        self._from3270 = None

    def __del__(self):
        self._debug('_session deleted')

    @property
    def prompt(self):
        """Gets the last emulator prompt
           str: Last emulator prompt

        """
        return self._prompt

    def run_action(self,cmd,*args):
        """Send an action to the emulator

           Args:
              cmd (str): Action name
                 Action name. If 'args' is omitted, this is the entire
                 properly-formatted action name and arguments, and the text
                 will be passed through unmodified.
              args (iterable): Arguments
           Returns:
              str: Command output
                 Mulitiple lines are separated by newline characters.
           Raises:
              ActionFailException: Emulator returned an error.
              EOFError: Emulator exited unexpectedly.
        """
        if (not isinstance(cmd, str)):
            raise TypeError("First argument must be a string")
        self._debug("args is {0}, len is {1}".format(args, len(args)))
        if (args == ()):
            argstr = cmd
        elif (len(args) == 1 and not isinstance(args[0], str)):
            # One argument that can be iterated over.
            argstr = cmd + '(' + ','.join(quote(str(arg)) for arg in args[0]) + ')'
        else:
            # Multiple arguments.
            argstr = cmd + '(' + ','.join(quote(str(arg)) for arg in args) + ')'
        self._to3270.write(argstr + '\n')
        self._to3270.flush()
        self._debug('Sent ' + argstr)
        result = ''
        prev = ''
        while (True):
            text = self._from3270.readline().rstrip('\n')
            if (text == ''): raise EOFError('Emulator exited')
            self._debug("Got '" + text + "'")
            if (text == 'ok'):
                self._prompt = prev
                break
            if (text == 'error'):
                self._prompt = prev
                raise ActionFailException(result)
            if (result == ''): result = prev.lstrip('data: ')
            else: result = result + '\n' + prev.lstrip('data: ')
            prev = text
        return result

    def _debug(self,text):
        """Debug output

           Args:
              text (str): Text to log. A Newline will be added.
        """
        if (self._debug_enabled):
            if os.name != 'nt':
                sys.stderr.write('[33m')
            sys.stderr.write(text)
            if os.name != 'nt':
                sys.stderr.write('[0m')
            sys.stderr.write('\n')
