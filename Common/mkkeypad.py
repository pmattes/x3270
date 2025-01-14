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
# Construct keypad data structures from a set of descriptor files.
#
# The files are:
#  keypad.labels
#   literal text to be drawn for the keypad
#  keypad.outline
#   outlines for the keys, ACS encoded ('l' for upper left, etc.)
#  keypad.map
#   sensitivity map for the keypad (aaaa is field 'a', etc.)
#  keypad.full
#   not used by this program, but gives the overall plan
#
# The result is an array of structures:
#  unsigned char literal;      text from keypad.labels
#  unsigned char outline;      ACS-encoded outline text
#  sens_t *sens;               sensitivity, or NULL
#
# A sens_t is a structure:
#  unsigned char ul_x, ul_y;   upper left corner
#  unsigned char lr_x, lr_y;   lower right corner
#  unsigned char callback_name; 'a', 'b', etc.

from io import TextIOWrapper
import os
import os.path
import sys

class Sensmap:
    '''Sensitivity map'''

    # Global index.
    index = 0

    def __init__(self, x: int, y: int):
        self.ul_x = x
        self.ul_y = y
        self.lr_x = x
        self.lr_y = y
        self.callback = None

        self.index = Sensmap.index
        Sensmap.index += 1

def fopen_inc(name: str) -> TextIOWrapper:
    '''Open a file, using the -I directory if needed'''
    try:
        f = open(name)
    except Exception as ex:
        if incdir != None:
            f = open(os.path.join(incdir, name))
        else:
            raise ex
    return f

def mkkeypad(incdir: str, out_file_name: str):
    '''Genereate the keypad map'''
    map = fopen_inc('keypad.map')
    callbacks = fopen_inc('keypad.callbacks')
    sensmaps = {}

    if out_file_name != None:
        out_file = open(out_file_name, 'w')
    else:
        out_file = sys.stdout

    # Read in the map file.
    x = 0
    y = 0
    while True:
        c = map.read(1)
        if c == '':
            break
        if c == '\n':
            y += 1
            x = 0
            continue
        if c == ' ':
            x += 1
            continue
        if c in sensmaps:
            sensmaps[c].lr_x = x
            sensmaps[c].lr_y = y
        else:
            sensmaps[c] = Sensmap(x, y)
        x += 1
    map.close()

    # Read in the callbacks.
    while True:
        buf = callbacks.readline()
        if buf == '':
            break
        buf = buf.strip()
        c = buf[0]
        buf = buf[1:].strip()
        if c in sensmaps:
            sensmaps[c].callback = buf
        else:
            print( 'Unknown map: {c}', file=sys.stderr)
            exit(1)
    callbacks.close()

    # Check the sensmaps.
    for key, s in sensmaps.items():
        if s.callback == None:
            print(f'Map "{key}" has no callback', file=sys.stderr)
            exit(1)

    # Dump out the sensmaps.
    keysort = {} # dictionary of indices to semsmaps keys
    for key, value in sensmaps.items():
        keysort[value.index] = key
    print(f'sens_t sens[{len(sensmaps)}] =', '{', file=out_file)
    for ks in sorted(keysort.keys()):
        key = keysort[ks]
        s = sensmaps[key]
        print('  { ' + f'{s.ul_x:2d}, {s.ul_y:2d}, {s.lr_x:2d}, {s.lr_y:2d}, "{s.callback}"' + ' },', file=out_file)
    print('};', file=out_file)

    # Read in the label and outline files, and use them to dump out keypad_desc[].
    labels = fopen_inc('keypad.labels')
    outline = fopen_inc('keypad.outline')

    print(f'keypad_desc_t keypad_desc[{y}][80] = ' + '{', file=out_file)
    print('{ /* row 0 */', file=out_file)
    x = 0
    y = 0
    while True:
        c = labels.read(1)
        if c == '':
            break
        d = outline.read(1)
        if c == '\n':
            if d != '\n':
                print(f'labels and outline are out of sync at line {y + 1}', file=sys.stderr)
                exit(1)
            y += 1
            x = 0
            continue
        if x == 0 and y != 0:
            print('},', file=out_file)
            print('{ ' + f'/* row {y} */', file=out_file)
        found = False
        for _, s in sensmaps.items():
            if x >= s.ul_x and y >= s.ul_y and x <= s.lr_x and y <= s.lr_y:
                print('  { ' + f"'{c}', '{d}', &sens[{s.index}]" + ' },', file=out_file)
                found = True
                break
        if not found:
            if c == ' ' and d == ' ':
                print('  {   0,   0, NULL },', file=out_file)
            else:
                print('  { ' + f"'{c}', '{d}', NULL" + ' },', file=out_file)
        x += 1
    d = outline.read(1)
    if d != '':
        print('labels and outlines are out of sync at EOF', file=os.stderr)
        exit(1)
    print('} };', file=out_file)
    labels.close()
    outline.close()

    if out_file_name != None:
        out_file.close()

# Parse the command line.
incdir = None
outfile_name = None
args = sys.argv.copy()[1:]
while len(args) > 0:
    if args[0].startswith('-I'):
        incdir = args[0][2:]
    elif args[0] == '-o':
        if len(args) < 2:
            print('Missing value after -o', file=sys.stderr)
            exit(1)
        outfile_name = args[1]
        args = args[1:]
    else:
        print(f"Unknown option '{args[0]}'", file=sys.stderr)
        exit(1)
    args = args[1:]

# Run.
mkkeypad(incdir, outfile_name)