#!/usr/bin/env python3
#
# Copyright (c) 2021-2026 Paul Mattes.
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
# x3270 Xvfb support class

import os
import shutil
import socket
from subprocess import Popen, DEVNULL, run
import tempfile

import Common.Test.cti as cti

def server_test():
    '''Tests for the presence of the server'''
    return shutil.which('Xvfb') != None

no_server = 'Need Xvfb'

class Xvfb:
    def get_fp(self, display: str) -> str:
        '''Get the font list from the specified display'''
        xset_out = run(['xset', '-display', display, 'q'], capture_output=True, text=True, check=False).stdout.split('\n')
        ix = xset_out.index('Font Path:')
        return xset_out[ix + 1].strip()

    def default_Xvfb_fp(self, port: int, cti: cti.cti) -> str:
        ''' Get the default font path for Xvfb'''
        display = f':{port}'
        server = Popen(['Xvfb', '-listen', 'tcp', display], stdout=DEVNULL, stderr=DEVNULL, env=self.Env)
        cti.check_listen(6000 + port)
        fp = self.get_fp(display).removesuffix(',built-ins')
        server.terminate()
        server.wait()
        return fp

    def __init__(self, cti: cti.cti, need_fonts = False):
        self.cti = cti
        self.Display = None
        self.home_object = tempfile.TemporaryDirectory()
        self.Home = self.home_object.name
        self.User = 'foo'
        self.server = None
        self.added_font = False
        self.obj_dir = os.path.abspath(os.path.split(shutil.which('x3270'))[0])

        # Set up an environment to make Xvfb safe.
        # Set HOME and USER to avoid any interaction with the current user's environment.
        # Set SSH_CONNECTION to keep the VirtualBox extensions from starting.
        # Unset all of the environment variables that GNOME, XDG and SSH use.
        self.Env = os.environ.copy()
        for key in list(self.Env.keys()):
            if key.startswith('XDG') or key.startswith('GNOME') or key.startswith('SSH'):
                self.Env.pop(key)
        self.Env['HOME'] = self.Home
        self.Env['USER'] = self.User
        self.Env['SSH_CONNECTION'] = 'foo'

        # If they want to run locally, stop here, while possibly adding fonts.
        if 'LOCALFB' in os.environ:
            self.Display = os.environ['DISPLAY']
            if need_fonts:
                if not self.obj_dir in self.get_fp(self.Display):
                    cti.assertEqual(0, os.system(f'xset fp+ {self.obj_dir}'))
                    cti.assertEqual(0, os.system(f'xset fp rehash'))
                    self.added_font = True
            return

        # Find a free port.
        found = False
        for port in range(2, 11):
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect(('127.0.0.1', 6000 + port))
            except:
                found = True
                break
        cti.assertTrue(found, 'Could not find free display port')
        self.Display = f':{port}'

        # Set up the font path. It turns out that Xvfp does not actually allow its
        # font path to be changed at runtime. The 'xset' command appears to work, but
        # has no effect.
        # The workaround is to add the x3270 directory to the path when starting
        # Xvfb. That means we need to learn the default path, which means starting it
        # once just to do that.
        if need_fonts:
            fp = self.default_Xvfb_fp(port, cti) + ',' + self.obj_dir
            fp_arg = ['-fp', fp]
        else:
            fp_arg = []

        # Start the server.
        self.server = Popen(['Xvfb'] + fp_arg + ['-listen', 'tcp', self.Display], stdout=DEVNULL, stderr=DEVNULL, env=self.Env)
        cti.check_listen(6000 + port)

        self.Env['DISPLAY'] = self.Display

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.close()

    def close(self):
        if self.server:
            self.server.terminate()
            self.server.wait()
            self.server = None
        self.home_object.cleanup()
        if self.added_font:
            self.cti.assertEqual(0, os.system(f'xset fp- {self.obj_dir}'))
            self.cti.assertEqual(0, os.system(f'xset fp rehash'))
            self.added_font = False

    def __del__(self):
        self.close()
