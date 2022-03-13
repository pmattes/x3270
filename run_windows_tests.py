#!/usr/bin/env python3
# Run windows tests.

import os
import os.path
import sys
import unittest
import glob

# Check the platform.
if not sys.platform.startswith('win'):
    if sys.platform == 'cygwin':
        print('On Cygwin, this needs to run using a native Windows copy of Python.', file=sys.stderr)
    else:
        print('Only works on native Windows.', file=sys.stderr)
    exit(1)

# Build the code.
if os.system('make windows') != 0:
    exit(1)

# Run the library tests.
if os.system('make windows-lib-test') != 0:
    exit(1)

# Set the path.
if os.path.exists('obj\\win64'):
    obj = 'obj\\win64'
elif os.path.exists('obj\\win32'):
    obj = 'obj\\win32'
else:
    print("Missing object directory.", file=sys.stderr)
    exit(1)
dirs = ['s3270', 'b3270', 'c3270', 'wc3270']
os.environ['PATH'] = ';'.join([os.getcwd() + '\\' + obj + '\\' + dir for dir in dirs] + [os.environ['PATH']])

verbose = '-v ' if len(sys.argv) > 1 and sys.argv[1] == '-v' else ''

# Run the tests.
os.system(sys.executable + ' -m unittest ' + verbose + ' '.join([' '.join(glob.glob(dir + '\\Test\\test*.py')) for dir in dirs]))
