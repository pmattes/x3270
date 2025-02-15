#!/usr/bin/env python3
#
# Copyright (c) 2021-2025 Paul Mattes.
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
# s3270 TELNET NEW-ENVIRON tests

import enum
import os
from subprocess import Popen, PIPE, DEVNULL
import tempfile
from typing import Any
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

class user_method(enum.Enum):
    resource = enum.auto()
    user_env = enum.auto()
    username_env = enum.auto()
    uri = enum.auto()
    UNKNOWN = enum.auto()

class TestS3270NewEnviron(cti):

    def s3270_elf(self, file: str, applid=None):
        '''s3270 NEW-ENVIRON ELF test'''

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, f's3270/Test/{file}.trc', port=port) as p:
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            if applid != None:
                env['IBMAPPLID'] = applid
            else:
                if 'IBMAPPLID' in env:
                    del env['IBMAPPLID']
            s3270 = Popen(vgwrap(['s3270', f'a:c:127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

            # Feed s3270 some actions.
            s3270.stdin.write(b'Quit()\n')
            s3270.stdin.flush()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    # s3270 TELNET NEW-ENVIRON ELF test
    def test_s3270_elf_set(self):
        self.s3270_elf('elf', '12345')
    def test_s3270_elf_default(self):
        self.s3270_elf('elfDefault')

    def s3270_empty_user(self, user: user_method):
        '''s3270 NEW-ENVIRON empty USER test'''

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, f's3270/Test/empty-user.trc', port=port) as p:
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            match (user):
                case user.resource:
                    opt = ['-user', '']
                case user.user_env:
                    opt = []
                    env['USER'] = ''
                case user.username_env:
                    opt = []
                    del env['USER']
                    env['USERNAME'] = ''
            command = ['s3270']
            command += opt
            command.append(f'a:c:127.0.0.1:{port}')
            s3270 = Popen(vgwrap(command), stdin=PIPE, stdout=DEVNULL, env=env)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

        # Wait for the process to exit.
        s3270.stdin.write(b'Quit()\n')
        s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_empty_user_env(self):
        self.s3270_empty_user(user_method.user_env)
    def test_s3270_empty_username_env(self):
        self.s3270_empty_user(user_method.username_env)
    def test_s3270_empty_user_resource(self):
        self.s3270_empty_user(user_method.resource)

    def s3270_specific_user(self, user: user_method):
        '''s3270 NEW-ENVIRON specific USER test'''

        # Create a trace file on the fly, expecting the enum name as the USER value.
        (handle, file_name) = tempfile.mkstemp()
        os.close(handle)
        with open(file_name, 'w') as f:
            f.write('< 0x0   fffd27\n') # RCVD DO NEW-ENVIRON
            f.write('> 0x0   fffb27\n') # SENT WILL NEW-ENVIRON
            f.write('< 0x0   fffa27010055534552fff0\n') # RCVD SB NEW-ENVIRON SEND VAR "USER" SE
            f.write('> 0x0   fffa2700005553455201' + bytes.hex(user.name.encode()) + 'fff0\n') # SENT SB NEW-ENVIRON IS VAR "USER" "xxx" SE
            f.write('# trigger comparison\n')

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, file_name, port=port) as p:
            ts.close()

            # Start s3270.
            env = os.environ.copy()
            if 'USER' in env:
                user_value = env['USER']
            else:
                user_value = None
            if 'USERNAME' in env:
                username_value = env['USERNAME']
            else:
                username_value = None
            match (user):
                case user.resource:
                    opt = ['-user', user.name]
                case user.user_env:
                    opt = []
                    env['USER'] = user.name
                case user.username_env:
                    opt = []
                    if user_value != None:
                        del env['USER']
                    env['USERNAME'] = user.name
                case user.uri:
                    opt = []
                case user.UNKNOWN:
                    opt = []
                    if user_value != None:
                        del env['USER']
                    if username_value != None:
                        del env['USERNAME']
            command = ['s3270']
            command += opt
            if user != user.uri:
                command.append(f'a:c:127.0.0.1:{port}')
            else:
                command.append(f'telnet://{user.name}@127.0.0.1:{port}?waitoutput=false')
            s3270 = Popen(vgwrap(command), stdin=PIPE, stdout=DEVNULL, stderr=DEVNULL, env=env)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

        # Wait for the process to exit.
        s3270.stdin.write(b'Quit()\n')
        s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270)
        os.unlink(file_name)

    def test_s3270_specific_user_env(self):
        self.s3270_specific_user(user_method.user_env)
    def test_s3270_specific_username_env(self):
        self.s3270_specific_user(user_method.username_env)
    def test_s3270_specific_user_resource(self):
        self.s3270_specific_user(user_method.resource)
    def test_s3270_specific_user_uri(self):
        self.s3270_specific_user(user_method.uri)
    def test_s3270_specific_unknown(self):
        self.s3270_specific_user(user_method.UNKNOWN)

    def test_s3270_devname_success(self):
        '''s3270 NEW-ENVIRON DEVNAME success test'''

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, f's3270/Test/devname_success.trc', port=port) as p:
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-devname', 'foo===', f'127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

            # Feed s3270 some actions.
            s3270.stdin.write(b'Quit()\n')
            s3270.stdin.flush()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_devname_failure(self):
        '''s3270 NEW-ENVIRON DEVNAME failure test'''

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, f's3270/Test/devname_failure.trc', port=port) as p:
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-devname', 'foo=', f'127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

            # Feed s3270 some actions.
            s3270.stdin.write(b'Quit()\n')
            s3270.stdin.flush()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def test_s3270_devname_change(self):
        '''s3270 NEW-ENVIRON DEVNAME change test'''

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, f's3270/Test/devname_change1.trc', port=port) as p:
            ts.close()

            # Start s3270.
            s3270 = Popen(vgwrap(['s3270', '-devname', 'foo=', f'127.0.0.1:{port}']), stdin=PIPE, stdout=DEVNULL)
            self.children.append(s3270)

            # Make sure the emulator does what we expect.
            p.match()

        # Start 'playback' again to read s3270's output.
        # It should start over with devname foo1.
        port, ts = unused_port()
        with playback(self, f's3270/Test/devname_change1.trc', port=port) as p:
            ts.close()

            # Connect again.
            s3270.stdin.write(f'open 127.0.0.1:{port}\n'.encode())
            s3270.stdin.flush()

            # Make sure the emulator does what we expect.
            p.match()

        # Start 'playback' again to read s3270's output.
        # c3270 should start over with devname bar1.
        port, ts = unused_port()
        with playback(self, f's3270/Test/devname_change2.trc', port=port) as p:
            ts.close()

            # Change the devname and connect again.
            s3270.stdin.write(b'Set(devname,bar=)\n')
            s3270.stdin.flush()
            s3270.stdin.write(f'open 127.0.0.1:{port}\n'.encode())
            s3270.stdin.flush()

            # Make sure the emulator does what we expect.
            p.match()

        s3270.stdin.write(b'Quit()\n')
        s3270.stdin.flush()

        # Wait for the processes to exit.
        s3270.stdin.close()
        self.vgwait(s3270)

    def construct_tracefile(self, expect) -> str:
        '''Create a trace file on the fly'''
        (handle, file_name) = tempfile.mkstemp()
        os.close(handle)
        with open(file_name, 'w') as f:
            e_codepage = bytes.hex('CODEPAGE'.encode())
            e_charset = bytes.hex('CHARSET'.encode())
            e_kbdtype = bytes.hex('KBDTYPE'.encode())
            f.write('< 0x0   fffd27\n') # RCVD DO NEW-ENVIRON
            f.write('> 0x0   fffb27\n') # SENT WILL NEW-ENVIRON
            f.write('< 0x0   fffa270103' + e_codepage
                                + '03' + e_charset
                                + '03' + e_kbdtype + 'fff0\n') # RCVD SB NEW-ENVIRON SEND USERVAR "CODEPAGE" USERVAR "CHARSET" USERVAR "KBDTYPE" SE
            f.write('> 0x0   fffa270003' + e_codepage + '01' + bytes.hex(expect[1].encode()) +
                                    '03' + e_charset + '01' + bytes.hex(expect[2].encode()) +
                                    '03' + e_kbdtype + '01' + bytes.hex(expect[3].encode()) +
                            'fff0\n') # SENT SB NEW-ENVIRON IS USERVAR "CODEPAGE" "xxx" ... SE
            f.write('# trigger comparison\n')
        return file_name

    def s3270_new_environ_charset(self, env_override: Any, expect: Any, expect2=None):
        '''s3270 NEW-ENVIRON CODEPAGE test'''

        # Start s3270.
        env = env_override if env_override != None else os.environ.copy()
        s3270 = Popen(vgwrap(['s3270', '-codepage', expect[0]]), stdin=PIPE, stdout=DEVNULL, stderr=DEVNULL, env=env)
        self.children.append(s3270)

        # Create a trace file on the fly.
        file_name = self.construct_tracefile(expect)

        # Start 'playback' to read s3270's output.
        port, ts = unused_port()
        with playback(self, file_name, port=port) as p:
            ts.close()
            s3270.stdin.write(f'Open(a:c:127.0.0.1:{port})\n'.encode())
            s3270.stdin.flush()

            # Make sure the emulator does what we expect.
            p.match()

        # If there is a second expect tuple, switch code pages and try again.
        if expect2 != None:
            os.unlink(file_name)
            file_name = self.construct_tracefile(expect2)
            port, ts = unused_port()
            with playback(self, file_name, port=port) as p:
                ts.close()
                s3270.stdin.write(f'Wait(disconnect)\n'.encode())
                s3270.stdin.write(f'Set(codepage,{expect2[0]})\n'.encode())
                s3270.stdin.write(f'Open(a:c:127.0.0.1:{port})\n'.encode())
                s3270.stdin.flush()

                # Make sure the emulator does what we expect.
                p.match()

        # Wait for the process to exit.
        s3270.stdin.write(b'Quit()\n')
        s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270)
        os.unlink(file_name)

    def test_s3270_new_environ_charset_us(self):
        self.s3270_new_environ_charset(None, ('bracket', '037', '697', 'USB'))
    def test_s3270_new_environ_charset_dbcs_chinese(self):
        self.s3270_new_environ_charset(None, ('simplified-chinese', '836', '1174', 'RCB'))
    def test_s3270_new_environ_charset_dbcs_korean(self):
        self.s3270_new_environ_charset(None, ('korean', '833', '1173', 'KQB'))
    def test_s3270_new_environ_charset_override_codepage(self):
        env = os.environ.copy()
        env['CODEPAGE'] = 'foo'
        self.s3270_new_environ_charset(env, ('simplified-chinese', 'foo', '1174', 'RCB'))
    def test_s3270_new_environ_charset_override_charset(self):
        env = os.environ.copy()
        env['CHARSET'] = 'foo'
        self.s3270_new_environ_charset(env, ('simplified-chinese', '836', 'foo', 'RCB'))
    def test_s3270_new_environ_charset_override_kbdtype(self):
        env = os.environ.copy()
        env['KBDTYPE'] = 'foo'
        self.s3270_new_environ_charset(env, ('simplified-chinese', '836', '1174', 'foo'))
    def test_s3270_new_environ_charset_switch(self):
        self.s3270_new_environ_charset(None, ('bracket', '037', '697', 'USB'), ('simplified-chinese', '836', '1174', 'RCB'))

if __name__ == '__main__':
    unittest.main()
