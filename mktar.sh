#!/usr/bin/env bash
# Create a git archive that includes an expanded copy of submodules.
set -x
. Common/version.txt
ver=${version%%[a-z]*}

# Create a temporary directory and make sure it will be cleaned up.
tmpdir=/tmp/mkt$$
tardir=$tmpdir/suite3270-$ver
trap "rm -rf $tmpdir" EXIT
trap exit INT
mkdir -p $tardir

# Create a git archive and dump it into the temporary directory.
git archive --format=tar HEAD | (cd $tardir && tar -xf -)

# Add submodules.
for mod in $(git submodule | awk '{print $2}')
do	tar -cf - "--exclude=.git*" $mod | (cd $tardir && tar -xf -)
done

# Create an archive of the result.
(cd $tmpdir && tar -czf - *) >suite3270-$version-src.tgz
