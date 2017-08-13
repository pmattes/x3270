#!/usr/bin/env sh

# Copyright (c) 2017 Paul Mattes.
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

# Create a manifest file from the template
#  mkmanifest version.txt exe-name "description" [WIN64]
#set -x

set -e

if [ $# -lt 4 -o $# -gt 5 ]
then	echo >&2 "Usage: mkmanifest.sh version.txt manifest.tmpl exe-name 'description' [1]"
    	exit 2
fi

. $1

# Name and description are easy.
name="$3"
description="$4"

# Version is trickier.
# <major>.<minor>text<update> becomes <major>.<minor>.<update>.0
version_subst=`echo $version | sed 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\)[a-z][a-z]*\([0-9][0-9]*\)$/\1.\2.\3.0/'`

# Architecture is straightforward, but odd.
# If no last argument, assume x86.
# Otherwise, "1" means ia64.
case "$5" in
    "")	arch=x86;;
    1)	arch=ia64;;
    *)	echo >&2 "Invalid arch parameter (must be empty for x86, 1 for ia64)"
    exit 1
esac

sed -e "s/%NAME%/$name/g" \
    -e "s/%VERSION%/$version_subst/g" \
    -e "s/%ARCHITECTURE%/$arch/g" \
    -e "s/%DESCRIPTION%/$description/g" \
    $2
