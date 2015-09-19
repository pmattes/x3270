#!/bin/sh
# Scan conf.h to find all of the '#define X3270_xxx' statements, and translate
# them to '-DX3270_xxx' options for use by qcpp.
# This should just be inline in the Makefile, but so far I haven't found a
# way to do that.
grep '^#define X3270_' conf.h | awk '{print "-D" $2}'
