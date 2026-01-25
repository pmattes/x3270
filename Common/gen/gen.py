#!/usr/bin/env python3
#
# Copyright (c) 2022-2026 Paul Mattes.
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

import sys
import subprocess

from common import *
import field
import read
import telnet
import tn3270e
import write
import wsf

class Gen(GenCommon):

    # Instance variables: input line number, TN3270E sequence number.
    line = 1

    # Instance initialization.
    def __init__(self, rows=24, columns=80, codepage="cp037"):
        if rows < 24:
            raise GenValueError('rows must be >= 24')
        self.max_rows = rows
        if columns < 80:
            raise GenValueError('columns must be >= 80')
        self.max_columns = columns
        self.ebcdic_codepage = codepage

    # Sets the number of rows.
    def rows(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('rows takes one argument')
        self.max_rows = int(args[0])
        assert self.max_rows >= 24
        return ''

    # Sets the number of columns.
    def columns(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('columns takes one argument')
        self.max_columns = int(args[0])
        assert self.max_columns >= 80
        return ''

    # Sets the code page for EBCDIC translation.
    def codepage(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('codepage takes one argument')
        self.ebcdic_codepage = args[0]
        return ''

    # text string (EBCDIC text)
    def text(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('text takes 1 argument')
        p = subprocess.Popen(['iconv', '-f', 'utf8', '-t', self.ebcdic_codepage],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        p.stdin.write(args[0].encode('utf8'))
        out = p.communicate()[0]
        p.wait(timeout=2)
        xout = ''.join([f'{c:02x}' for c in out])
        if xout.startswith('0e'):
            xout = xout[2:]
        if xout.endswith('0f'):
            xout = xout[:-2]
        return quote(xout)

    # atext string (ASCII text)
    def atext(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('atext takes 1 argument')
        return atext_str(args[0])

    # raw bytes
    def raw(self, *argx):
        args = argx[0] if len(argx) == 1 and isinstance(argx[0], list) else argx
        if len(args) != 1:
            raise GenSyntaxError('raw takes 1 argument')
        return args[0]

    # Flushes the current record
    def flush(self, args):
        if len(args) != 0:
            raise GenSyntaxError('flush takes 0 arguments')
        return '&flush'

    # Function dispatch table
    funcs = {
        'atext': atext,
        'cmd.ew': write.ew,
        'cmd.ewa': write.ewa,
        'cmd.rb': read.rb,
        'cmd.rm': read.rm,
        'columns': columns,
        'codepage': codepage,
        'flush': flush,
        'ord.ic': write.ic,
        'ord.mf': field.mf,
        'ord.pt': write.pt,
        'ord.ra': write.ra,
        'ord.sa': field.sa,
        'ord.sba': write.sba,
        'ord.sf': field.sf,
        'ord.sfe': field.sfe,
        'raw': raw,
        'rows': rows,
        'telnet.do': telnet.do,
        'telnet.eor': telnet.eor,
        'telnet.sb': telnet.sb,
        'telnet.se': telnet.se,
        'text': text,
        'tn3270e': tn3270e.tn3270e,
        'tn3270e.connect': tn3270e.connect,
        'tn3270e.device-type': tn3270e.device_type,
        'tn3270e.functions': tn3270e.functions,
        'tn3270e.send': tn3270e.send,
        'wsf.set-reply-mode': wsf.set_reply_mode,
    }

    # For each function that isn't a member of gen, define a method with the same name (with . and - replaced by _).
    # This allows Python code to run this class programmatically, as opposed to needing to generate text for this
    # class to parse.
    # There is probably a safer way to do this.
    for d in [f for f in funcs.keys() if '.' in f]:
        external_name = d.replace('.', '_').replace('-','_')
        real_name = funcs[d].__module__ + '.' + funcs[d].__name__
        exec(f"""def {external_name}(self, *argx):
                return {real_name}(self, list(argx))
                """)

    def tn3270e(self, *argx):
        return tn3270e.tn3270e(self, list(argx))

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
            raise GenSyntaxError('unclosed literal')
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
                acc = self.funcs[tokens[0]](self, tokens[1:])
            except GenSyntaxError as e:
                raise RuntimeError(f'{filename}, line {self.line}: {str(e)}') from e
            except GenValueError as e:
                raise RuntimeError(f'{filename}, line {self.line}: {str(e)}') from e
            except KeyError as e:
                raise RuntimeError(f'{filename}, line {self.line}: Unknown keyword {str(e)}') from e
            except Exception as e:
                raise RuntimeError(f'{filename}, line {self.line}: {type(e)} {str(e)}') from e
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
            print('Usage: gen filename', file=sys.stdout)
            exit(1)
        try:
            self.process_file(argv[1])
        except Exception as e:
            print(str(e), file=sys.stderr)
            exit(1)

if __name__ == '__main__':
    d = Gen()
    d.main(sys.argv)
