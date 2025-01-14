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
#  mkversion [-o outfile] [program [path-to-version.txt]]
#
# Formerly a shell script:
#
# Ensure that 'date' emits 7-bit U.S. ASCII.
# LANG=C
# LC_ALL=C
# export LANG LC_ALL
# 
# set -e
# 
# if date --help >/dev/null 2>&1
# then    # GNU date
#         ddopt="-d@"
# else    # BSD date
#         ddopt="-r"
# fi
# 
# . ${2-./version.txt}
# date="date -u"
# [ -n "$SOURCE_DATE_EPOCH" ] && date="$date $ddopt$SOURCE_DATE_EPOCH"
# builddate=`$date`
# sccsdate=`$date +%Y/%m/%d`
# user=${LOGNAME-$USER}
# 
# # Create an all numeric timestamp for rpqnames.
# # rpq.c will return this string of numbers in bcd format
# # It is OK to change the length (+ or -), but use
# # decimal (0-9) digits only. Length must be even number of digits.
# rpq_timestamp=`$date +%Y%m%d%H%M%S`
# 
# app=${1-x3270}
# cat <<EOF
# const char *app = "$app";
# const char *build = "$app v$version $builddate $user";
# const char *app_defaults_version = "$adversion";
# const char *cyear = "$cyear";
# const char sccsid[] = "@(#)$app v$version $sccsdate $user";
# 
# const char *build_rpq_timestamp = "$rpq_timestamp";
# const char *build_rpq_version = "$version";
# EOF

from datetime import datetime, timezone
import locale
import os
import re
import sys

def mkversion(outfile_name: str, program: str, version_txt: str):
    # Set the locale.
    locale.setlocale(locale.LC_ALL, 'C')

    # Read in version.txt.
    ver = {}
    lno = 0
    infile = open(version_txt)
    while True:
        line = infile.readline()
        if line == '':
            break
        lno += 1
        line = line.strip()
        m = re.fullmatch('^([a-z]+)="(.*)"$', line)
        if m == None:
            print(f'Syntax error in version.txt at line {lno}', file = sys.stderr)
            continue
        ver[m.group(1)] = m.group(2)
    infile.close()

    if not 'version' in ver or not 'adversion' in ver or not 'cyear' in ver:
        print('Missing fields(s) in version.txt', file = sys.stderr)
        exit(1)
    version = ver['version']
    adversion = ver['adversion']
    cyear = ver['cyear']

    # Open the output file.
    if outfile_name != None:
        outfile = open(outfile_name, 'w')
    else:
        outfile = sys.stderr

    # Get the date.
    if 'SOURCE_DATE_EPOCH' in os.environ:
        now = datetime.strptime(os.environ['SOURCE_DATE_EPOCH'], '%a %b %d %H:%M:%S %Z %Y').replace(tzinfo=timezone.utc)
    else:
        now = datetime.now(timezone.utc)
    builddate = datetime.strftime(now, '%a %b %d %H:%M:%S %Z %Y')
    sccsdate = datetime.strftime(now, '%Y/%m/%d')
    rpq_timestamp = datetime.strftime(now, '%Y%m%d%H%M%S')

    # Get the user.
    if 'USER' in os.environ:
        user = os.environ['USER']
    elif 'USERNAME' in os.environ:
        user = os.environ['USERNAME']
    elif 'LOGNAME' in os.environ:
        user = os.environ['LOGNAME']
    else:
        user = 'unknown user'

    # Print it all out.
    print(f'const char *app = "{program}";', file=outfile)
    print(f'const char *build = "{program} v{version} {builddate} {user}";', file=outfile)
    print(f'const char *app_defaults_version = "{adversion}";', file=outfile)
    print(f'const char *cyear = "{cyear}";', file=outfile)
    print(f'const char sccsid[] = "@(#){program} v{version} {sccsdate} {user}";', file=outfile)
    print(file=outfile)
    print(f'const char *build_rpq_timestamp = "{rpq_timestamp}";', file=outfile)
    print(f'const char *build_rpq_version = "{version}";', file=outfile)

    if outfile_name != None:
        outfile.close()

def usage():
    '''Print a usage message and exit'''
    print('Usage: mkversion [-o out_file] [program [path-to-version.txt]]', file=sys.stderr)
    exit(1)

# Parse the command line.
outfile_name = None
program = 'x3270'
version_txt = './version.txt'
args = sys.argv.copy()[1:]
while len(args) > 0 and args[0].startswith('-'):
    if args[0] == '-o':
        if len(args) < 2:
            usage()
        outfile_name = args[1]
        args = args[1:]
    else:
        usage()
    args = args[1:]

if len(args) > 2:
    usage()
if len(args) > 0:
    program = args[0]
if len(args) > 1:
    version_txt = args[1]

# Run.
mkversion(outfile_name, program, version_txt)