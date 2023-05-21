#!/usr/bin/env python3
#
# Copyright (c) 2023 Paul Mattes.
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
# x3270 test target host, abstracted TN3270 layer.

from abc import ABC, abstractmethod

import ds
import tn3270e_proto

# Abstract tn3270 class, used to protect tn3270 and prevent circular imports.
class atn3270(ABC):
    '''Abstract tn3270 class'''
    @abstractmethod
    def e_sb(data: bytes):
        pass
    @abstractmethod
    def e_to_terminal(data: bytes):
        '''Send data to terminal'''
        pass
    @abstractmethod
    def e_to_host(data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Send data to host'''
        pass
    @abstractmethod
    def e_in3270(in3270: bool):
        '''Switch 3270 modes'''
        pass
    @abstractmethod
    def e_get_termid() -> str:
        pass
    @abstractmethod
    def e_get_system() -> str:
        pass
    @abstractmethod
    def e_warning(module: str, msg: str):
        pass
    @abstractmethod
    def e_info(module: str, msg: str):
        pass
    @abstractmethod
    def e_debug(module: str, msg: str):
        pass
    @abstractmethod
    def e_set_ttype(ttype: str) -> ds.dinfo:
        pass
