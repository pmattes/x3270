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
# stunnel-like relay that can negotiate TELNET STARTTLS.

import argparse
from datetime import datetime
import logging.handlers
import time
import ipaddress
import logging
import select
import signal
import socket
import threading
import traceback
from typing import Dict, Any

import oopts
import socketwrapper
from starttls import starttls_layer
import target_tls
import ttelnet

class real_socket(socketwrapper.socketwrapper):
    '''Concrete implementation of socketwrapper'''
    def __init__(self, conn: socket.socket, logger: logging.Logger):
        self.conn = conn
        self.logger = logger
    def send(self, b: bytes):
        self.conn.send(b)
    def recv(self, count: int, flags: int) -> bytes:
        return self.conn.recv(count, flags)
    def close(self):
        if self.conn != None:
            self.conn.close()
            self.conn = None
    def isopen(self):
        return self.conn != None
    def wrap(self, cert: str, key: str) -> bool:
        try:
            self.conn = target_tls.wrap(self.conn, cert, key)
            return True
        except Exception as e:
            self.logger.warning(f'TLS wrap failed: {e}')
            return False
    def fileno(self) -> int:
        return self.conn.fileno()

class relay():
    '''TELNET STARTTLS stunnel relay'''

    exiting = False
    servers = []

    # Initialization.
    def __init__(self, port: int, opts: Dict[str, Any]):

        self.opts = oopts.oopts(opts)
        self.logger = logging.getLogger()
        self.relaysocket = None
        if (self.opts.get('logfile') != None and self.opts.get('logfile') != 'stdout'):
            ch = logging.handlers.RotatingFileHandler(self.opts.get('logfile'), maxBytes=128*1024, backupCount=10)
        else:
            ch = logging.StreamHandler()
        formatter = logging.Formatter('%(asctime)sZ %(levelname)s %(message)s')
        formatter.converter = time.gmtime
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)
        logLevel = self.opts.get('log', logging.WARNING)
        if logLevel == 'NONE':
            self.logger.setLevel(100)
        else:
            self.logger.setLevel(logLevel)

        self.tls = self.opts.get('tls', target_tls.none)
        address = self.opts.get('fromaddress', '0.0.0.0')
        addr = ipaddress.ip_address(address)
        s = socket.socket(socket.AF_INET if addr.version == 4 else socket.AF_INET6)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((address, port))
        s.listen()
        addr_text = str(addr) if addr.version == 4 else f'[{addr}]'
        self.logger.info(f'st-relay: listening on {addr_text}/{port}')
        t = threading.Thread(target=self.accept, args=[s], name='listen')
        t.start()
        self.servers.append(t)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.logger.info(f'st-relay: shutting down')
        self.exiting = True
        for t in self.servers:
            self.logger.info(f'st-relay: shutting down {t}')
            t.join()
        self.servers = []
        return

    # Shutdown.
    def __del__(self):
        pass

    def close(self):
        pass

    def log_exception(self, instance: str, e: Exception):
        '''Log an exception'''
        self.logger.error(f'st-relay:{instance} caught {type(e)}')
        for eline in traceback.format_exception(e):
            if eline.endswith('\n'):
                eline = eline[0:-1]
            self.logger.error(f'st-relay:{eline}')

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
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            if self.tls == target_tls.immediate:
                try:
                    conn = target_tls.wrap(conn, self.opts['cert'], self.opts['key'])
                except Exception as e:
                    self.logger.error(f'st-relay: TLS wrap failed: {e}')
                    conn.close()
                    good=False
            if good:
                peer = conn.getpeername()
                peer_address = ('[' + peer[0] + ']') if ':' in peer[0] else peer[0]
                peername = f'{peer_address}/{peer[1]}'
                t = threading.Thread(target=self.process_connection, args=[conn, peername], name=peername)
                t.start()
                self.servers.append(t)
        listensocket.close()

    # Process a connection, asynchronously.
    def process_connection(self, conn: socket.socket, peername: str):
        conn.settimeout(0.5) # So we can shut down when the service shuts down, and time out STARTTLS negotiation
        self.logger.info(f'st-relay:{peername}: new connection')
        address = self.opts.get('toaddress')
        addr = ipaddress.ip_address(address)
        port = self.opts.get('toport', '3270')

        # Create the connection to the real server.
        relaysocket = socket.socket(socket.AF_INET if addr.version == 4 else socket.AF_INET6)
        try:
            relaysocket.connect((address, int(port)))
        except Exception as e:
            self.logger.error(f'st-relay:{peername}: relay connect to {address}/{port} failed: {e}')
            conn.send(b'Relay connection failed\r\n')
            conn.shutdown(socket.SHUT_RDWR)
            conn.close()
            return

        wrapper = real_socket(conn, self.logger)
        with starttls_layer(wrapper, self.logger, peername, self.tls == target_tls.negotiated, self.opts) as starttls:
            with ttelnet.ttelnet(wrapper, self.logger, peername, starttls) as telnet:
                try:
                    if starttls.ready():
                        start_time = datetime.now()
                        while not self.exiting and wrapper.isopen():
                            if not starttls.negotiation_complete():
                                now = datetime.now()
                                if (now - start_time).seconds > 5:
                                    self.logger.error(f'st-relay:{peername}: negotiation timeout')
                                    break
                            rfds = [wrapper]
                            if starttls.negotiation_complete():
                                rfds.append(relaysocket)
                            r, _, _ = select.select(rfds, [], [], 0.5)
                            if r == []:
                                continue
                            if wrapper in r:
                                data = wrapper.recv(1024, 0)
                                if data == b'':
                                    self.logger.info(f'st-relay:{peername}: EOF')
                                    break
                                if starttls.negotiation_complete():
                                    relaysocket.send(data)
                                else:
                                    telnet.process(data)
                            if relaysocket in r:
                                data = relaysocket.recv(1024)
                                if data == b'':
                                    self.logger.info(f'st-relay:{peername}: relay EOF')
                                    break
                                wrapper.send(data)
                except Exception as e:
                    self.log_exception(peername, e)
        wrapper.close()
        relaysocket.close()
        self.logger.info(f'st-relay:{peername}: done')

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
    exit_event = threading.Event()
    def exit_signal(signum, frame):
        exit_event.set()

    parser = argparse.ArgumentParser(description='TELNET STARTTLS wrapper relay')
    parser.add_argument('--cert', default=None, action='store', help='server certificate path', required=True)
    parser.add_argument('--key', default=None, action='store', help='server key path', required=True)
    parser.add_argument('--fromaddress', default='::', help='address to listen on (::)')
    parser.add_argument('--fromport', type=int, default=8023, action='store', help='port to listen on (8023)')
    parser.add_argument('--toaddress', default='::1', help='address to connect to (::1)')
    parser.add_argument('--toport', type=int, default=3270, action='store', help='port to connect to (3270)')
    parser.add_argument('--log', default='WARNING', choices=['NONE', 'DEBUG', 'INFO', 'WARNING', 'ERROR'], help='logging level (WARNING)')
    parser.add_argument('--logfile', default=None, action='store', help='pathname of log file (stdout)')
    parser.add_argument('--tls', type=argconv(none=target_tls.none, immediate=target_tls.immediate, negotiated=target_tls.negotiated), default=target_tls.negotiated, help='TLS type {none,immediate,negotiated} (negotiated)')
    opts = vars(parser.parse_args())
    signal.signal(signal.SIGINT, exit_signal)
    signal.signal(signal.SIGTERM, exit_signal)
    with relay(opts['fromport'], opts) as server:
        exit_event.wait()
