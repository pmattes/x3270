#! /bin/bash
# TSO login script, to be run via the x3270 Script() action.
# bash version

set -x
me=${0##*/}

# Make sure we're in the right environment.
if [ -z "$X3270INPUT" -o -z "$X3270OUTPUT" ]
then	echo >&2 "$me: must be run via the x3270 Script() action."
	exit 1
fi

# Set up login parameters
tcp_host=${1-ibmsys}
dial_user=${2-VTAM}
sna_host=${3-TSO}
userid=${4-USERID}
password=${5-PASSWORD}

# Verbose flag for x3270if
v="-v"

# Define some handly local functions.

# Common x3270 Ascii function
function ascii
{
	x3270if $v 'Ascii('$1')'
}

# Common x3270 String function
function string
{
	x3270if $v 'String("'"$@"'")'
}

# x3270 cursor column
function cursor_col
{
	x3270if $v -s 10
}

# x3270 connection status
function cstatus
{
	x3270if $v -s 4
}

# Failure.
function die
{
	x3270if $v "Info(\"$me error: $@\")"
	x3270if $v "CloseScript(1)"
	exit 1
}

# Make sure we're connected.
x3270if $v Wait
[ "$(cstatus)" = N ] && die "Not connected."

# Get to a VM command screen
x3270if $v Enter

# Wait for VM's prompt
while [ "$(ascii 1,0,5)" != "Enter" ]
do	sleep 2
done

# Dial out to VTAM
string "DIAL $dial_user"
x3270if $v Enter
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
x3270if $v Enter

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
x3270if $v Enter

# Now look for "LOGON IN PROGRESS"
typeset -i nl=18+${#userid}
[ "$(ascii 0,11,$nl)" = "$userid LOGON IN PROGRESS" ] || die "Couldn't log on"

# Make sure TSO is waiting for a '***' enter
[ "$(cursor_col)" -eq 5 ] || die "Don't understand logon screen"

# Off to ISPF
x3270if $v Enter

# No need to explicitly call CloseScript -- x3270 will interpret EOF as success.
