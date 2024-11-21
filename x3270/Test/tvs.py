#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# x3270 tightvncserver support class

import os
import stat

import Common.Test.cti as cti

def tightvncserver_test():
    '''Tests for the presence of tightvnscerver'''
    return os.system('tightvncserver --help 2>/dev/null') == 65280

class tightvncserver:
    def __init__(self, cti: cti.cti):
        # The password file needs to be 0600 or Vnc will prompt for it again.
        os.chmod('x3270/Test/vnc/.vnc/passwd', stat.S_IREAD | stat.S_IWRITE)
        self.cwd=os.getcwd()
        # Set HOME and USER to avoid any interaction with the current user's environment.
        # Set SSH_CONNECTION to keep the VirtualBox extensions from starting in the tightvncserver.
        # Unset all of the environment variables that GNOME, XDG and SSH use.
        unset = 'unset ' + ' '.join([x for x in os.environ.keys() if x.startswith('XDG') or x.startswith('GNOME') or x.startswith('SSH')])
        cmd = f'{unset}; HOME={self.cwd}/x3270/Test/vnc USER=foo SSH_CONNECTION=foo tightvncserver :2 2>/dev/null'
        cti.assertEqual(0, os.system(cmd))
        cti.check_listen(5902)

        # As of Ubuntu 24.04, Xvncserver returns garbage the first time a client asks for font info.
        cti.try_until(lambda: os.system('xlsfonts -display :2 -ll -fn "-*-helvetica-bold-r-normal--14-*-100-100-p-*-iso8859-1" >/dev/null 2>&1'), 2, 'Xvncserver is not sane')

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.close()

    def close(self):
        os.system(f'HOME={self.cwd}/x3270/Test/vnc tightvncserver -kill :2 2>/dev/null')

    def __del__(self):
        self.close()