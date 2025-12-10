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
# b3270 NVT tests

from dataclasses import dataclass
import re
from subprocess import Popen, DEVNULL
from typing import Sequence
import unittest

from Common.Test.cti import *

@requests_timeout
class TestB3270Nvt(cti):

    # Window report test.
    def window_report(self, send: str, receive: str):

        # Start a server to throw NVT escape sequences at b3270.
        s = copyserver()

        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport), '-json']), stdout=DEVNULL)
        self.children.append(b3270)
        self.check_listen(hport)
        ts.close()

        # Connect to the server.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(a:c:t:127.0.0.1:{s.port})')

        # Send what they want.
        if send != None:
            s.send(send)

        # Ask for rows and columns.
        s.send('\033[18t')

        # End the session.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # See what we get back.
        reply = s.data().decode()
        self.assertEqual(receive,reply)

        # Clean up.
        self.vgwait(b3270)

    # Basic window report test.
    def test_window_report(self):
        self.window_report(None, '\033[8;43;80t')
    # Window size change success test.
    def test_window_change_success(self):
        self.window_report('\033[8;45;81t', '\033[8;45;81t')
    # Window size change by lines test.
    def test_window_change_success_lines(self):
        self.window_report('\033[45t', '\033[8;45;80t')

    # Window change request that fails.
    def window_change_fail(self, rows: int, cols: int):
        self.window_report(f'\033[8;{rows};{cols}t', '\033[8;43;80t')
    
    def test_window_change_fail_small(self):
        self.window_change_fail(20, 20)
    def test_window_change_fail_large(self):
        self.window_change_fail(1000, 1000)
    def test_window_change_fail_zero_rows(self):
        self.window_change_fail(0, 80)
    def test_window_change_fail_zero_cols(self):
        self.window_change_fail(43, 0)

    # Window changes with missing (use existing value) parameters.
    def test_window_change_omit_rows(self):
        self.window_report('\033[8;;81t', '\033[8;43;81t')
    def test_window_change_omit_cols(self):
        self.window_report('\033[8;45t', '\033[8;45;80t')

    # Send an escape sequence and expect a b3270 response.
    def single_parameter(self, send: str, receive: bytes, extra_send=None):
        # Start a server to throw NVT escape sequences at b3270.
        s = copyserver()

        # Start b3270.
        hport, ts = unused_port()
        b3270 = Popen(vgwrap(['b3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport), '-json']), stdout=PIPE)
        self.children.append(b3270)
        self.check_listen(hport)
        ts.close()

        # Connect to the server.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(a:c:t:127.0.0.1:{s.port})')

        # Send someting extra.
        if extra_send != None:
            r = self.get(f'http://127.0.0.1:{hport}/3270/rest/json/{extra_send}')
            self.assertTrue(r.ok)

        # Send what they want.
        if send != None:
            s.send(send)

        # End the session.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')
        _ = s.data()

        # See what we get back.
        output = b3270.stdout.readlines()
        wc = [line for line in output if b'window-change' in line]
        b3270.stdout.close()
        if receive != None:
            self.assertEqual(receive, wc[0])
        else:
            got = wc[0] if len(wc) > 0 else None
            self.assertEqual(0, len(wc), f'expected nothing, got {got}')

        # Clean up.
        self.vgwait(b3270)

    def test_deiconify(self):
        self.single_parameter('\033[1t', b'{"window-change":{"operation":"state","state":"normal"}}\n')
    def test_iconify(self):
        self.single_parameter('\033[2t', b'{"window-change":{"operation":"state","state":"iconified"}}\n')
    def test_move(self):
        self.single_parameter('\033[3;100;200t', b'{"window-change":{"operation":"move","x":100,"y":200}}\n')
    def test_resize(self):
        self.single_parameter('\033[4;100;200t', b'{"window-change":{"operation":"size","type":"window","height":100,"width":200}}\n')
    def test_resize2(self):
        self.single_parameter('\033[4;;200t', b'{"window-change":{"operation":"size","type":"window","width":200}}\n')
    def test_resize3(self):
        self.single_parameter('\033[4;100t', b'{"window-change":{"operation":"size","type":"window","height":100}}\n')
    def test_raise(self):
        self.single_parameter('\033[5t', b'{"window-change":{"operation":"stack","order":"raise"}}\n')
    def test_lower(self):
        self.single_parameter('\033[6t', b'{"window-change":{"operation":"stack","order":"lower"}}\n')
    def test_refresh(self):
        self.single_parameter('\033[7t', b'{"window-change":{"operation":"refresh"}}\n')
    def test_unmaximize(self):
        self.single_parameter('\033[9;0t', b'{"window-change":{"operation":"state","state":"normal"}}\n')
    def test_unmaximize2(self):
        self.single_parameter('\033[9t', b'{"window-change":{"operation":"state","state":"normal"}}\n')
    def test_maximize(self):
        self.single_parameter('\033[9;1t', b'{"window-change":{"operation":"state","state":"maximized"}}\n')
    def test_unfullscreen(self):
        self.single_parameter('\033[10;0t', b'{"window-change":{"operation":"state","state":"normal"}}\n')
    def test_fullscreen(self):
        self.single_parameter('\033[10;1t', b'{"window-change":{"operation":"state","state":"full-screen"}}\n')
    def test_toggle_fullscreen(self):
        self.single_parameter('\033[10;2t', b'{"window-change":{"operation":"state","state":"toggle-full-screen"}}\n')

    def test_xtwinops_resource(self):
        self.single_parameter('\033[2t', None, extra_send='Set(xtwinops,false)')

    # Test the window-change operation.
    def oper_window_change(self, bmsg: str, bmsg_reply: str, esc: str, esc_reply: str, json=True, debug=False):
        # Start a server to throw NVT escape sequences at b3270.
        s = copyserver()

        # Start b3270.
        hport, ts = unused_port()
        cmd = ['b3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport)]
        if json:
            cmd.append('-json')
        b3270 = Popen(vgwrap(cmd), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        self.check_listen(hport)
        ts.close()

        # Connect to the server.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(a:c:t:127.0.0.1:{s.port})')

        # Send what they want on stdin.
        if bmsg != None:
            b3270.stdin.write(bmsg.encode() + b'\n')
            b3270.stdin.flush()
        if esc != None:
            s.send(esc)

        # End the session.
        self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Quit()')

        # See what we get back.
        got = s.data()
        output = b3270.stdout.readlines()
        b3270.stdout.close()
        b3270.stdin.close()
        if bmsg_reply != None:
            wc = [line for line in output if re.search(bmsg_reply.encode(), line) != None]
            self.assertEqual(1, len(wc))
            if debug:
                print('match is', wc[0])
        else:
            wc = [line for line in output if b'error' in line]
            self.assertEqual(0, len(wc))
        if esc_reply != None:
            self.assertEqual(esc_reply.encode(), got)

        # Clean up.
        self.vgwait(b3270)

    @dataclass
    class ChangeParam:
        bmsg: str
        bmsg_reply: str
        esc: str
        esc_reply: str
    
    # Test the window-change operation, in bulk.
    def oper_window_change_bulk(self, params: Sequence[ChangeParam], json=True, debug=False):
        # Start b3270.
        hport, ts = unused_port()
        cmd = ['b3270', '-set', 'noTelnetInputMode=character', '-httpd', str(hport)]
        if json:
            cmd.append('-json')
        b3270 = Popen(vgwrap(cmd), stdin=PIPE, stdout=PIPE)
        self.children.append(b3270)
        self.check_listen(hport)
        ts.close()

        for param in params:
            # Start a copyserver to throw NVT escape sequences at b3270.
            s = copyserver()

            # Connect to the copyserver.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Connect(a:c:t:127.0.0.1:{s.port})')

            # Send what they want on stdin.
            if param.bmsg != None:
                b3270.stdin.write(param.bmsg.encode() + b'\n')
                b3270.stdin.flush()
            if param.esc != None:
                s.send(param.esc)

            # Disconnect from the copyserver and send a marker.
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Disconnect()')
            self.get(f'http://127.0.0.1:{hport}/3270/rest/json/Info(xxx)')

            # See what we get back.
            got = s.data()
            output = []
            while True:
                line = b3270.stdout.readline()
                if b'xxx' in line:
                    break
                output.append(line)
            if param.bmsg_reply != None:
                wc = [line for line in output if re.search(param.bmsg_reply.encode(), line) != None]
                other_error = None
                if len(wc) == 0:
                    wx = [line for line in output if b'error' in line]
                    if len(wx) > 0:
                        other_error = wx[0]
                self.assertEqual(1, len(wc), f'sent {param.bmsg}, want {param.bmsg_reply}, got {other_error}')
            else:
                wc = [line for line in output if b'error' in line]
                other_error = None
                if len(wc) > 0:
                    other_error = wc[0]
                self.assertEqual(0, len(wc), f'sent {param.bmsg}, unexpected error: {other_error}')
            if param.esc_reply != None:
                self.assertEqual(param.esc_reply.encode(), got)

        # Clean up.
        b3270.stdout.close()
        b3270.stdin.close()
        self.vgwait(b3270)

    def test_change_esc(self):
        self.oper_window_change_bulk([
            self.ChangeParam(None, None, '\033[21t]', '\033]l\033\\'),
            self.ChangeParam('{"window-change":{"operation":"title","text":"foo"}}', None, '\033[21t]', '\033]lfoo\033\\'),
            self.ChangeParam(None, None, '\033[11t]', '\033[1t'),
            self.ChangeParam('{"window-change":{"operation":"state","state":"iconified"}}', None, '\033[11t]', '\033[2t'),
            self.ChangeParam('{"window-change":{"operation":"state","state":"normal"}}', None, '\033[11t]', '\033[1t'),
            self.ChangeParam('{"window-change":{"operation":"state","state":"maximized"}}', None, None, None),
            self.ChangeParam(None, None, '\033[13t]', '\033[3;0;0t'),
            self.ChangeParam('{"window-change":{"operation":"move","x":1,"y":2}}', None, '\033[13t]', '\033[3;1;2t'),
            self.ChangeParam('{"window-change":{"operation":"move","x":-1,"y":-2}}', None, '\033[13t]', '\033[3;65535;65534t'),
            ])
        self.oper_window_change_bulk([
            self.ChangeParam('<window-change operation="title" text="foo"/>', None, '\033[21t]', '\033]lfoo\033\\'),
            self.ChangeParam('<window-change operation="state" state="iconified"/>', None, '\033[11t]', '\033[2t'),
            self.ChangeParam('<window-change operation="state" state="normal"/>', None, '\033[11t]', '\033[1t'),
            self.ChangeParam('<window-change operation="state" state="maximized"/>', None, None, None),
            self.ChangeParam('<window-change operation="move" x="1" y="2"/>', None, '\033[13t]', '\033[3;1;2t'),
            self.ChangeParam('<window-change operation="move" x="-1" y="-2"/>', None, '\033[13t]', '\033[3;65535;65534t'),
            ], json=False)
    def test_change_size(self):
        self.oper_window_change_bulk([
            self.ChangeParam('{"window-change":{"operation":"size","height":10,"width": 20,"type":"window"}}', None, '\033[14;2t]', '\033[4;10;20t'),
            self.ChangeParam('{"window-change":{"operation":"size","height":30,"width": 40,"type":"character"}}', None, '\033[16t]', '\033[6;30;40t'),
            self.ChangeParam('{"window-change":{"operation":"size","height":30,"width": 40,"type":"character"}}', None, '\033[14t]', '\033[4;1290;3200t'),
            self.ChangeParam('{"window-change":{"operation":"size","height":50,"width": 60,"type":"screen"}}', None, '\033[15t]', '\033[5;50;60t'),
        ])
        self.oper_window_change_bulk([
            self.ChangeParam('<window-change operation="size" type="window" width="10" height="20"/>', None, None, None),
            self.ChangeParam('<window-change operation="size" type="character" width="30" height="40"/>', None, None, None),
            self.ChangeParam('<window-change operation="size" type="screen" width="50" height="60"/>', None, None, None),
        ], json=False)
    def test_change_junk(self):
        self.oper_window_change_bulk([
            self.ChangeParam('{"window-change":null}', 'must be an object', None, None),
            self.ChangeParam('{"window-change":{}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"fooey"}}', 'invalid member', None, None),
            self.ChangeParam('{"window-change":{"foo":"bar"}}', 'unknown member', None, None),
            self.ChangeParam('{"window-change":{"operation":"title"}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"title","text":3}}', 'must be a string', None, None),
            self.ChangeParam('{"window-change":{"operation":"state"}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"state","state":"foo"}}', 'invalid member', None, None),
            self.ChangeParam('{"window-change":{"operation":"move"}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"move","x":"fred"}}', 'must be a short integer', None, None),
            self.ChangeParam('{"window-change":{"operation":"move","x":655355}}', 'must be a short integer', None, None),
            self.ChangeParam('{"window-change":{"operation":"move","x":-655355}}', 'must be a short integer', None, None),
            self.ChangeParam('{"window-change":{"operation":"move","x":10}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"move","x":10,"y":"poot"}}', 'must be a short integer', None, None),
            self.ChangeParam('{"window-change":{"operation":"size"}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"size","height":"foo"}}', 'must be an unsigned short integer', None, None),
            self.ChangeParam('{"window-change":{"operation":"size","height":10}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"size","height":10,"width": 10}}', 'missing member', None, None),
            self.ChangeParam('{"window-change":{"operation":"size","height":10,"width": 10,"type":"foo"}}', 'unknown', None, None),
        ])
        self.oper_window_change_bulk([
            self.ChangeParam('<window-change/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="fooey"/>', 'invalid attribute', None, None),
            self.ChangeParam('<window-change foo="bar"/>', 'unknown attribute', None, None),
            self.ChangeParam('<window-change operation="title"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="state"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="state" state="foo"/>', 'invalid attribute', None, None),
            self.ChangeParam('<window-change operation="state"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="move"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="move" x="fred" y="3"/>', 'must be a short integer', None, None),
            self.ChangeParam('<window-change operation="move" x="655355" y="3"/>', 'must be a short integer', None, None),
            self.ChangeParam('<window-change operation="move" x="-655355" y="3"/>', 'must be a short integer', None, None),
            self.ChangeParam('<window-change operation="move" x="10"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="move" x="10" y="poot"/>', 'must be a short integer', None, None),
            self.ChangeParam('<window-change operation="size"/>', 'missing attribute', None, None),
            self.ChangeParam('<window-change operation="size" type="foo" width="x"/>', 'invalid attribute', None, None),
            self.ChangeParam('<window-change operation="size" type="foo" width="10" height="10"/>', 'invalid attribute', None, None),
        ], json=False)

if __name__ == '__main__':
    unittest.main()
