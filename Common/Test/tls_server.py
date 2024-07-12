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
# TLS playback server

import ssl
import Common.Test.cti as cti
import Common.Test.telnet as telnet
import Common.Test.playback as playback

# TLS playback server.
class tls_server(playback.playback):
    '''TLS server'''
    clear_conn = None
    def __init__(self, cert: str, key: str, cti: cti.cti, trace_file: str, port: int, ipv6=False):
        # Set up the TLS context.
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)

        # Do common initialization.
        playback.playback.__init__(self, cti, trace_file, port, ipv6)

    # Cleanup.
    def __exit__(self, exc_type, exc_value, traceback):
        if self.clear_conn != None:
            self.clear_conn.close()
            self.clear_conn = None
        playback.playback.__exit__(self, exc_type, exc_value, traceback)

    def limit_tls13(self):
        '''Prohibit TLS 1.3'''
        self.context.maximum_version = ssl.TLSVersion.TLSv1_2

    def wrap(self):
        '''Wrap an accepted connection'''
        self.wait_accept()
        self.clear_conn = self.conn
        self.clear_conn.settimeout(2)
        self.conn = self.context.wrap_socket(self.clear_conn, server_side=True)

    def starttls(self, timeout=2):
        '''Do STARTTLS negotiation'''
        self.wait_accept()
        # Send IAC DO STARTTLS.
        self.conn.send(telnet.iac + telnet.do + telnet.startTls)
        # Make sure they respond with IAC WILL STARTTLS and the right SB.
        startTlsSb = telnet.iac + telnet.sb + telnet.startTls + telnet.follows + telnet.iac + telnet.se
        expectStartTls = telnet.iac + telnet.will + telnet.startTls + startTlsSb
        data = self.nread(len(expectStartTls), timeout)
        assert data == expectStartTls
        # Send the SB.
        self.conn.send(startTlsSb)
        # Wrap the clear socket with TLS.
        self.wrap()

# TLS send server.
class tls_sendserver(cti.sendserver):
    '''TLS send server'''
    clear_conn = None
    def __init__(self, cert: str, key: str, cti: cti.cti, port: int, ipv6=False):
        self.cti = cti

        # Set up the TLS context.
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)

        # Do common initialization.
        super().__init__(cti, port, ipv6)

    def __enter__(self):
        return self

    # Cleanup.
    def __exit__(self, exc_type, exc_value, traceback):
        if self.clear_conn != None:
            self.clear_conn.close()
            self.clear_conn = None
        super().close()

    def wrap(self):
        '''Wrap an accepted connection'''
        self.cti.try_until(lambda: (self.conn != None), 2, 'emulator did not connect')
        self.clear_conn = self.conn
        self.clear_conn.settimeout(2)
        self.conn = self.context.wrap_socket(self.clear_conn, server_side=True)

    def starttls(self, timeout=2):
        '''Do STARTTLS negotiation'''
        self.wait_accept()
        # Send IAC DO STARTTLS.
        self.conn.send(telnet.iac + telnet.do + telnet.startTls)
        # Make sure they respond with IAC WILL STARTTLS and the right SB.
        startTlsSb = telnet.iac + telnet.sb + telnet.startTls + telnet.follows + telnet.iac + telnet.se
        expectStartTls = telnet.iac + telnet.will + telnet.startTls + startTlsSb
        data = self.nread(len(expectStartTls), timeout)
        assert data == expectStartTls
        # Send the SB.
        self.conn.send(startTlsSb)
        # Wrap the clear socket with TLS.
        self.wrap()
