#!/bin/bash
# Construct the webpage tarball
# mktar.bash -Idir [-Idir...] -o tarball file...
unset inc
while [[ "$1" =~ ^-.* ]]
do
    case $1 in
    -I*)
	inc="$inc ${1#-I}"
	shift
	;;
    -o)
	shift
	out=$1
	shift
	;;
    -*)
	echo >&2 "Unknown option $1"
	exit 1
    esac
done

if [ -z "$inc" ]
then echo >&2 "Missing -I"
    exit 1
fi
if [ -z "$out" ]
then echo >&2 "Missing -o"
    exit 1
fi

# Set up the temporary directory to stage the files in.
tf=/tmp/mktar$$
rm -f $tf
trap "rm -rf $tf" exit
mkdir $tf

for f in $*
do	if [ -f $f ]
    	then
	    # Found directly. If it's a path, create the subdirectory, otherwise copy directly.
	    case $f in
		*/*)
		    mkdir -p $tf/${f%/*}
		    cp $f $tf/${f%/*}
		    ;;
		*)
		    cp $f $tf
		    ;;
	    esac
	else
	    # Look for it in the search path.
	    unset found
	    for d in $inc
	    do
		if [ -f $d/$f ]
		then cp $d/$f $tf
		    found=1
		    break
		fi
	    done
	    if [ -z "$found" ]
	    then echo >&2 "Cannot find $f"
		exit 1
	    fi
	fi
done

# Create the tarball.
(cd $tf && tar -czf - $*) >$out
