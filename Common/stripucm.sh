#!/usr/bin/env sh

# Strip the junk away from a .ucm file.
# For now, outputs the length, followed by the data in two columns (Unicode and
# target).  Later it should do extent compression, likely requiring Perl.

tf=/tmp/u$$
rm -rf $tf
sed	-e '1,/^CHARMAP/d' \
	-e '/^END CHARMAP/,$d' \
	-e '/^$/d' \
	-e '/^#/d' \
	-e 's/ #.*//' \
	-e 's/ |0.*//' \
	-e 's/^<U//' \
	-e 's/>//' \
	-e 's/\\x//g' \
	$1 | \
	    tr '[A-F]' '[a-f]' >$tf
echo "/* Translation tables for $2. */"
echo ""
echo '#include "xl.h"'
echo ""
sort +0 -1 $tf | ./mktbl encode_$2
sort +1    $tf | awk '{print $2 " " $1}' | ./mktbl decode_$2
#rm $tf
