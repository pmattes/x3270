#!/usr/bin/env python3
#
# Copyright (c) 2022-2024 Paul Mattes.
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
# x3270 test target host, common TELNET logic.

import logging
import socketwrapper

import consumer
from ftie import ftie
from telnet_proto import *

class ttelnet():
    '''TELNET server'''

    # Initialization.
    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger, peername: str, consumer: consumer.consumer):
        self.conn = conn
        self.logger = logger
        self.peername = peername
        self.consumer = consumer
        self.state = tn_state.DATA
        self.linebuf = []       # accumulated bytes of input data
        self.myopts = []        # my options (telopt)
        self.theiropts = []     # their options (telopt)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.__del__()
        return

    def __del__(self):
        return

    # Logging support.
    def log(self, level: int, module: str, message: str):
        self.logger.log(level, f'{module}:{self.peername}: {message}')
    def debug(self, module: str, message: str):
        self.log(logging.DEBUG, module, message)
    def info(self, module: str, message: str):
        self.log(logging.INFO, module, message)
    def warning(self, module: str, message: str):
        self.log(logging.WARNING, module, message)
    def error(self, module: str, message: str, prefix):
        self.log(logging.ERROR, module, message)

    def process(self, b: bytes) -> bool:
        '''Process input'''
        self.debug('TELNET', f'got {len(b)} bytes')
        for byte in b:
            match self.state:
                case tn_state.DATA:
                    if byte == int(telcmd.IAC):
                        self.state = tn_state.IAC
                        continue
                    self.linebuf.append(byte)
                    if not telopt.EOR in self.myopts and not telopt.TN3270E in self.theiropts:
                        self.consumer.rcv_data(bytes(self.linebuf))
                        self.linebuf.clear()
                case tn_state.IAC:
                    match ftie(byte, telcmd):
                        case telcmd.IAC:
                            self.linebuf.append(byte)
                            if not telopt.EOR in self.myopts and not telopt.TN3270E in self.theiropts:
                                self.consumer.rcv_data(bytes(self.linebuf))
                                self.linebuf.clear()
                            self.state = tn_state.DATA
                        case telcmd.DONT:
                            self.state = tn_state.DONT
                        case telcmd.DO:
                            self.state = tn_state.DO
                        case telcmd.WONT:
                            self.state = tn_state.WONT
                        case telcmd.WILL:
                            self.state = tn_state.WILL
                        case telcmd.SB:
                            self.state = tn_state.SB
                        case telcmd.EOR:
                            if telopt.EOR in self.myopts or telopt.TN3270E in self.theiropts:
                                self.consumer.rcv_data(bytes(self.linebuf))
                                self.linebuf.clear()
                            self.state = tn_state.DATA
                        case _:
                            pass
                case tn_state.WILL:
                    opt = ftie(byte, telopt)
                    self.debug('TELNET', f'got WILL {opt.name}')
                    if not opt in self.theiropts:
                        self.theiropts.append(opt)
                        if not self.consumer.rcv_will(opt):
                            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.DONT), byte]))
                            self.debug('TELNET', f'sent DONT {opt.name}')
                    self.state = tn_state.DATA
                case tn_state.WONT:
                    opt = ftie(byte, telopt)
                    self.debug('TELNET', f'got WONT {opt.name}')
                    if opt in self.theiropts:
                        self.theiropts.remove(opt)
                        self.send_raw(bytes([int(telcmd.IAC), int(telcmd.DONT), byte]))
                        self.debug('TELNET', f'sent DONT {opt.name}')
                    self.consumer.rcv_wont(opt)
                    self.state = tn_state.DATA
                case tn_state.DO:
                    opt = ftie(byte, telopt)
                    self.debug('TELNET', f'got DO {opt.name}')
                    if not opt in self.myopts:
                        if opt != telopt.TM and self.consumer.rcv_do(opt):
                            self.myopts.append(opt)
                            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.WILL), byte]))
                            self.debug('TELNET', f'sent WILL {opt.name}')
                        else:
                            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.WONT), byte]))
                            self.debug('TELNET', f'sent WONT {opt.name}')
                    self.state = tn_state.DATA
                case tn_state.DONT:
                    opt = ftie(byte, telopt)
                    self.debug('TELNET', f'got DONT {opt.name}')
                    if opt in self.myopts:
                        self.myopts.remove(opt)
                        if self.consumer.rcv_dont(opt):
                            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.WONT), byte]))
                            self.debug('TELNET', f'sent WONT {opt.name}')
                    self.state = tn_state.DATA
                case tn_state.SB:
                    if byte == int(telcmd.IAC):
                        self.state = tn_state.SB_IAC
                        continue
                    self.linebuf.append(byte)
                case tn_state.SB_IAC:
                    if byte == int(telcmd.SE):
                        if len(self.linebuf) > 0 and ftie(self.linebuf[0], telopt) in self.theiropts:
                            self.debug('TELNET', f'got SB {ftie(self.linebuf[0], telopt)} {len(self.linebuf) - 1} bytes')
                            self.consumer.rcv_sb(ftie(self.linebuf[0], telopt), bytes(self.linebuf[1:]))
                        self.linebuf.clear()
                        self.state = tn_state.DATA
                    else:
                        self.linebuf.append(byte)
                        self.state = tn_state.SB

    def send_will(self, opt: telopt):
        '''Tell client we will do an option'''
        if not ftie(opt) in self.myopts:
            self.myopts.append(ftie(opt))
            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.WILL), int(opt)]))
            self.debug('TELNET', f'sent WILL {opt.name}')

    def send_wont(self, opt: telopt):
        '''Tell client we won't do an option'''
        if ftie(opt) in self.myopts:
            self.myopts.remove(ftie(opt))
            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.WONT), int(opt)]))
            self.debug('TELNET', f'sent WONT {opt.name}')

    def send_do(self, opt: telopt):
        '''Tell client to DO an option'''
        if not ftie(opt) in self.theiropts:
            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.DO), int(opt)]))
            self.debug('TELNET', f'sent DO {opt.name}')

    def send_dont(self, opt: telopt):
        '''Tell client to DONT an option'''
        if ftie(opt) in self.theiropts:
            self.send_raw(bytes([int(telcmd.IAC), int(telcmd.DONT), int(opt)]))
            self.debug('TELNET', f'sent DONT {opt.name}')

    def ask_sb(self, opt: telopt, b=b''):
        '''Tell client to SB SEND an option'''
        self.send_raw(bytes([int(telcmd.IAC), int(telcmd.SB), int(opt), int(telqual.SEND)]) + b + bytes([int(telcmd.IAC), int(telcmd.SE)]))
        self.debug('TELNET', f'sent SB {opt.name} SEND')

    def send_sb(self, opt: telopt, b: bytes):
        '''Send an IAC SB'''
        self.send_raw(bytes([int(telcmd.IAC), int(telcmd.SB), int(opt)]) + b + bytes([int(telcmd.IAC), int(telcmd.SE)]))
        self.debug('TELNET', f'sent SB {opt.name} +{len(b)} bytes')

    def send_data(self, b: bytes, eor=False):
        '''Quote and send data to client'''
        bb = b.replace(b'\xff', b'\xff\xff')
        if eor:
            bb += bytes([int(telcmd.IAC), int(telcmd.EOR)])
        self.send_raw(bb)

    def send_raw(self, b: bytes):
        '''Send raw data to client'''
        self.conn.send(b)

    def undo(self):
        '''Undo all TELNET negotiation'''
        for opt in self.theiropts:
            if opt != telopt.STARTTLS:
                self.send_dont(opt.real)
        for opt in self.myopts:
            if opt != telopt.STARTTLS:
                self.send_wont(opt.real)

    def hangup(self):
        '''Hang up the connection'''
        self.conn.close()

    def wrap(self, cert: str, key: str) -> bool:
        '''Start TLS negotiation'''
        return self.conn.wrap(cert, key)
