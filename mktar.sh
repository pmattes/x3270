#!/bin/bash
. Common/version.txt
ver=${version%%[a-z]*}
tar -czf suite3270-$version-src.tgz -T manifest --transform "s@^@suite3270-$ver/@" -h --hard-dereference
