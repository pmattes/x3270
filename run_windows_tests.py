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

# Set paths.
if os.path.exists('obj\\win64'):
    obj = os.getcwd() + '\\obj\\win64'
elif os.path.exists('obj\\win32'):
    obj = os.getcwd() + '\\obj\\win32'
else:
    print("Missing object directory.", file=sys.stderr)
    exit(1)
os.environ['PATH'] = obj + '\\s3270;' + obj + '\\b3270;' + obj + '\\wc3270;' + obj + '\\playback' + ';' + os.environ['PATH']

verbose = '-v ' if len(sys.argv) > 1 and sys.argv[1] == '-v' else ''

# Run the tests.
os.system(sys.executable + ' -m unittest ' + verbose + ' '.join(glob.glob('s3270\\Test\\test*.py') + glob.glob('b3270\\Test\\test*.py') + glob.glob('wc3270\\Test\\test*.py')))
