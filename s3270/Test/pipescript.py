#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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
# s3270 pipe-based child script test program

import os
import sys
import time

if __name__ != '__main__':
    print('Cannot import', file=sys.stderr)
    exit(1)

args = sys.argv[1:]
gulp = False
if len(args) > 1 and args[0] == '-gulp':
    gulp = True
    args = args[1:]

if len(args) != 2:
    print(f'Usage: {sys.argv[0]} [-gulp] infile outfile', file=sys.stderr)
    exit(1)
infile_name = args[0]
outfile_name = args[1]

# Find the pipe file descriptors.
if not 'X3270INPUT' in os.environ or not 'X3270OUTPUT' in os.environ:
    print('Missing X3270INPUT/X3270OUTPUT', file=sys.stderr)
    exit(1)

# Copy from infile to s3270.
with open(int(os.environ['X3270INPUT']), 'a') as s3270_inf:
    with open(infile_name, 'r') as inf:
        if gulp:
            s3270_inf.writelines(inf.readlines())
        else:
            while True:
                s = inf.readline()
                if s == '':
                    break
                s3270_inf.write(s)
                s3270_inf.flush()
                # Let s3270 absorb the input before we write the next line.
                time.sleep(0.1)

# Copy from s3270 to outfile.
with open(int(os.environ['X3270OUTPUT']), 'r') as outfile:
    with open(outfile_name, 'w') as outf:
        outf.writelines(outfile.readlines())