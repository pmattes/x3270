#!/usr/bin/env python3
# Simple Python version of x3270if
#
# Copyright (c) 2017-2018 Paul Mattes.
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

"""x3270 host specification formatter"""

#import io
#import os
#import socket
#import sys
#import subprocess
#import time

_bad_host_chars = '@,[]='
_good_lu_chars = 'ABCDEFGHIJKLMNOPQRSTUVWYZabcdefghijklmnopqrstuvwxyz0123456789_-'
_bad_accept_chars = '@,[]=:'

class host_specification:
    """Host specification with proper formatting"""
    def __init__(self,host_name,port=23,logical_unit_names=[],tls_tunnel=False,validate_host_certificate=True,accept_name=None):
        """Initialize an instance

           Args:
              host_name (str): Host name or IP address.
              port (int, optional): TCP port number.
              logical_unit_names (list of str, optional): Logical unit names.
              tls_tunnel (bool, optional): Set up a TLS tunnel.
              validate_host_certificate (bool, optional): Validate the host TLS certificate.
              accept_name (str, optional): Host name to accept in the host TLS certificate.
       """
        self.host_name = host_name
        self.port = port
        self.logical_unit_names = logical_unit_names
        self.tls_tunnel = tls_tunnel
        self.validate_host_certificate = validate_host_certificate
        self.accept_name = accept_name

    @property
    def host_name(self):
        """Host name or IP address"""
        return self._host_name
    @host_name.setter
    def host_name(self,value):
        if (any(ch in value for ch in _bad_host_chars)):
            raise ValueError("host_name contains invalid character(s)")
        self._host_name = value

    @property
    def port(self):
        """TCP port number"""
        return self._port
    @port.setter
    def port(self,value):
        self._port = int(value)
        if (self._port < 1 or self._port > 0xffff):
            raise ValueError("Invalid port value")

    @property
    def logical_unit_names(self):
        """List of Logical Unit (LU) names"""
        return self._logical_unit_names
    @logical_unit_names.setter
    def logical_unit_names(self,value):
        for lu in value:
            if (any(ch not in _good_lu_chars for ch in lu)):
                raise ValueError("Logical unit name contains invalid character(s)")
        self._logical_unit_names = value

    @property
    def accept_name(self):
        """Name to accept in the host TLS certificate"""
        return self._accept_name
    @accept_name.setter
    def accept_name(self,value):
        if (value == None):
            self._accept_name = value
            return
        for lu in value:
            if (any(ch in value for ch in _bad_accept_chars)):
                raise ValueError("Accept name contains invalid character(s)")
        self._accept_name = value

    def __str__(self):
        if (self.host_name == None):
            return ''
        r = ''
        if (self.tls_tunnel):
            r += "L:"
        if (not self.validate_host_certificate):
            r += "Y:"
        if (self.logical_unit_names != []):
            r += ','.join(self.logical_unit_names) + '@'
        if (':' in self.host_name):
            r += '[' + self.host_name + ']'
        else:
            r += self.host_name
        if (self.port != 23):
            r += ':' + str(self.port)
        if (self.accept_name != None):
            r += '=' + self.accept_name
        return r

