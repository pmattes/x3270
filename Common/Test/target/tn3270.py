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
# x3270 test target host, TN3270 server.

import pathlib
import queue
import re
import select
import socket

import aswitch
from atn3270 import atn3270
import ds
from ibm3270ds import *
import oopts
from telnet_proto import *
import tn3270e
import tn3270e_proto
from ttelnet import *

def base26(i: int) -> str:
    '''Generate a two-digit base-26 string'''
    ldigit = i // 26
    rdigit = i % 26
    alphas = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    return alphas[ldigit: ldigit + 1] + alphas[rdigit : rdigit + 1]

liked_options = [ telopt.TTYPE, telopt.BINARY, telopt.EOR, telopt.TN3270E, telopt.STARTTLS, telopt.NEW_ENVIRON ]

# Set up the LU queues.
lu_count = 100
termids = queue.Queue()
systems = queue.Queue()
for i in range(lu_count):
    termids.put('IBM0TE' + f'{i:02}')
    systems.put('IBM0SM' + base26(i))

class tn3270_server(ttelnet, atn3270, consumer.consumer):
    '''TN3270 protocol server'''

    def __init__(self, conn: socket.socket, logger: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        super().__init__(conn, logger, peername, self, switch)
        self.in3270 = False
        self.dumps = {}
        self.tn3270e = tn3270e.tn3270e(self, opts)
        self.termid = None
        self.system = None
        self.tls = tls
        self.do_tn3270e = opts.get('tn3270e')
        self.elf = opts.get('elf')
        self.devname_max = opts.get('devname')
    def __enter__(self):
        super().__enter__()
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        super().__exit__(exc_type, exc_value, exc_traceback)
    def __del__(self):
        super().__del__()
        if self.termid != None:
            termids.put(self.termid)
            systems.put(self.system)
            self.termid = None
            self.system = None

    def ready(self) -> bool:
        '''Initialize TN3270 state'''
        try:
            self.termid = termids.get_nowait()
            self.system = systems.get_nowait()
        except Exception as e:
            self.info('TN3270', f'Caught {type(e)}')
            self.send_data(b'Logical units exhausted\r\n')
            return False
        if self.tls:
            # Negotiated TLS.
            # Wait 2 seconds for input, then ask for STARTTLS.
            r, _, _ = select.select([self.conn], [], [], 2)
            if r == []:
                self.debug('TN3270', 'initial TLS negotiation read timeout')
                self.send_do(telopt.STARTTLS)
            else:
                if not self.wrap():
                    self.hangup()
                self.send_do(telopt.TN3270E if self.do_tn3270e else telopt.TTYPE)
                self.send_do(telopt.NEW_ENVIRON)
        else:
            self.send_do(telopt.TN3270E if self.do_tn3270e else telopt.TTYPE)
            self.send_do(telopt.NEW_ENVIRON)
        return True

    # Called from TELNET.
    def rcv_data(self, data: bytes):
        '''Process data from TELNET'''
        if telopt.TN3270E in self.theiropts:
            # TN3270E needs to see this first, and may pass something to the host.
            self.tn3270e.from_terminal(data)
        else:
            # Send it to the emulated host.
            self.rcv_data_cooked(data, mode=tn3270e_proto.data_type.d3270_data)

    def send_host(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Send host data'''
        if telopt.TN3270E in self.theiropts:
            data = self.tn3270e.header(mode) + data
        self.send_data(data, eor=True)

    def rcv_sb(self, option: telopt, data: bytes):
        '''Consume SB'''
        if option == telopt.TN3270E:
            self.tn3270e.rcv_sb(data)
            return
        if option == telopt.TTYPE and len(data) > 0 and telqual(data[0]) == telqual.IS:
            self.ttype = data[1:].decode('iso8859-1')
            self.debug('TN3270', f'got TTYPE {self.ttype}')
            if not re.match(r'IBM-327[89](-E)?', self.ttype) and self.ttype != 'IBM-DYNAMIC':
                self.warning('TN3270', 'wrong terminal type')
                self.send_data(b'3270 emulation required (wrong terminal type)\r\n')
                self.hangup()
                return
            self.dinfo = ds.dinfo(self.ttype)
            self.send_do(telopt.EOR)
        if option == telopt.STARTTLS:
            if len(data) < 1 or data[0] != int(teltls.FOLLOWS):
                self.warning('TN3270', 'bad SB STARTTLS')
                self.send_dont(telopt.STARTTLS)
                self.send_do(telopt.TN3270E if self.do_tn3270e else telopt.TTYPE)
                return
            self.send_sb(telopt.STARTTLS, bytes([int(teltls.FOLLOWS)]))
            if not self.wrap():
                self.hangup()
            self.send_do(telopt.TN3270E if self.do_tn3270e else telopt.TTYPE)
        if option == telopt.NEW_ENVIRON:
            self.parse_new_environ(data)

    def rcv_will(self, option: telopt) -> bool:
        '''Approve WILL option'''
        if option == telopt.TN3270E:
            self.tn3270e.negotiate()
        if option == telopt.TTYPE:
            self.ask_sb(option)
        if option == telopt.EOR:
            self.send_do(telopt.BINARY)
            self.send_will(telopt.BINARY)
            self.send_will(telopt.EOR)
        if telopt.BINARY in self.theiropts and telopt.EOR in self.theiropts and telopt.BINARY in self.myopts and telopt.EOR in self.myopts:
            if not self.in3270:
                self.switch_3270(True)
        if option == telopt.NEW_ENVIRON:
            request = b''
            if self.elf:
                request += bytes([int(telobj.USERVAR)]) + 'IBMELF'.encode() + bytes([int(telobj.USERVAR)]) + 'IBMAPPLID'.encode()
            if self.devname_max > 0:
                request += bytes([int(telobj.USERVAR)]) + 'DEVNAME'.encode()
            self.ask_sb(option, request, decode_new_environ_sb_send(request))
            self.devname_last_value = None
            self.devname_count = 0
        return option in liked_options

    def rcv_wont(self, option: telopt) -> bool:
        '''Notify of WONT option'''
        if option == telopt.TN3270E:
            self.tn3270e.stop()
            self.send_do(telopt.TTYPE)
        if option == telopt.TTYPE or telopt.BINARY or option == telopt.EOR:
            self.warning('TN3270', 'missing required option')
            self.send_data(b'3270 emulation required (option ' + str(option.name).encode('iso8859-1') + b'r equired)\r\n')
            self.hangup()
            return False
        if option == telopt.STARTTLS and self.tls:
            self.send_do(telopt.TN3270E if self.do_tn3270e else telopt.TTYPE)
        return True

    def rcv_do(self, option: telopt) -> bool:
        '''Approve DO option'''
        return option == telopt.BINARY or option == telopt.EOR

    def rcv_dont(self, option: telopt) -> bool:
        '''Approve DONT option'''
        if option == telopt.BINARY or option == telopt.EOR:
            self.warning('TN3270', 'missing required option')
            self.send_data(b'3270 emulation required (option ' + str(option.name).encode('iso8859-1') + b' required)\r\n')
            self.hangup()
            return False
        return True

    def rcv_cmd(self, cmd: telcmd):
        '''Notify of command'''
        self.debug('TN3270', f'got {cmd.name} command')

    def get_dump(self, name: str) -> bytes:
        '''Get a screen dump file and write it out to the client'''
        if name in self.dumps:
            return self.dumps[name]
        with (pathlib.Path(__file__).parent.resolve() / (name + '.dump')).open() as f:
            blob = bytes.fromhex(''.join([line for line in f.readlines() if not line.startswith('#')]))
        self.dumps[name] = blob
        return blob

    def query(self):
        '''Send a Query'''
        # Note: 0x00, 0x05 is a 16-bit length 5, which includes the length itself
        self.send_host(bytes([command.write_structured_field, 0x00, 0x05, sf.read_partition, 0xff, sf_rp.query]))
        self.debug('TN3270', 'sent Query')

    def parse_new_environ(self, data: bytes):
        '''Parse a NEW-ENVIRON sub-negotiation'''
        if len(data) == 0:
            return
        is_is = False
        if ftie(data[0], telqual) == telqual.IS:
            decode = 'IS'
            is_is = True
        elif ftie(data[0], telqual) == telqual.INFO:
            decode = 'INFO'
            is_is = False
        else:
            self.warning('TN3270', f'Unknown NEW-ENVIRON SB code {data[0]}')
            return
        is_uservar = False
        varname = ''
        value = ''
        data = data[1:]
        devname = None
        while len(data) > 0:
            if telobj(data[0]) == telobj.VAR:
                decode += ' VAR '
                is_uservar = False
            elif telobj(data[0]) == telobj.USERVAR:
                decode += ' USERVAR '
                is_uservar = True
            else:
                self.warning('TN3270', f'Unknown NEW-ENVIRON code {data[0]}')
                return
            # Accumulate bytes until we hit the end or a VALUE.
            data = data[1:]
            varname = ''
            while len(data) > 0 and ftie(data[0], telobj) != telobj.VALUE and ftie(data[0], telobj) != telobj.VAR and ftie(data[0], telobj) != telobj.USERVAR:
                if ftie(data[0], telobj) == telobj.ESC:
                    data = data[1:]
                    if len(data) == 0:
                        self.warning('TN3270', 'Missing data after ESC')
                        return
                nxb = data[0:1].decode()
                data = data[1:]
                decode += nxb
                varname += nxb
            decode += '="'
            value = ''
            if len(data) > 0 and ftie(data[0], telobj) == telobj.VALUE:
                data = data[1:]
                while len(data) > 0 and ftie(data[0], telobj) != telobj.VAR and ftie(data[0], telobj) != telobj.USERVAR:
                    if ftie(data[0], telobj) == telobj.ESC:
                        data = data[1:]
                        if len(data) == 0:
                            self.warning('TN3270', 'Missing data after ESC')
                            return
                    nxb = data[0:1].decode()
                    data = data[1:]
                    decode += nxb
                    value += nxb
            decode += '"'
            if is_is and is_uservar and varname == 'DEVNAME':
                devname = value
        self.debug('TN3270', f'NEW-ENVIRON {decode}')
        if self.devname_max > 0 and devname != None:
            if devname == self.devname_last_value:
                self.info('TN3270', 'DEVNAME repeated, disconnecting')
                self.hangup()
                return
            self.devname_last_value = devname
            self.devname_count += 1
            if self.devname_count <= self.devname_max:
                sb = bytes([int(telobj.USERVAR)]) + 'DEVNAME'.encode()
                self.ask_sb(telopt.NEW_ENVIRON, sb, decode_new_environ_sb_send(sb))

    # TN3270E entry points.
    def e_sb(self, data: bytes):
        '''Send sub-negotiation'''
        self.send_raw(bytes([int(telcmd.IAC), int(telcmd.SB), int(telopt.TN3270E)])
                       + data.replace(b'\xff', b'\xff\xff') + bytes([int(telcmd.IAC), int(telcmd.SE)]))

    def e_to_terminal(self, data: bytes):
        '''Send data to terminal'''
        self.send_data(data, eor=True)
        return

    def e_to_host(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Send data to host'''
        self.rcv_data_cooked(data, mode)
        return

    def e_in3270(self, in3270: bool):
        '''Switch 3270 modes'''
        self.switch_3270(in3270)
        if not in3270:
            self.send_wont(telopt.TN3270E)
            self.send_dont(telopt.TN3270E)
            self.send_do(telopt.TTYPE)

    def e_get_termid(self) -> str:
        '''Get the terminal ID'''
        return self.termid

    def e_get_system(self) -> str:
        '''Get the system'''
        return self.system

    def switch_3270(self, in3270: bool):
        '''Switch 3270 modes'''
        if in3270 != self.in3270:
            self.in3270 = in3270
            self.debug('TN3270', f'{"in" if self.in3270 else "not in"} 3270 mode')
            if self.in3270:
                self.start3270()

    def e_warning(self, message: str):
        '''Log a warning message'''
        self.warning('TN3270E', message)

    def e_info(self, message: str):
        '''Log an info message'''
        self.info('TN3270E', message)

    def e_debug(self, message: str):
        '''Log a debug message'''
        self.debug('TN3270E', message)

    def e_set_ttype(self, ttype: str) -> ds.dinfo:
        '''Set the terminal type'''
        self.ttype = ttype
        self.dinfo = ds.dinfo(ttype)
        return self.dinfo


def decode_new_environ_sb_send(b: bytes) -> str:
    '''Decode a NEW_ENVIRON SB send'''
    data = b
    decode = ''
    space = ''
    while len(data) > 0:
        if telobj(data[0]) == telobj.VAR:
            decode += space + 'VAR '
        elif telobj(data[0]) == telobj.USERVAR:
            decode += space + 'USERVAR '
        else:
            break
        space = ' '
        data = data[1:]
        while len(data) > 0 and ftie(data[0], telobj) != telobj.VAR and ftie(data[0], telobj) != telobj.USERVAR:
            decode += data[0:1].decode()
            data = data[1:]
    return decode
