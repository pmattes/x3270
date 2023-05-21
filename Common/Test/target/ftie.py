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
# Fault-tolerant IntEnum class.

import enum
from typing import Any

# An extension class for IntEnums, which allows undefined integer values.
# Given an IntEnum class x with member FOO of value 3, you can create ftie(x.FOO)
# or ftie(3, x) and they will be equivalent.
# You can also create ftie(27, x) when x does not have a member with value 27, and
# it will:
# - succeed
# - compare equal to ftie(27, x)
# - display as Unknown[27]
# Eventually python will support a KEEP flag for IntEnum and this can go away.
class ftie():

    def __init__(self, value: Any, e: type = None):
        '''Initialize'''
        if e == None:
            if not isinstance(value, enum.IntEnum):
                raise Exception('ftie requires an IntEnum')
            self.real = value
            self.alt = None
            self.name = self.__str__()
            return
        try:
            self.real = e(value)
            self.alt = None
        except Exception as ex:
            self.alt = value
        self.name = self.__str__()

    def __str__(self):
        '''Convert to string'''
        return f'Unknown-{self.alt}' if self.alt != None else self.real.name

    def __int__(self):
        '''Convert to integer'''
        return self.alt if self.alt != None else int(self.real)

    def __eq__(self, other):
        '''Compare for equality'''
        if type(other) == ftie:
            if self.alt != None:
                if other.alt == None:
                    return False
                if self.alt == other.alt:
                    return True
            return self.real == other.real
        if self.alt != None:
            return self.alt == other
        return self.real == other

    def __hash__(self):
        '''Compute hash'''
        if self.alt != None:
            return self.alt.__hash__()
        return self.real.__hash__()

if __name__ == '__main__':
    class test_enum(enum.IntEnum):
        YES = 1,
        NO = 2,

    x = ftie(1, test_enum)
    print('For 1:', x)
    x = ftie(3, test_enum)
    print('For 3:', x)
    x = ftie(test_enum.YES)
    print('For YES:', x)
    y = [ ftie(test_enum.YES), ftie(3, test_enum) ]
    if ftie(test_enum.YES) in y:
        print('Yes!')
    else:
        print('No!')
