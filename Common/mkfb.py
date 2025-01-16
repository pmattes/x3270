#!/usr/bin/env python3
#
# Copyright (c) 2025 Paul Mattes.
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
# Utility to create RDB string definitions from a simple #ifdef'd .ad file.
#  mkfb [-c] [-o outfile] [infile...]

from enum import Flag
import os
import sys

# #ifdef bit masks.
class Mode(Flag):
    NONE = 0
    COLOR = 0x1
    APL = 0x2
    X3270_DBCS = 0x4
    _WIN32 = 0x8
    MASK = 0xfff

# Globals.
cmode = False
is_defined = Mode.COLOR | Mode.APL | Mode.X3270_DBCS
is_undefined = Mode.NONE

class Ss:
    '''#ifdef stack element'''
    def __init__(self):
        self.ifdefs = Mode.NONE
        self.ifndefs = Mode.NONE
        self.lno = 0

# Set up ss (the #ifdef nesting stack) as an array for now.
# Clearly this should be something more Pythonic.
SSSZ = 10
ss = [Ss() for i in range(SSSZ)]
ssp = 0

class FilteredRecord:
    '''#ifdef temp'''
    def __init__(self, ifdefs: Mode, ifndefs: Mode, lno: int, filename: str, value: str):
        self.ifdefs = ifdefs
        self.ifndefs = ifndefs
        self.lno = lno
        self.filename = os.path.basename(filename)
        self.value = value

# Dictionary mapping #ifdef symbols to bit masks.
parts = {
    Mode.COLOR.name: Mode.COLOR,
    Mode.APL.name: Mode.APL,
    Mode.X3270_DBCS.name: Mode.X3270_DBCS,
    Mode._WIN32.name: Mode._WIN32
}

class OutBuffer:
    n_per_line = 19
    def __init__(self):
        self.text = []
        self.n_out = 0
    def append(self, text: str):
        '''Append a line of text'''
        self.text.append(text)
    def emit(self, c:str):
        '''Emit a byte, with nice formatting'''
        if self.n_out >= OutBuffer.n_per_line:
            self.append('')
            self.n_out = 0
        self.text[-1] += f'{ord(c):3d},'
        self.n_out += 1

