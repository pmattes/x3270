#! /bin/sh
# Copyright 1995, 1999, 2000 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.
#
# pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
#
# Create version.o from version.txt
#set -x

set -e

. ./version.txt
builddate=`date`
sccsdate=`date +%Y/%m/%d`
user=${LOGNAME-$USER}

trap 'rm -f version.c' 0 1 2 15

cat <<EOF >version.c
const char *build = "${2-x3270} v$version $builddate $user";
const char *app_defaults_version = "$adversion";
static const char sccsid[] = "@(#)${2-x3270} v$version $sccsdate $user";
EOF

${1-cc} -c version.c
