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
# x3270 test target host.

import argparse
import ipaddress
import logging
import select
import socket
import threading
import traceback
from typing import Dict, Any

import aswitch
import menu
import oopts
import server
import socketwrapper
import target_tls
import tn3270_ibmlink
import tn3270_snake
import tn3270_sruvm
import tn3270_uvvm

class real_socket(socketwrapper.socketwrapper):
    '''Concrete implementation of socketwrapper'''
    def __init__(self, conn: socket.socket, logger: logging.Logger):
        self.conn = conn
        self.logger = logger
    def send(self, b: bytes):
        if self.conn != None:
            self.conn.send(b)
    def recv(self, count: int) -> bytes:
        return self.conn.recv(count)
    def close(self):
        if self.conn != None:
            self.conn.close()
            self.conn = None
    def isopen(self):
        return self.conn != None
    def wrap(self) -> bool:
        try:
            self.conn = target_tls.wrap(self.conn)
            return True
        except Exception as e:
            self.logger.warning(f'TLS wrap failed: {e}')
            return False
    def fileno(self) -> int:
        return self.conn.fileno()

class target(aswitch.aswitch):
    '''x3270 test target'''

    type = None
    servers = []
    exiting = False
    switch_to = {}      # Next type
    active_type = {}    # Current type
    previous_type = {}  # Previous type
    drain = {}          # True if switching requires an input drain step

    # Initialization.
    def __init__(self, port: int, opts: Dict[str, Any]):

        self.opts = oopts.oopts(opts)

        self.logger = logging.getLogger()
        ch = logging.StreamHandler()
        formatter = logging.Formatter('%(levelname)s:%(message)s')
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)
        self.logger.setLevel(self.opts.get('log', logging.WARNING))

        self.tls = self.opts.get('tls', target_tls.none)
        self.type = self.opts.get('type', 'menu')
        address = self.opts.get('address', '127.0.0.1')
        addr = ipaddress.ip_address(address)
        s = socket.socket(socket.AF_INET if addr.version == 4 else socket.AF_INET6)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((address, port))
        s.listen()
        self.logger.info(f'target: listening on {addr} port {port}')
        t = threading.Thread(target=self.accept, args=[s])
        t.start()
        self.servers.append(t)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.__del__()
        return

    # Shutdown.
    def __del__(self):
        self.exiting = True
        for t in self.servers:
            t.join()
        self.servers = []

    def close(self):
        '''Close the object'''
        self.__del__()

    def log_exception(self, instance: str, e: Exception):
        '''Log an exception'''
        self.logger.error(f'target:{instance} caught {type(e)}')
        for eline in traceback.format_exception(e):
            if eline.endswith('\n'):
                eline = eline[0:-1]
            self.logger.error(f'target:{eline}')

    # Accept connections and process them.
    def accept(self, listensocket: socket.socket):
        while not self.exiting:
            for u in self.servers:
                if not u.is_alive():
                    u.join()
                    self.servers.remove(u)
            r, _, _ = select.select([listensocket], [], [], 0.5)
            if r == []:
                continue
            good=True
            (conn, _) = listensocket.accept()
            if self.tls == target_tls.immediate:
                try:
                    conn = target_tls.wrap(conn)
                except Exception as e:
                    self.logger.error(f'target: TLS wrap failed: {e}')
                    conn.close()
                    good=False
            if good:
                t = threading.Thread(target=self.process_connection, args=[conn])
                t.start()
                self.servers.append(t)
        listensocket.close()

    def list(self) -> Dict[str,str]:
        '''Return the list of servers'''
        ret = {}
        for server in servers:
            ret[server] = servers[server].__doc__
        return ret

    def switch(self, peername: str, other: str, drain=False) -> str:
        '''Switch to a new server'''
        if other in servers:
            self.switch_to[peername] = other
            self.previous_type[peername] = self.active_type[peername]
            if drain:
                self.drain[peername] = True
            return None
        return f'No such service: {other}'

    def revert(self, peername: str):
        '''Revert to previous type'''
        if self.is_switched(peername):
            self.switch_to[peername] = self.previous_type[peername]
            self.previous_type.pop(peername)
            self.drain[peername] = True

    def is_switched(self, peername: str) -> bool:
        '''Test for being a switched session'''
        return peername in self.previous_type

    # Process a connection, asynchronously.
    def process_connection(self, conn: socket.socket):
        peer = conn.getpeername()
        peername = f'{peer[0]}/{peer[1]}'
        conn.settimeout(0.5)
        self.active_type[peername] = self.type
        if peername in self.switch_to:
            self.switch_to.pop(peername)
        self.logger.info(f'target:{peername}: new connection')
        while True:
            try:
                wrapper = real_socket(conn, self.logger)
                with servers[self.active_type[peername]](wrapper, self.logger, peername, self.tls == target_tls.negotiated, self, self.opts) as dynserver:
                    if dynserver.ready():
                        while not self.exiting and not peername in self.switch_to and wrapper.isopen():
                            try:
                                data = wrapper.recv(1024)
                            except TimeoutError:
                                continue
                            if data == b'':
                                # EOF
                                break
                            dynserver.process(data)
            except Exception as e:
                self.log_exception(peername, e)
            if not peername in self.switch_to:
                break

            # Switch to a new type.
            self.active_type[peername] = self.switch_to[peername]
            self.switch_to.pop(peername)
            self.logger.info(f'target:{peername} switching to {self.active_type[peername]}')

            if peername in self.drain:
                # Drain traffic for half a second
                self.logger.info(f'target:{peername} draining')
                self.drain.pop(peername)
                drained = False
                while not drained and not self.exiting and wrapper.isopen():
                    while True:
                        try:
                            data = wrapper.recv(1024)
                        except TimeoutError:
                            drained = True
                            break
            self.tls = target_tls.none # so it doesn't get negotiated again
        wrapper.close()
        self.active_type.pop(peername)
        if peername in self.previous_type:
            self.previous_type.pop(peername)
        self.logger.info(f'target:{peername} done')

