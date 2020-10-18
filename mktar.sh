#!/usr/bin/env bash
set -x
. Common/version.txt
ver=${version%%[a-z]*}
git archive --format=tar --prefix=suite3270-$ver/ HEAD | gzip >suite3270-$version-src.tgz
