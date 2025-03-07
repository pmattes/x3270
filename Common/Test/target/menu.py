#!/usr/bin/env python3
#
# Copyright (c) 2022-2023 Paul Mattes.
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
# x3270 test target host, menu.

import logging

import aswitch
from ds import *
from ibm3270ds import *
import oopts
import server
import socketwrapper
import tn3270
import tn3270e_proto

title = 'x3270 test target'
default_prompt = '==> '.encode()
no_such_msg = 'No such service'
quit = 'quit'

def get_menu(kind: str, switch: aswitch.aswitch, prompt=default_prompt) -> bytes:
    '''Create the menu'''
    ret = f'{title} ({kind})\r\n\r\n'
    servers = switch.list()
    mx = max(len(server) for server in servers)
    for server in servers:
        ret += ' ' + server.ljust(mx + 2)
        if servers[server] != None:
            ret += servers[server]
        ret += '\r\n'
    ret += ' ' + quit.ljust(mx + 2) + 'Disconnect from test target\r\n\r\n'
    return ret.encode() + prompt

def clean(b: bytes):
    '''Clean an input string'''
    b = b.strip(b' \r\n\x85')
    bfilt = bytes([x if x >= 0x20 and x < 0x7f else 0x2e for x in b])
    return bfilt.decode()

def no_such(cmd: str, prompt=default_prompt) -> bytes:
    '''Build 'no such command' error message'''
    return f'{no_such_msg}: {cmd}\r\n'.encode() + prompt

def to_ebc(b: bytes) -> bytes:
    '''Convert ASCII to EBCDIC'''
    # Slight funkiness: U+0085 (NEL) becomes EBCDIC X'15' (NL).
    return b.decode().replace('\r\n', '\x85').encode('cp037')

def to_ascii(b: bytes):
    '''Convert EBCDIC to ASCII'''
    return b.decode('cp037').encode()

def expand_newlines(text: bytes) -> bytes:
    '''Expand X'15' into SBA orders'''
    row = 1
    col = 1
    ret = []
    for char in text:
        if char == 0x15:
            ret += list(sba_bytes(row + 1, 1, 80))
            row += 1
            col = 1
            continue
        else:
            ret.append(char)
            col += 1
            if col > 80:
                row += 1
                col = 1
    return bytes(ret)

class menu(server.server):
    '''Menu using plain TELNET'''

    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        '''Initialize'''
        self.conn = conn
        self.peername = peername
        self.switch = switch
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def process(self, b: bytes):
        '''Process data'''
        cmd = clean(b)
        if cmd == '':
            self.conn.send(default_prompt)
            return
        if cmd == quit:
            self.conn.close()
            return
        if cmd in self.switch.list():
            self.switch.switch(self.peername, cmd)
        else:
            self.conn.send(no_such(cmd))

    def ready(self) -> bool:
        '''Ready'''
        self.conn.send(get_menu('plain TELNET', self.switch))
        return True

class menu_n(tn3270.tn3270_server):
    '''Menu using TN3270E NVT mode'''

    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        '''Initialize'''
        mod_opts = oopts.clone(opts)
        mod_opts.override('tn3270e', 'True')
        super().__init__(conn, logger, peername, tls, switch, mod_opts)
        self.conn = conn
        self.peername = peername
        self.switch = switch
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def rcv_data_cooked(self, b: bytes, mode: tn3270e_proto.data_type):
        '''Process data'''
        if mode != tn3270e_proto.data_type.nvt_data:
            self.warning('menu_en', f'unexpected input type {mode}')
            return
        cmd = clean(b)
        if cmd == '':
            self.send_host(default_prompt, mode)
            return
        if cmd == quit:
            self.conn.close()
            return
        if cmd in self.switch.list():
            self.undo()
            self.switch.switch(self.peername, cmd, drain=True)
        else:
            self.send_host(no_such(cmd), mode)

    def start3270(self) -> bool:
        '''Ready'''
        self.send_host(get_menu('TN3270E NVT mode', self.switch), tn3270e_proto.data_type.nvt_data)
        return True

class menu_s(tn3270.tn3270_server):
    '''Menu using TN3270E SSCP-LU mode'''

    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        '''Initialize'''
        mod_opts = oopts.clone(opts)
        mod_opts.override('tn3270e', 'True')
        super().__init__(conn, logger, peername, tls, switch, mod_opts)
        self.conn = conn
        self.peername = peername
        self.switch = switch
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def rcv_data_cooked(self, b: bytes, mode: tn3270e_proto.data_type):
        '''Process data'''
        if mode != tn3270e_proto.data_type.sscp_lu_data:
            self.warning('menu_es', f'unexpected input type {mode}')
            return
        cmd = clean(to_ascii(b))
        if cmd == '':
            self.send_host(to_ebc(default_prompt), mode)
            return
        if cmd == quit:
            self.conn.close()
            return
        if cmd in self.switch.list():
            self.undo()
            self.switch.switch(self.peername, cmd, drain=True)
        else:
            self.send_host(to_ebc(no_such(cmd)), mode)

    def start3270(self) -> bool:
        '''Ready'''
        self.send_host(to_ebc(get_menu('TN3270E SSCP-LU mode', self.switch)), tn3270e_proto.data_type.sscp_lu_data)
        return True

