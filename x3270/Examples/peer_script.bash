#! /bin/bash
# TSO login script, which runs as a peer of x3270.
# bash version

set -x
me=${0##*/}

# Set up login parameters
tcp_host=${1-ibmsys}
dial_user=${2-VTAM}
sna_host=${3-TSO}
userid=${4-USERID}
password=${5-PASSWORD}

# Verbose flag for x3270if
#v="-v"

# Define some handly local functions.

# x3270 interface function
function xi
{
	X3270OUTPUT=6 X3270INPUT=5 x3270if 5>$ip 6<$op $v "$@"
}

# Common x3270 Ascii function
function ascii
{
	xi 'Ascii('$1')'
}

# Common x3270 String function
function string
{
	xi 'String("'"$@"'")'
}

# x3270 cursor column
function cursor_col
{
	xi -s 10
}

# x3270 connection status
function cstatus
{
	xi -s 4
}

# Failure.
function die
{
	xi "Info(\"$me error: $@\")"
	xi "CloseScript(1)"
	exit 1
}

# Set up pipes for x3270 I/O
ip=/tmp/ip.$$
op=/tmp/op.$$
rm -f $ip $op
trap "rm -f $ip $op" EXIT
trap "exit" INT QUIT HUP TERM
mknod $ip p
mknod $op p

# Start x3270
x3270 -script -model 2 <$ip >$op &
xp=$!
exec 5>$ip	# hold the pipe open
xi -s 0 >/dev/null || exit 1

# Connect to host
xi "Connect($tcp_host)" || die "Connection failed."

# Make sure we're connected.
xi Wait
[ "$(cstatus)" = N ] && die "Not connected."

# Get to a VM command screen
xi Enter

# Wait for VM's prompt
while [ "$(ascii 1,0,5)" != "Enter" ]
do	sleep 2
done

# Dial out to VTAM
string "DIAL $dial_user"
xi Enter
typeset -i sl=10+${#dial_user}
typeset -i dl=5+${#dial_user}
while [ "$(ascii 0,64,4)" != VTAM ]
do	s="$(ascii 8,0,$sl | sed 's/^ *//')"
	if [ "$s" != "DIALED TO $dial_user" -a "$s" != "" ]
	then	if [ "$(ascii 7,0,$dl)" = "DIAL $dial_user" ]
		then	die "Couldn't get to VTAM"
		fi
	fi
	sleep 2
done

# Get to the SNA host
string "$sna_host $userid"
xi Enter

# Pass VTAM digestion message and initial blank TSO screen
while [ "$(ascii 0,21,20)" = "USS COMMAND HAS BEEN" ]
do	sleep 2
done
while :
do	s="$(ascii 0,33,11 | sed 's/^ *//')"
	[ "$s" != "" ] && break
	sleep 2
done

# Now verify the "TSO/E LOGON" screen
[ "$s" = "TSO/E LOGON" ] || die "Couldn't get to TSO logon screen"

# Pump in the password
string "$password"
xi Enter

# Now look for "LOGON IN PROGRESS"
typeset -i nl=18+${#userid}
[ "$(ascii 0,11,$nl)" = "$userid LOGON IN PROGRESS" ] || die "Couldn't log on"

# Make sure TSO is waiting for a '***' enter
[ "$(cursor_col)" -eq 5 ] || die "Don't understand logon screen"

# Off to ISPF
xi Enter
xi 'CloseScript(0)'
