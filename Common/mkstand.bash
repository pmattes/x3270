#!/usr/bin/env bash
# Make a standalone webpage from an xxx-body webpage.
#  mkstand 'title' infile outfile

if [ $# -ne 3 ]
then	echo >&2 "usage: $0 'title' infile outfile"
	exit 1
fi

. ./version.txt

tf=/tmp/mkstand$$
rm -f $tf
trap "rm -f $tf" exit
trap "exit" INT QUIT HUP TERM

(cat <<EOF
<html>
 <head>
 <title>$1</title>
 <link HREF="http://www.w3.org/StyleSheets/Core/Steely" TYPE="text/css" REL="stylesheet">
 </head>
 <body>
EOF
 sed "s/CYEAR/$cyear/g" $2
cat <<EOF
 </body>
</html>
EOF
) >$tf
mv $tf $3
