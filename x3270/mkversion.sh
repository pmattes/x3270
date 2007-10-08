#! /bin/sh
# Copyright 1995, 1999, 2005 by Paul Mattes.
# RPQNAMES modifications copyright 2005 by Don Russell.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.
#
# x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
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
