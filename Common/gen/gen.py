#!/usr/bin/env python3
#
# Copyright (c) 2022 Paul Mattes.
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
# 3270 screen generator

import functools
import sys
import subprocess

class gen():
    class GenSyntaxError(SyntaxError):
        # Constructor method
        def __init__(self, value):
            self.value = value
        # __str__ display function
        def __str__(self):
            return(repr(self.value))

    class GenValueError(ValueError):
        # Constructor method
        def __init__(self, value):
            self.value = value
        # __str__ display function
        def __str__(self):
            return(repr(self.value))

    # Table used to encode values as 'printable'.
    code_table = [
        '40', 'c1', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7',
        'c8', 'c9', '4a', '4b', '4c', '4d', '4e', '4f',
        '50', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7',
        'd8', 'd9', '5a', '5b', '5c', '5d', '5e', '5f',
        '60', '61', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7',
        'e8', 'e9', '6a', '6b', '6c', '6d', '6e', '6f',
        'f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7',
        'f8', 'f9', '7a', '7b', '7c', '7d', '7e', '7f'
    ]

    # Map from color names to their encoded values
    color_map = {
        'default': '00',
        'neutralBlack': 'f0',
        'blue': 'f1',
        'red': 'f2',
        'pink': 'f3',
        'green': 'f4',
        'turquoise': 'f5',
        'yellow': 'f6',
        'neutralWhite': 'f7',
        'black': 'f8',
        'deepBlue': 'f9',
        'orange': 'fa',
        'purple': 'fb',
        'paleGreen': 'fc',
        'paleTurquoise': 'fd',
        'grey': 'fe',
        'white': 'ff' 
    }

    # Map from highlighting attributes to their encoded values
    highlighting_map = {
        'default': 0,
        'normal': 0xf0,
        'blink': 0xf1,
        'reverse': 0xf2,
        'underscore': 0xf4,
        'intensify': 0xf8
    }

    # Maps an argument
    def map_arg(self, arg, map, type):
        if not arg in map:
            raise self.GenValueError(f'Unkown {type} {arg}')
        return map[arg]

    # Map from TELNET option names to values.
    telnet_opts = {
        'tn3270e': '28'
    }

    # Instance variables. Input line number, current rows and columns, maximim rows and columns.
    line = 1
    rows = 24
    columns = 80
    max_rows = 24
    max_columns = 80

    # Instance initialization.
    def __init__(self, rows=24, columns=80):
        if rows < 24:
            raise self.GenValueError('rows must be >= 24')
        self.max_rows = rows
        if columns < 80:
            raise self.GenValueError('columns must be >= 80')
        self.max_columns = columns

    # Sets the number of rows.
    def set_rows(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('rows takes one argument')
        self.max_rows = int(args[0])
        assert self.max_rows >= 24
        return ''

    # Sets the number of columns.
    def set_columns(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('colums takes one argument')
        assert len(args) == 1
        self.max_columns = int(args[0])
        assert self.max_columns >= 80
        return ''

    # Makes non-TELNET data IAC safe.
    def quote(self, hex):
        return ''.join([b+'ff' if b == 'ff' else b for b in [hex[i:i+2] for i in range(0, len(hex), 2)]])

    # tn3270e header types
    tn3270e_type = {
        '3270-data': '00',
        'scs-data': '01',
        'response': '02',
        'bind-image': '03',
        'unbind': '04',
        'nvt-data': '05',
        'request': '06',
        'sscp-lu-data': '07',
        'print-eoj': '08',
        'bid': '09'
    }

    # tn3270e request flags
    tn3270e_req = {
        'none': '00',
        'err-cond-cleared': '00',
        'send-data': '01',
        'keyboard-restore': '02',
        'signal': '04'
    }

    # tn3270e response flags
    tn3270e_rsp = {
        'no-response': '00',
        'error-response': '01',
        'always-response': '02',
        'positive-response': '00',
        'negatve-response': '01',
        'sna-sense': '04'
    }

    # tn3270e type req rsp seq
    def tn3270e(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 4:
            raise self.GenSyntaxError('tn3270e takes 4 arguments')
        ret = self.quote(self.map_arg(args[0], self.tn3270e_type, 'type') + \
              self.map_arg(args[1], self.tn3270e_req, 'request flag') + \
              self.map_arg(args[2], self.tn3270e_rsp, 'response flag') + \
              f'{int(args[3]):04x}')
        return self.quote(ret)

    # do opt
    def telnet_do(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('do takes 1 argument')
        return 'fffd' + self.telnet_opts[args[0]]

    # sb opt
    def telnet_sb(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('sb takes 1 argument')
        return 'fffa' + self.telnet_opts[args[0]]

    # se
    def telnet_se(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 0:
            raise self.GenSyntaxError('se takes 0 arguments')
        return 'fff0'

    # ic
    def ic(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 0:
            raise self.GenSyntaxError('ic takes 0 arguments')
        return '13'

    # Encodes a buffer address
    # row and column are 1-origin
    def baddr(self, row, col):
        if row <= 0:
            raise self.GenValueError('row must be >0')
        if row > self.rows:
            raise self.GenValueError(f'row must be <={self.rows}')
        if col <= 0:
            raise self.GenValueError('column must be >0')
        if col > self.columns:
            raise self.GenValueError(f'column mustr be <={self.columns}')
        baddr = ((row - 1) * self.columns) + (col - 1)
        if self.rows * self.columns > 0x1000:
            return f'{(baddr >> 8) & 0x3f:02x}{(baddr & 0xff):02x}'
        else:
            return self.code_table[(baddr >> 6) & 0x3f] + self.code_table[baddr & 0x3f]

    # sba row col
    def sba(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 2:
            raise self.GenSyntaxError('sb takes 2 arguments')
        return self.quote('11' + self.baddr(int(args[0]), int(args[1])))

    # Map for WCC flags
    wcc_map = {
        'reset': 0x40,
        'startprinter': 0x08,
        'alarm': 0x04,
        'restore': 0x02,
        'resetmdt': 0x01
    }

    # Encodes WCC flags
    def encode_wcc_flags(self, flags):
        x = 0
        for f in flags.split(','):
            if not f in self.wcc_map:
                raise self.GenValueError(f'Unknown WCC flag {f}')
            x |= self.wcc_map[f]
        if x < 0x40:
            return self.code_table[x]
        else:
            return self.code_table[x & 0x3f]

    # Map for SF flags.
    sf_map = {
        'normal': 0x00,
        'protect': 0x20,
        'numeric': 0x10,
        'modify': 0x01,
        'sel': 0x04,
        'high': 0x08,
        'zero': 0x0c,
        'skip': 0x30
    }
    # Encodes SF flags.
    def encode_sf_flags(self, flags):
        x = 0
        for f in flags.split(','):
            if not f in self.sf_map:
                raise self.GenValueError(f'Unknown SF flag {f}')
            x |= self.sf_map[f]
        return self.code_table[x & 0x3f]

    # ew flags
    def ew(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) > 1:
            raise self.GenSyntaxError('ew takes 0 or 1 arguments')
        self.rows = 24
        self.columns = 80
        if len(args) == 0:
            return self.quote('f5' + self.code_table[0])
        return self.quote('f5' + self.encode_wcc_flags(args[0]))

    # ewa flags
    def ewa(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) > 1:
            raise self.GenSyntaxError('ewa takes 0 or 1 arguments')
        self.rows = self.max_rows
        self.columns = self.max_columns
        if len(args) == 0:
            return self.quote('7e' + self.code_table[0])
        return self.quote('7e' + self.encode_wcc_flags(args[0]))

    # sf flags
    def sf(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('sf takes 1 argument')
        return self.quote('1d' + self.encode_sf_flags(args[0]))

    # highlighting x,y,z
    def highlighting(self, h: str):
        return '{:#04x}'.format(functools.reduce(lambda x, y: x | y, [self.highlighting_map[hh] for hh in h.split(',')]))[2:]

    # sfe type value ...
    def sfe_sa_mf(self, keyword, order, args, include_count=True):
        if len(args) == 0:
            raise self.GenSyntaxError(f'{keyword} takes at least 1 argument')
        ret = []
        while len(args) > 0:
            if args[0] == '3270':
                ret.append('c0' + self.encode_sf_flags(args[1]))
                args = args[2:]
            elif args[0] == 'highlighting':
                ret.append('41' + self.highlighting(args[1]))
                args = args[2:]
            elif args[0] == 'fg':
                ret.append('42' + self.color_map[args[1]])
                args = args[2:]
            elif args[0] == 'charset':
                ret.append('43' + args[1])
                args = args[2:]
            elif args[0] == 'bg':
                ret.append('45' + self.color_map[args[1]])
                args = args[2:]
            elif args[0] == 'all':
                ret.append('00' + args[1])
                args = args[2:]
            else:
                print(f'Line {self.line}: Unknow sfe attribute {args[0]}')
                exit(1)
        res = order
        if include_count:
            res += f'{len(ret):02x}'
        res += ''.join(ret)
        return self.quote(res)

    # sfe type value...
    def sfe(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        return self.sfe_sa_mf('sfe', '29', args)
    
    # sa type value...
    def sa(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 2:
            raise self.GenSyntaxError('sa takes 2 arguments')
        return self.sfe_sa_mf('sa', '28', args, include_count=False)

    # ra row col char
    def ra(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 3:
            raise self.GenSyntaxError('ra takes 3 arguments')
        return self.quote('3c' + self.baddr(int(args[0]), int(args[1])) + args[2])

    # pt
    def pt(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 0:
            raise self.GenSyntaxError('pt takes 0 arguments')
        return '05'

    # mf type value...
    def mf(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        return self.sfe_sa_mf('mf', '2c', args)

    # text string (EBCDIC text)
    def text(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('text takes 1 argument')
        p = subprocess.Popen(['dd', 'conv=ebcdic'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        p.stdin.write(args[0].encode('utf8'))
        out = p.communicate()[0]
        p.wait(timeout=2)
        return self.quote(''.join([f'{c:2x}' for c in out]))

    # atext string (ASCII text)
    def atext(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('atext takes 1 argument')
        return ''.join([f'{c:2x}' for c in args[0].encode('utf8')])

    # raw bytes
    def raw(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise self.GenSyntaxError('raw takes 1 argument')
        return args[0]

    # eor
    def eor(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 0:
            raise self.GenSyntaxError('eor takes 0 arguments')
        return 'ffef'

    # Flushes the current record
    def flush(self, args):
        if len(args) != 0:
            raise self.GenSyntaxError('flush takes 0 arguments')
        return '&flush'

    # function dispatch table
    funcs = {
        'rows': set_rows,
        'columns': set_columns,
        'telnet.do': telnet_do,
        'telnet.sb': telnet_sb,
        'telnet.se': telnet_se,
        'ord.ic': ic,
        'ord.sba': sba,
        'cmd.ew': ew,
        'cmd.ewa': ewa,
        'ord.sf': sf,
        'ord.sfe': sfe,
        'ord.sa': sa,
        'ord.ra': ra,
        'ord.pt': pt,
        'ord.mf': mf,
        'text': text,
        'atext': atext,
        'tn3270e': tn3270e,
        'raw': raw,
        'telnet.eor': eor,
        'flush': flush
    }

    # Splits a line into tokens, observing double quotes, which may
    # be doubled to include them, and taking care of '#' comments
    def token_split(self, line):
        out = []
        inq = False  # inside a double quote
        inqq = False # saw a possible closing or doubled quote
        for d in line:
            c = ord(d)
            if inqq:
                # Saw a possible closing quote
                if c == ord('"'):
                    # Doubled.
                    out.append(c)
                    inqq = False
                else:
                    out.append(c)
                    inqq = False
                    inq = False
            elif inq:
                # Inside a quoted literal
                if c == ord('"'):
                    inqq = True
                elif c == ord(' '):
                    # Hide spaces as \ue000.
                    out.append(0xe000)
                else:
                    out.append(c)
            elif c == ord('"'):
                inq = True
            elif c == ord('#'):
                # Comment, outside of a quoted literal.
                break
            else:
                # Ordinary text.
                out.append(c)
        if inq and not inqq:
            raise self.GenSyntaxError('unclosed literal')
        s = ''.join([chr(c) for c in out]).split()
        ret = [c.replace('\ue000', ' ') for c in s]
        return ret

    # Dumps the output in trace/playback format
    def dump(self, out):
        offset = 0
        while out != '':
            oss = f'{offset:x}'
            print(f'< 0x{oss:3} ', end='')
            print(out[0:64])
            out = out[64:]
            offset += 32
    
    # Processes a file.
    def process_file(self, filename):
        f = open(filename, 'r')
        lines = f.readlines()
        out = ''
        self.line = 0
        last = None
        for line in lines:
            self.line += 1
            if line.startswith('#'):
                print('//', line, end='')
                continue
            tokens = self.token_split(line)
            if len(tokens) == 0:
                continue
            try:
                if not tokens[0] in self.funcs:
                    raise self.GenSyntaxError(f'No such keyword: {tokens[0]}')
                acc = self.funcs[tokens[0]](self, tokens[1:])
            except self.GenSyntaxError as e:
                raise RuntimeError(f'{filename}, line {self.line}: {str(e)}') from e
            except self.GenValueError as e:
                raise RuntimeError(f'{filename}, line {self.line}: {str(e)}') from e
            if acc == '&flush':
                self.dump(out)
                out = ''
            else:
                print('//', line, end='')
                out += acc
        self.dump(out)

    # Main function
    # Parameter is the name of the file to read
    def main(self, argv):
        if len(argv) != 2:
            print('Usage: gen.py filename', file=sys.stdout)
            exit(1)
        try:
            self.process_file(argv[1])
        except Exception as e:
            print(str(e), file=sys.stderr)
            exit(1)

if __name__ == '__main__':
    d = gen()
    d.main(sys.argv)
