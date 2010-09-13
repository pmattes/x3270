#! /bin/sh

# Copyright (c) 1995-2009, Paul Mattes.
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
#     * Neither the name of Paul Mattes nor his contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# VM login script, which runs as a peer of x3270.
# sh version

#set -x
me=`echo $0 | sed 's/.*\///'`

# Set up login parameters
tcp_host=${1-ibmsys}
userid=${2-USERID}
password=${3-PASSWORD}

# Verbose flag for x3270if
#v="-v"

# Define some handly local functions.

# x3270 interface function
xi()
{
	x3270if $v "$@"
}

# 'xi' function, with space-to-comma translation
xic()
{
        _sep=""
	_cmd="$1("

	shift
	while [ $# -gt 0 ]
	do	_cmd="$_cmd$_sep\"$1\""
		_sep=","
		shift
	done
	_cmd="$_cmd)"
	xi "$_cmd"
}

# Common x3270 Ascii function
ascii()
{
	xic Ascii $@
}

# Common x3270 String function
string()
{
	xic String "$@"
}

# x3270 connection status
cstatus()
{
	xi -s 4
}

# x3270 rows
rows()
{
	xi -s 7
}

# x3270 columns
cols()
{
	xi -s 8
}

# Failure.
die()
{
	xic Info "$me error: $@"
	xic CloseScript 1
	exit 1
}

# x3270 Snap function
snap()
{
        xic Snap $@
}

# Wait for a READ prompt.
waitread()
{
	snap
	while :
	do	r=`snap Rows`
		r=`expr $r - 1`
		c=`snap Cols`
		c=`expr $c - 17`
		s=`snap Ascii $r $c 4`
		[ "$s" = "READ" ] && break

		xic Wait Output
		snap
	done
}

# Set up pipes for x3270 I/O
ip=/tmp/ip.$$
op=/tmp/op.$$
rm -f $ip $op
mkfifo $ip $op

# Start x3270
x3270 -script -model 2 <$ip >$op &
exec 5>$ip 6<$op
rm -f $ip $op
X3270INPUT=5
X3270OUTPUT=6
export X3270INPUT X3270OUTPUT
xi -s 0 >/dev/null || exit 1

# Connect to host
xic Connect $tcp_host || die "Connection failed."

# Make sure we're connected.
xic Wait InputField
[ "`cstatus`" = N ] && die "Not connected."

# Log in.
string "$userid"
xic Tab
string "$password"
xic Enter
waitread
if [ "`ascii 1 11 7`" = "Already" ]
then	die "Can't run -- already logged in."
	exit 1
fi

# Boot CMS, if we have to.
r=`rows`
r=`expr $r - 1`
c=`cols`
c=`expr $c - 20`
s=`ascii $r $c 2`
if [ "$s" = "CP" ]
then	xic Clear
	string "i cms"
	xic Enter
	waitread
fi

# Done.
xic CloseScript
exit 0
