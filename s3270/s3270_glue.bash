#! /bin/bash
#
# Copyright 1993, 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2004, 2006,
#  2007 by Paul Mattes.
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in
# supporting documentation.
#
# s3270 glue for bash
#
# Intended use in a bash script:
#
#  . s3270_glue.bash
#  Start
#  Connect host
#  Wait InputField
#  # interact with the host as needed
#  Disconnect
#  Stop

#set -x

# Define some handy functions.

# s3270 interface function
function xi
{
	typeset x y

	if [ "X$1" = "X-s" ]
	then	echo >&5
		read x <&6
		read <&6
		echo "$x" | awk "{print \$$2}"
		return
	fi

	echo >&5 "$@"
	while read x <&6
	do	y=$(expr substr "$x" 1 5)
		if [ "$y" = "data:" ]
		then	z="${x#?????}"
			if [ -n "$z" ]
			then	echo "${z#?}"
			else	echo "$z"
			fi
		elif [ "$x" = ok ]
		then	return 0
		elif [ "$x" = error ]
		then	return 1
		fi
	done
	return 1
}

# 'xi' function, with space-to-comma and double-quote translation
function xic
{
	typeset sep cmd="$1("
	typeset a

	shift
	while [ $# -gt 0 ]
	do	echo "$1" | sed 's/"/\\"/' >/tmp/x$$
		a="$(</tmp/x$$)"
		rm -f /tmp/x$$
		cmd="$cmd$sep\"$a\""
		sep=","
		shift
	done
	cmd="$cmd)"
	echo "cmd=$cmd"
	xi "$cmd"
}


# All of the s3270 actions.
function AnsiText
{
	xic AnsiText "$@"
}
function Ascii
{
	xic Ascii "$@"
}
function AsciiField
{
	xic AsciiField "$@"
}
function Attn
{
	xic Attn "$@"
}
function BackSpace
{
	xic BackSpace "$@"
}
function BackTab
{
	xic BackTab "$@"
}
function CircumNot
{
	xic CircumNot "$@"
}
function Clear
{
	xic Clear "$@"
}
function Connect
{
	xic Connect "$@"
}
function CursorSelect
{
	xic CursorSelect "$@"
}
function Delete
{
	xic Delete "$@"
}
function DeleteField
{
	xic DeleteField "$@"
}
function DeleteWord
{
	xic DeleteWord "$@"
}
function Disconnect
{
	xic Disconnect "$@"
}
function Down
{
	xic Down "$@"
}
function Dup
{
	xic Dup "$@"
}
function Ebcdic
{
	xic Ebcdic "$@"
}
function EbcdicField
{
	xic EbcdicField "$@"
}
function Enter
{
	xic Enter "$@"
}
function Erase
{
	xic Erase "$@"
}
function EraseEOF
{
	xic EraseEOF "$@"
}
function EraseInput
{
	xic EraseInput "$@"
}
function FieldEnd
{
	xic FieldEnd "$@"
}
function FieldMark
{
	xic FieldMark "$@"
}
function HexString
{
	xic HexString "$@"
}
function Home
{
	xic Home "$@"
}
function Insert
{
	xic Insert "$@"
}
function Interrupt
{
	xic Interrupt "$@"
}
function Key
{
	xic Key "$@"
}
function Left
{
	xic Left "$@"
}
function Left2
{
	xic Left2 "$@"
}
function Macro
{
	xic Macro "$@"
}
function MonoCase
{
	xic MonoCase "$@"
}
function MoveCursor
{
	xic MoveCursor "$@"
}
function Newline
{
	xic Newline "$@"
}
function NextWord
{
	xic NextWord "$@"
}
function PA
{
	xic PA "$@"
}
function PF
{
	xic PF "$@"
}
function PreviousWord
{
	xic PreviousWord "$@"
}
function Query
{
	xic Query "$@"
}
function Quit
{
	xic Quit "$@"
}
function ReadBuffer
{
	xic ReadBuffer "$@"
}
function Reset
{
	xic Reset "$@"
}
function Right
{
	xic Right "$@"
}
function Right2
{
	xic Right2 "$@"
}
function Script
{
	xic Script "$@"
}
function Snap
{
	xic Snap "$@"
}
function String
{
	xic String "$@"
}
function SysReq
{
	xic SysReq "$@"
}
function Tab
{
	xic Tab "$@"
}
function Toggle
{
	xic Toggle "$@"
}
function ToggleInsert
{
	xic ToggleInsert "$@"
}
function ToggleReverse
{
	xic ToggleReverse "$@"
}
function Transfer
{
	xic Transfer "$@"
}
function Up
{
	xic Up "$@"
}
function Wait
{
	xic Wait "$@"
}

# s3270 cursor column
function Cursor_col
{
	xi -s 10
}

# s3270 connection status
function Cstatus
{
	xi -s 4
}

# Start function
function Start
{
	# Set up pipes for s3270 I/O
	ip=/tmp/ip.$$
	op=/tmp/op.$$
	rm -f $ip $op
	trap "rm -f $ip $op" EXIT
	trap "exit" INT QUIT HUP TERM
	mknod $ip p
	mknod $op p

	# Start s3270
	s3270 "$@" <$ip >$op &
	xp=$!
	exec 5>$ip 6<$op	# hold the pipes open
	xi -s 0 >/dev/null || exit 1
}

# Stop function
function Stop
{
	# Close the pipes.
	exec 5>&- 6<&-

	# Remove them.
	rm -f $ip $op
}

# Failure.
function Die
{
	echo >&2 "$@"
	Stop
	exit 1
}
