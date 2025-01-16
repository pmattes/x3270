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
# Create version.c from version.txt
#  mkmanifest.py version.txt manifest.tmpl exe-name 'description' [architecture]
#
# Formerly a shell script:
#
# set -e
#
# if [ $# -lt 4 -o $# -gt 5 ]
# then    echo >&2 "Usage: mkmanifest.sh version.txt manifest.tmpl exe-name 'description' [architecture]"
#         exit 2
# fi
#
# . $1
#
# # Name and description are easy.
# name="$3"
# description="$4"
#
# # Version is trickier.
# # <major>.<minor>text<update> becomes <major>.<minor>.<update>.0
# version_subst=`echo $version | sed 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\)[a-z][a-z]*\([0-9][0-9]*\)$/\1.\2.\3.0/'`
#
# if [ -n "$5" ]
# then    arch=$5
# else    arch=x86
# fi
#
# sed -e "s/%NAME%/$name/g" \
#     -e "s/%VERSION%/$version_subst/g" \
#     -e "s/%ARCHITECTURE%/$arch/g" \
#     -e "s/%DESCRIPTION%/$description/g" \
#     $2

import re
import sys

def mkmanifest(outfile_name: str, version_txt: str, template: str, exe_name: str, description: str):

    # Read in version.txt.
    ver = {}
    lno = 0
    with open(version_txt) as infile:
        while True:
            line = infile.readline()
            if line == '':
                break
            lno += 1
            line = line.strip()
            m = re.fullmatch('^([a-z]+)="(.*)"$', line)
            if m == None:
                print(f'Syntax error in {version_txt}:{lno}', file = sys.stderr)
                continue
            ver[m.group(1)] = m.group(2)

    if not 'version' in ver:
        print('Missing fields(s) in version.txt', file = sys.stderr)
        exit(1)
    version = ver['version']

    # Transform the version.
    m = re.fullmatch(r'^([0-9][0-9]*)\.([0-9][0-9]*)[a-z][a-z]*([0-9][0-9]*)$', version)
    if m == None:
        print(f'Version "{version}" does not have the right format', file = sys.stderr)
        exit(1)
    xversion = '.'.join([m.group(i) for i in range(1, 4)]) + '.0'

    # Read in the template.
    with open(template) as tf:
        tc = tf.readlines()

    # Open the outfile.
    if outfile_name != None:
        outfile = open(outfile_name, 'w', newline='\r\n')
    else:
        outfile = sys.stdout

    # Substitute.
    for line in tc:
        print(line.rstrip().replace('%NAME%', exe_name).replace('%VERSION%', xversion).replace('%DESCRIPTION%', description), file=outfile)

    if outfile_name != None:
        outfile.close()

def usage():
    '''Print a usage message and exit'''
    print('Usage: mkmanifest [-o outfile] version.txt manifest.tmpl exe-name description', file=sys.stderr)
    exit(1)

# Parse the command line.
args = sys.argv.copy()[1:]
outfile_name = None
while len(args) > 0 and args[0].startswith('-'):
    if args[0] == '-o':
        if len(args) < 2:
            usage()
        outfile_name = args[1]
        args = args[1:]
    else:
        usage()
    args = args[1:]
if len(args) != 4:
    usage()

# Run.
mkmanifest(outfile_name, args[0], args[1], args[2], args[3])