def mkfb(infiles, ofile: str):
    '''Generate the fallback files'''

    last_continue = False
    lno = 0
    ssp = 0

    is_undefined = Mode.COLOR | (~is_defined & Mode.MASK)

    # Start accumulating filtered output.
    filtered = []

    # Do #ifdef, comment and whitespace processing first.
    while True:
        if infiles != None:
            if infiles == []:
                break
            in_file = open(infiles[0])
            filename = infiles[0]
            infiles = infiles[1:]
            lno = 0
        else:
            in_file = sys.stdin
            filename = 'standard input'

        # Process.
        while True:
            buf = in_file.readline()
            if buf == '':
                break
            lno += 1

            s = buf.strip()
            if cmode and (s.startswith('x3270.') or s.startswith('x3270*')):
                s = s[6:]

            if (not last_continue and s.startswith('!')) or len(s) == 0:
                continue

            if s.startswith('#'):
                is_ifndef = s.startswith('#ifndef')
                if s.startswith('#ifdef') or is_ifndef:
                    if ssp >= SSSZ:
                        print(f'{filename}:{lno}: Stack overflow', file=sys.stderr)
                        exit(1)
                    ss[ssp].ifdefs = Mode.NONE
                    ss[ssp].ifndefs = Mode.NONE
                    ss[ssp].lno = lno

                    tk = 6 + (1 if is_ifndef else 0)
                    condition = s[tk:].lstrip()
                    if condition in parts:
                        if is_ifndef:
                            ss[ssp].ifndefs = parts[condition]
                        else:
                            ss[ssp].ifdefs = parts[condition]
                        ssp += 1
                    else:
                        print(f'{filename}:{lno}: Unknown condition {condition}', file=sys.stderr)
                        exit(1)
                    continue
                elif s == '#else':
                    if ssp == 0:
                        print(f'{filename}:{lno}: Missing #if[n]def', file=sys.stderr)
                        exit(1)
                    tmp = ss[ssp-1].ifdefs
                    ss[ssp-1].ifdefs = ss[ssp-1].ifndefs
                    ss[ssp-1].ifndefs = tmp
                elif s == '#endif':
                    if ssp == 0:
                        print(f'{filename}:{lno}: Missing #if[n]def', file=sys.stderr)
                        exit(1)
                    ssp -= 1
                else:
                    print(f'{filename}:{lno}: Unrecognized # directive', file=sys.stderr)
                    exit(1)
                continue

            # Figure out if there's anything to emit.
            # First, look for contradictions.
            ifdefs = Mode.NONE
            ifndefs = Mode.NONE
            for i in range(ssp):
                ifdefs |= ss[i].ifdefs
                ifndefs |= ss[i].ifndefs
            if ifdefs != Mode.NONE and ifndefs != Mode.NONE:
                continue

            # Then, apply the actual values.
            if ifdefs != Mode.NONE and (ifdefs & is_defined) != ifdefs:
                continue
            if ifndefs != Mode.NONE and (ifndefs & is_undefined) != ifndefs:
                continue

            # Emit the text.
            filtered.append(FilteredRecord(ifdefs, ifndefs, lno, filename, s))
            last_continue = s.endswith('\\')

        if infiles != None:
            in_file.close()
        else:
            break
    
    if ssp != 0:
        print(f'{ssp} missing #endifs', file=sys.stderr)
        print(f'last #ifdef was at line {ss[ssp-1].lno}', file=sys.stderr)
        exit(1)

    # Re-scan, emitting code this time.
    t = OutBuffer()
    if not cmode:
        tc = OutBuffer()
        tm = OutBuffer()
    
    # Emit the initial boilerplate.
    t.append('/* This file was created automatically by mkfb. */')
    t.append('')
    t.append('#include "globals.h"',)
    t.append('#include "fallbacks.h"')
    t.append('')
    if cmode:
        t.append('static unsigned char fsd[] = {')
        t.append('')
    else:
        t.append('unsigned char common_fallbacks[] = {')
        t.append('')
        tc.append('unsigned char color_fallbacks[] = {')
        tc.append('')
        tm.append('unsigned char mono_fallbacks[] = {')
        tm.append('')

    # Scan the file, emitting the fsd array and creating the indices.
    cc = 0
    aix = []
    xlno = []
    xfilename = []
    backslash = False
    for rec in filtered:
        ifdefs = rec.ifdefs
        ifndefs = rec.ifndefs
        lno = rec.lno
        buf = rec.value

        if cmode:
            # Ignore color. Accumulate offsets into an array.
            t_this = t
            if not backslash:
                aix.append(cc)
                xlno.append(lno)
                xfilename.append(rec.filename)
        else:
            # Use color to decide which list to write into.
            if not Mode.COLOR in ifdefs and not Mode.COLOR in ifndefs:
                # Both.
                t_this = t
            elif Mode.COLOR in ifdefs:
                # Just color.
                t_this = tc
            else:
                # Just mono.
                t_this = tm

        backslash = False
        white = 0
        for c in buf:
            if backslash:
                if cmode:
                    if c == 't':
                        c = '\t'
                    elif c == 'n':
                        c = '\n'
                else:
                    t_this.emit('\\')
                    cc += 1
                backslash = False
            if c == ' ' or c == '\t':
                white += 1
            elif white > 0:
                t_this.emit(' ')
                cc += 1
                white = 0
            if c != ' ' and c != '\t':
                if c == '#':
                    if not cmode:
                        t_this.emit('\\')
                        t_this.emit('#')
                        cc += 2
                    else:
                        t_this.emit(c)
                        cc += 1
                elif c == '\\':
                    backslash = True
                else:
                    t_this.emit(c)
                    cc += 1
        if white > 0:
            # Line ended with whitespace pending.
            t_this.emit(' ')
            cc += 1
            white = 0
        if not backslash:
            # Line is not a continuation, terminate it.
            if cmode:
                t_this.emit('\0')
            else:
                t_this.emit('\n')
            cc += 1

    if cmode:
        t.append('};',)
        t.append('')
    else:
        t.emit('\0')
        t.append('};',)
        t.append('')
        tc.emit('\0')
        tc.append('};')
        tc.append('')
        tm.emit('\0')
        tm.append('};')
        tm.append('')

    # Copy tmp to output.

    # Open the output file.
    if ofile != None:
        out_file = open(ofile, 'w')
    else:
        out_file = sys.stdout
    for line in t.text:
        out_file.write(line + '\n')
    if not cmode:
        for line in tc.text:
            out_file.write(line + '\n')
        for line in tm.text:
            out_file.write(line + '\n')
    
    if cmode:
        # Emit the fallback array.
        print(f'char *fallbacks[{len(aix)+1}] = ' + '{', file=out_file)
        for i in range(len(aix)):
            print(f'    (char *)&fsd[{aix[i]}], /* {xfilename[i]}:{xlno[i]} */', file=out_file)
        print('    NULL\n};', file=out_file)
        print(file=out_file)

    # Emit some test code.
    print('#if defined(DEBUG) /*[*/', file=out_file)
    print('#include <stdio.h>', file=out_file)
    print('int', file=out_file)
    print('main(int argc, char *argv[])', file=out_file)
    print('{', file=out_file)
    print('    int i;', file=out_file)
    print(file=out_file)
    if cmode:
        print('    for (i = 0; fallbacks[i] != NULL; i++) {', file=out_file)
        print('\tprintf("%d: %s\\n", i, fallbacks[i]);', file=out_file)
        print('    }', file=out_file)
    else:
        print('    printf(\"Common:\\n%s\\n\", common_fallbacks);', file=out_file)
        print('    printf(\"Color:\\n%s\\n\", color_fallbacks);', file=out_file)
        print('    printf(\"Mono:\\n%s\\n\", mono_fallbacks);', file=out_file)
    print('    return 0;', file=out_file)
    print('}', file=out_file)
    print('#endif /*]*/', file=out_file)

def usage():
    '''Print a usage message and exit'''
    print('usage: mkfb [-c] -[w] [-o outfile] [infile...]', file=sys.stderr)
    exit(1)

# Parse the command line.
args = sys.argv[1:].copy()
infiles = None
ofile = None
while len(args) > 0:
    arg = args[0]
    if not arg.startswith('-'):
        infiles = args
        break
    if arg == '-c':
        cmode = True
    elif arg == '-w':
        is_defined |= Mode._WIN32
    elif arg == '-o':
        if len(args) < 2:
            usage()
        args = args[1:]
        ofile = args[0]
    else:
        usage()
    args = args[1:]

# Run.
mkfb(infiles, ofile)