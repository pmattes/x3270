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
        print('Only works on native Windows, Cygwin and MSYS2.', file=sys.stderr)
    exit(1)

# Parse command-line options.
verbose = False
verbose_flag = ''
build = True
dirs = ['lib', 's3270', 'b3270', 'c3270', 'wc3270', 'pr3287']
args = sys.argv[1:]
while len(args) > 0 and args[0][0] == '-':
    if args[0] == '-v':
        verbose = True
        verbose_flag = '-v'
    elif args[0] == '-nobuild':
        build = False
    else:
        print(f"Unknown option '{args[0]}'")
        exit(1)
    args = args[1:]
if len(args) > 0:
    dirs = args

# Run Makefile/gcc-based tests.
def run_gcc_tests():
    # Build the code.
    if build and os.system('make windows') != 0:
        exit(1)

    # Set the path.
    if os.path.exists('obj\\win64'):
        obj = 'obj\\win64'
    elif os.path.exists('obj\\win32'):
        obj = 'obj\\win32'
    else:
        print("Missing object directory.", file=sys.stderr)
        exit(1)
    os.environ['PATH'] = ';'.join([os.getcwd() + '\\' + obj + '\\' + dir for dir in dirs] + [os.environ['PATH']])

    if 'PYTESTS' in os.environ:
        cmd = sys.executable + ' -m unittest ' + verbose_flag + ' ' + os.environ['PYTESTS']
    else:
        # Run the library tests.
        if 'lib' in dirs:
            dirs.remove('lib')
            if os.system('make windows-lib-test') != 0:
                exit(1)

        cmd = sys.executable + ' -m unittest ' + verbose_flag + ' ' + ' '.join([' '.join(glob.glob(dir + '\\Test\\test*.py')) for dir in dirs])

    # Run the tests.
    os.system(cmd)

# Run Visual Studio tests.
def run_vs_tests():
    
    # Build the code.
    os.chdir('VisualStudio')
    if build and os.system('msbuild /p:Configuration=Debug /p:Platform=x64') != 0:
        exit(1)
    os.chdir('..')

    # Run the library tests.
    if 'lib' in dirs:
        dirs.remove('lib')
        for test in ['base64', 'bind_opts', 'json', 'utf8']:
            if verbose:
                print(test + '_test')
            if os.system(f'VisualStudio\\x64\\Debug\\{test}_test.exe') !=0:
                exit(1)

    # Run the emulator tests.
    cmd = 'python3 -m unittest ' + verbose_flag + ' ' + ' '.join([' '.join(glob.glob(dir + '\\Test\\test*.py')) for dir in dirs])
    os.system(cmd)

# Guess whether we are supposed to build/run with VS or gcc.
if 'DevEnvDir' in os.environ:
    run_vs_tests()
else:
    run_gcc_tests()