# Trivial echo server.
class echo_server(server.server):
    '''Trivial echo server'''
    def __init__(self, conn: socketwrapper.socketwrapper, log: logging.Logger, peername: str, tls: bool, switch: aswitch.aswitch, opts: oopts.oopts):
        self.conn = conn
        pass
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass
    def __del__(self):
        pass

    def process(self, b: bytes):
        self.conn.send(b)
    def ready(self) -> bool:
        return True

    desc = 'Simple echo server'

servers = {
    'echo': echo_server,
    'ibmlink': tn3270_ibmlink.ibmlink,
    'menu': menu.menu,
    'menu-f': menu.menu_f,
    'menu-n': menu.menu_n,
    'menu-s': menu.menu_s,
    'menu-u': menu.menu_u,
    'snake': tn3270_snake.snake,
    'sruvm': tn3270_sruvm.sruvm,
    'uvvm': tn3270_uvvm.uvvm
}

if __name__ == '__main__':
    def argconv(**convs):
        def parse_argument(arg):
            if arg in convs:
                return convs[arg]
            else:
                msg = "invalid choice: {!r} (choose from {})"
                choices = ", ".join(sorted(repr(choice) for choice in convs.keys()))
                raise argparse.ArgumentTypeError(msg.format(arg,choices))
        return parse_argument

    parser = argparse.ArgumentParser(description='x3270 test target')
    parser.add_argument('--address', default='127.0.0.1', help='address to listen on')
    parser.add_argument('--port', type=int, default=8021, action='store', help='port to listen on')
    parser.add_argument('--type', default='menu-f', choices=servers.keys(), help='type of server [menu-f]')
    parser.add_argument('--log', default='WARNING', choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'], help='logging level [WARNING]')
    parser.add_argument('--tls', type=argconv(none=target_tls.none, immediate=target_tls.immediate,
                                            negotiated=target_tls.negotiated), default=target_tls.none,
                                            choices=['none', 'immediate', 'negotiated'], help='TLS support [none]')
    parser.add_argument('--tn3270e', action=argparse.BooleanOptionalAction, default=True, help='TN3270E support [on]')
    parser.add_argument('--bind', action=argparse.BooleanOptionalAction, default=True, help='TN3270E BIND support [on]')
    parser.add_argument('--elf', action=argparse.BooleanOptionalAction, default=False, help='IBM ELF support [off]')
    parser.add_argument('--devname', type=int, default=0, action='store', metavar='LIMIT', help='RFC 4777 devname limit [0]')
    opts = vars(parser.parse_args())
    with target(opts['port'], opts) as server:
        print('Press <Enter> to stop the server.')
        try:
            x = input()
        except KeyboardInterrupt:
            exit(0)
