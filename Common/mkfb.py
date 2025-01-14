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

import os
import sys
import tempfile

# #ifdef bit masks.
MODE_COLOR = 0x1
MODE_APL = 0x2
MODE_DBCS = 0x4
MODE__WIN32 = 0x8
MODEMASK = 0xfff

# Globals.
cmode = False
is_defined = MODE_COLOR | MODE_APL | MODE_DBCS
is_undefined = 0

class Ss:
    '''#ifdef stack element'''
    def __init__(self):
        self.ifdefs = 0
        self.ifndefs = 0
        self.lno = 0

# Set up ss (the #ifdef nesting stack) as an array for now.
# Clearly this should be something more Pythonic.
SSSZ = 10
ss = [Ss() for i in range(SSSZ)]
ssp = 0

class FilteredRecord:
    '''#ifdef temp'''
    def __init__(self, ifdefs: int, ifndefs: int, lno: int, value: str):
        self.ifdefs = ifdefs
        self.ifndefs = ifndefs
        self.lno = lno
        self.value = value

# Dictionary mapping #ifdef symbols to bit masks.
parts = {
    'COLOR': MODE_COLOR,
    'X3270_APL': MODE_APL,
    'X3270_DBCS': MODE_DBCS,
    '_WIN32': MODE__WIN32
}

n_out = [ 0, 0, 0]
def emit(t, ix: int, c: str):
    '''Emit a byte, with nice formatting'''
    if n_out[ix] >= 19:
        t.append('')
        n_out[ix] = 0
    t[len(t)-1] += f'{ord(c):3d},'
    n_out[ix] += 1

def mkfb(infiles, ofile: str):
    '''Generate the fallback files'''

    last_continue = False
    lno = 0
    ssp = 0

    # Open the output file.
    if ofile != None:
        out_file = open(ofile, 'w')
    else:
        out_file = sys.stdout

    is_undefined = MODE_COLOR | (~is_defined & MODEMASK)

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
                is_ifndef = 1 if s.startswith('#ifndef') else 0
                if s.startswith('#ifdef') or s.startswith('#ifndef'):
                    if ssp >= SSSZ:
                        print(f'{filename}, line {lno}: Stack overflow', file=sys.stderr)
                        exit(1)
                    ss[ssp].ifdefs = 0
                    ss[ssp].ifndefs = 0
                    ss[ssp].lno = lno

                    tk = 7 + is_ifndef
                    if s[tk:] in parts:
                        if is_ifndef != 0:
                            ss[ssp].ifndefs = parts[s[tk:]]
                        else:
                            ss[ssp].ifdefs = parts[s[tk:]]
                        ssp += 1
                    else:
                        print(f'{filename}, line {lno}: Unknown condition {s[tk:]}', file=sys.stderr)
                        exit(1)
                    continue
                elif s == '#else':
                    if ssp == 0:
                        print(f'{filename}, line {lno}: Missing #if[n]def', file=sys.stderr)
                        exit(1)
                    tmp = ss[ssp-1].ifdefs
                    ss[ssp-1].ifdefs = ss[ssp-1].ifndefs
                    ss[ssp-1].ifndefs = tmp
                elif s == '#endif':
                    if ssp == 0:
                        print(f'{filename}, line {lno}: Missing #if[n]def', file=sys.stderr)
                        exit(1)
                    ssp -= 1
                else:
                    print(f'{filename}, line {lno}: Unrecognized # directive', file=sys.stderr)
                    exit(1)
                continue

            # Figure out if there's anything to emit.
            # First, look for contradictions.
            ifdefs = 0
            ifndefs = 0
            for i in range(ssp):
                ifdefs |= ss[i].ifdefs
                ifndefs |= ss[i].ifndefs
            if ifdefs != 0 and ifndefs != 0:
                continue

            # Then, apply the actual values.
            if ifdefs != 0 and (ifdefs & is_defined) != ifdefs:
                continue
            if ifndefs != 0 and (ifndefs & is_undefined) != ifndefs:
                continue

            # Emit the text.
            filtered.append(FilteredRecord(ifdefs, ifndefs, lno, s))
            last_continue = s.endswith('\\')

        if infiles == None:
            break
    
    if ssp != 0:
        print(f'{ssp} missing #endifs', file=sys.stderr)
        print(f'last #ifdef was at line {ss[ssp-1].lno}', file=sys.stderr)
        exit(1)

    # Re-scan, emitting code this time.
    t = []
    if not cmode:
        tc = []
        tm = []
    
    # Emit the initial boilerplate.
    t.append('/* This file was created automatically by mkfb. */')
    t.append('')
    t.append('#include "globals.h"',)
    t.append('#include "fallbacks.h"')
    if cmode:
        t.append('static unsigned char fsd[] = {')
    else:
        t.append('unsigned char common_fallbacks[] = {')
        tc.append('unsigned char color_fallbacks[] = {')
        tm.append('unsigned char mono_fallbacks[] = {')

    # Scan the file, emitting the fsd array and creating the indices.
    cc = 0
    aix = []
    xlno = []
    backslash = False
    for rec in filtered:
        ifdefs = rec.ifdefs
        ifndefs = rec.ifndefs
        lno = rec.lno
        buf = rec.value

        t_this = t
        ix = 0
        if cmode:
            # Ignore color. Accumulate offsets into an array.
            if not backslash:
                aix.append(cc)
                xlno.append(lno)
        else:
            # Use color to decide which file to write into.
            if (ifdefs & MODE_COLOR) == 0 and (ifndefs & MODE_COLOR) == 0:
                # Both.
                t_this = t
                ix = 0
            elif (ifdefs & MODE_COLOR) != 0:
                # Just color.
                t_this = tc
                ix = 1
            else:
                # Just mono.
                t_this = tm
                ix = 2

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
                    emit(t_this, ix, '\\')
                    cc += 1
                backslash = False
            if c == ' ' or c == '\t':
                white += 1
            elif white > 0:
                emit(t_this, ix, ' ')
                cc += 1
                white = 0
            if c != ' ' and c != '\t':
                if c == '#':
                    if not cmode:
                        emit(t_this, ix, '\\')
                        emit(t_this, ix, '#') 
                        cc += 2
                    else:
                        emit(t_this, ix, c)
                        cc += 1
                elif c == '\\':
                    backslash = True
                else:
                    emit(t_this, ix, c)
                    cc += 1
        if white > 0:
            # Line ended with whitespace pending.
            emit(t_this, ix, ' ')
            cc += 1
            white = 0
        if not backslash:
            # Line is not a continuation, terminate it.
            if cmode:
                emit(t_this, ix, '\0')
            else:
                emit(t_this, ix, '\n')
            cc += 1

    if cmode:
        t.append('};',)
        t.append('')
    else:
        emit(t, 0, '\0')
        t.append('};',)
        t.append('')
        emit(tc, 0, '\0')
        tc.append('};')
        tc.append('')
        emit(tm, 0, '\0')
        tm.append('};')
        tm.append('')

    # Copy tmp to output.
    for line in t:
        out_file.write(line + '\n')
    if not cmode:
        for line in tc:
            out_file.write(line + '\n')
        for line in tm:
            out_file.write(line + '\n')
    
    if cmode:
        # Emit the fallback array.
        print(f'char *fallbacks[{len(aix)+1}] = ' + '{', file=out_file)
        for i in range(len(aix)):
            print(f'\t(char *)&fsd[{aix[i]}], /* line {xlno[i]} */', file=out_file)
        print('\tNULL\n};', file=out_file)
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

    if infiles != None:
        in_file.close()

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
        is_defined |= MODE__WIN32
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