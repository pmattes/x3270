#!/bin/sh
# Create a source tarball
# srctar <outfile> <infile>...

# Validate arguments
me=`basename $0`
if [ -z "$1" -o -z "$1" ]
then	echo >&2 "usage: $0 <product> <infile>..."
    	exit 1
fi

# Product name
product=$1
shift

# Temporary directory
tempdir=/tmp/srctar$$
rm -rf $tempdir

# tar directory
tardir=$tempdir/$product
mkdir -p $tardir

# Walk the list of files, which might include subdirectories, and create
# symlinks back to the real files.
for file in $*
do	# If the subdirectory does not exist, create it
    	subdir=`dirname $file`
	if [ ! -d $tardir/$subdir ]
	then	mkdir -p $tardir/$subdir
	fi
	ln -s $PWD/$file $tardir/$file
done

# Create the tar file.
tarfile=$product-src.tgz
rm -f $tarfile
tar -czh -C $tempdir -f $tarfile $product
echo "Created $tarfile."