class menu_u(tn3270.tn3270_server):
    '''Menu using unformatted 3270 screen'''

    unformatted_prompt = 'Press CLEAR, type selection and press ENTER\r\nF3=END\r\n'.encode()

    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        '''Initialize'''
        mod_opts = oopts.clone(opts)
        super().__init__(conn, logger, peername, tls, switch, opts)
        self.conn = conn
        self.peername = peername
        self.switch = switch
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def rcv_data_cooked(self, b: bytes, mode: tn3270e_proto.data_type):
        '''Process data'''
        if mode != tn3270e_proto.data_type.d3270_data:
            self.warning('menu_u', f'unexpected input type {mode}')
            return
        if len(b) == 1 and b[0] == aid.CLEAR:
            self.send_host(bytes([command.erase_write, wcc.keyboard_restore | wcc.reset]))
            return
        if len(b) >= 1 and b[0] == aid.PF3:
            self.conn.close()
            return
        if len(b) == 0 or b[0] != aid.ENTER or len(b) <= 3:
            self.display_menu()
            return
        cmd = clean(to_ascii(b[3:]))
        if cmd == '':
            self.start3270()
            return
        if cmd == quit:
            self.conn.close()
            return
        if cmd in self.switch.list():
            self.undo()
            self.switch.switch(self.peername, cmd, drain=True)
        else:
            self.display_menu(no_such(cmd[0:10], b''))

    def display_menu(self, errmsg=b''):
        '''Display the menu with an optional error message'''
        if errmsg != b'':
            prompt = errmsg + self.unformatted_prompt
            alarm = wcc.sound_alarm
        else:
            prompt = self.unformatted_prompt
            alarm = 0
        self.send_host(bytes([command.erase_write, wcc.keyboard_restore | wcc.reset | alarm])
                    + expand_newlines(to_ebc(get_menu('unformatted 3270', self.switch, prompt)))
                    + bytes([order.ic]))

    def start3270(self) -> bool:
        '''Ready'''
        self.display_menu()
        return True

class menu_f(tn3270.tn3270_server):
    '''Menu using formatted 3270 screen'''

    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        '''Initialize'''
        super().__init__(conn, logger, peername, tls, switch, opts)
        self.conn = conn
        self.peername = peername
        self.switch = switch
        self.cmd = ''
        self.raw_cmd = b''
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def rcv_data_cooked(self, b: bytes, mode: tn3270e_proto.data_type):
        '''Process data'''
        if mode != tn3270e_proto.data_type.d3270_data:
            self.warning('menu_u', f'unexpected input type {mode}')
            return
        if len(b) == 1 and b[0] == aid.CLEAR:
            self.cmd = ''
            self.display_menu()
            return
        if len(b) >= 1 and b[0] == aid.PF3:
            self.conn.close()
            return
        if len(b) == 0 or b[0] != aid.ENTER or len(b) <= 3:
            self.display_menu()
            return
        self.enter(b)

    def display_menu(self, errmsg=b''):
        '''Display the menu with an optional error message'''
        if errmsg != b'':
            alarm = wcc.sound_alarm
        else:
            alarm = 0
        self.send_host(bytes([command.erase_write, wcc.keyboard_restore | wcc.reset | alarm])
                    + expand_newlines(to_ebc(get_menu('formatted 3270', self.switch, b'')))
                    + sba_bytes(21, 1, 80) + errmsg
                    + sba_bytes(22, 1, 80) + to_ebc(b'==>')
                    + bytes([order.sfe, 2, xa.m3270, fa.normal_nonsel | fa.modify, xa.input_control, 1, order.ic])
                    + self.raw_cmd
                    + sba_bytes(23, 80, 80) + bytes([order.sf, fa.protect | fa.numeric])
                    + to_ebc(b'F3=END'))

    def start3270(self) -> bool:
        '''Ready'''
        self.display_menu()
        return True

    def dbtrunc(self, text: bytes, length: int):
        '''Truncate a string to 'length' bytes, being mindful of SO/SI'''
        ret = [i for i in text][0:length]
        if len(ret) > 0:
            if ret[-1] == 0x0e:
                ret = ret[0:len(ret) - 1]
            else:
                dbcs = -1
                for i in ret:
                    if i == 0x0e:
                        dbcs = 0
                    elif i == 0x0f:
                        dbcs = -1
                    elif dbcs >= 0:
                        dbcs += 1
                if dbcs > 0:
                    if dbcs % 2 == 1:
                        # Odd number of DBCS characters, replace the odd one with an SO.
                        ret[-1] = 0x0f
                    else:
                        # Even number of DBCS characters, replace the last two with an SO.
                        ret = ret[0:len(ret)-1]
                        ret[-1] = 0x0f
        return bytes(ret)

    def enter(self, data: bytes):
        '''Process an ENTER AID'''

        # Decode the implied READ BUFFER.
        reported_data = [0 for _ in range(24 * 80)]
        buffer_addr = -1
        i = 3
        while i < len(data):
            if data[i] == order.sba.value:
                buffer_addr = decode_address(data[i + 1 : i + 3])
                i += 3
            else:
                reported_data[buffer_addr] = data[i]
                buffer_addr += 1
                if buffer_addr >= 24 * 80:
                    buffer_addr = 0
                i += 1
        self.cmd = get_field(reported_data, 22, 1, 80, 79, underscore=False, pad=False, upper=False)
        self.raw_cmd = get_field_raw(reported_data, 22, 1, 80, 79, underscore=False, pad=False)
        if self.cmd == '':
            self.start3270()
            return
        if self.cmd == quit:
            self.conn.close()
            return
        if self.cmd in self.switch.list():
            self.undo()
            self.switch.switch(self.peername, self.cmd, drain=True)
            return
        no = no_such('', b'').replace(b'\r\n', b'')
        self.display_menu(to_ebc(no) + self.dbtrunc(self.raw_cmd, 63))

