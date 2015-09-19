#! /bin/sh
# TSO login script, which runs as a peer of x3270.
# sh version
set -x
me=`echo $0 | sed 's/.*\///'`

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
xi()
{
	X3270OUTPUT=6 X3270INPUT=5 x3270if 5>$ip 6<$op $v "$@"
}

# Common x3270 Ascii function
ascii()
{
	xi 'Ascii('$1')'
}

# Common x3270 String function
string()
{
	xi 'String("'"$@"'")'
}

# x3270 cursor column
cursor_col()
{
	xi -s 10
}

# x3270 connection status
cstatus()
{
	xi -s 4
}

# Failure.
die()
{
	xi "Info(\"$me error: $@\")"
	xi "CloseScript(1)"
	exit 1
}

# Set up pipes for x3270 I/O
ip=/tmp/ip.$$
op=/tmp/op.$$
rm -f $ip $op
trap "rm -f $ip $op" 0
trap "exit" 2 3 1 15
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
[ "`cstatus`" = N ] && die "Not connected."

# Get to a VM command screen
xi Enter

# Wait for VM's prompt
while [ "`ascii 1,0,5`" != "Enter" ]
do	sleep 2
done

# Dial out to VTAM
string "DIAL $dial_user"
xi Enter
len0=`expr length $dial_user`
sl=`expr 10 + $len0`
dl=`expr 5 + $len0`
while [ "`ascii 0,64,4`" != VTAM ]
do	s="`ascii 8,0,$sl | sed 's/^ *//'`"
	if [ "$s" != "DIALED TO $dial_user" -a "$s" != "" ]
	then	if [ "`ascii 7,0,$dl`" = "DIAL $dial_user" ]
		then	die "Couldn't get to VTAM"
		fi
	fi
	sleep 2
done

# Get to the SNA host
string "$sna_host $userid"
xi Enter

# Pass VTAM digestion message and initial blank TSO screen
while [ "`ascii 0,21,20`" = "USS COMMAND HAS BEEN" ]
do	sleep 2
done
while :
do	s="`ascii 0,33,11 | sed 's/^ *//'`"
	[ "$s" != "" ] && break
	sleep 2
done

# Now verify the "TSO/E LOGON" screen
[ "$s" = "TSO/E LOGON" ] || die "Couldn't get to TSO logon screen"

# Pump in the password
string "$password"
xi Enter

# Now look for "LOGON IN PROGRESS"
len0=`expr length $userid`
nl=`expr 18 + $len0`
[ "`ascii 0,11,$nl`" = "$userid LOGON IN PROGRESS" ] || die "Couldn't log on"

# Make sure TSO is waiting for a '***' enter
[ "`cursor_col`" -eq 5 ] || die "Don't understand logon screen"

# Off to ISPF
xi Enter
xi 'CloseScript(0)'
