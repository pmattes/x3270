#! /bin/sh

# Copyright (c) 1995-2009, Paul Mattes.
# Copyright (c) 2005, Don Russell.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes, Don Russell nor their contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DON RUSSELL "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DON RUSSELL
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# Create version.o from version.txt
#set -x

# Ensure that 'date' emits 7-bit U.S. ASCII.
LANG=C
LC_ALL=C
export LANG LC_ALL

set -e

. ./version.txt
builddate=`date`
sccsdate=`date +%Y/%m/%d`
user=${LOGNAME-$USER}

# Create an all numeric timestamp for rpqnames.
# rpq.c will return this string of numbers in bcd format
# It is OK to change the length (+ or -), but use
# decimal (0-9) digits only. Length must be even number of digits.
rpq_timestamp=`date +%Y%m%d%H%M%S`

trap 'rm -f version.c' 0 1 2 15

cat <<EOF >version.c
const char *build = "${2-x3270} v$version $builddate $user";
const char *app_defaults_version = "$adversion";
static const char sccsid[] = "@(#)${2-x3270} v$version $sccsdate $user";

const char *build_rpq_timestamp = "$rpq_timestamp";
const char *build_rpq_version = "$version";
EOF

${1-cc} -c version.c
