#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Set up /etc/hosts for multi-address tests.

import sys
import os
import ctypes

marker = '# Entries for x3270 unit testing\n'
test_hostname = 'x3270.unittest'
script = 'Common.Test.setupHosts'

if sys.platform.startswith('win'):
    hosts_file = os.environ['WINDIR'] + '\\system32\\drivers\\etc\\hosts'
    qual='(runs elevated)'
else:
    hosts_file = '/etc/hosts'
    qual='as root'

warning = f"Need to set up /etc/hosts using '{sys.executable} -m {script}' {qual}"

def present():
    '''Check /etc/hosts for the entries'''
    with open(hosts_file, 'r') as file:
        found = False
        while True:
            line = file.readline()
            if line == '':
                return False
            if line == marker:
                return True

if __name__ == '__main__':
    if not (len(sys.argv) > 1 and sys.argv[1] == '-reverse'):
        # Add the host entries.
        if present():
            print('Not needed.')
            exit(0)
        if sys.platform.startswith('win') and ctypes.windll.shell32.IsUserAnAdmin() == 0:
            # On Windows, elevate and run this script again.
            assert 0 == os.system('powershell Common\\Test\\addhosts.ps1')
            if present():
                print(f"Done. To reverse, run '{sys.executable} -m {script} -reverse' {qual}.")
                exit(1)
            else:
                print('Failed.')
                exit(0)

        # Edit the hosts file.
        if not sys.platform.startswith('win') and os.getuid() != 0:
            print('Need to run this script as root.')
            exit(1)

        with open(hosts_file, 'a') as file:
            file.write(marker)
            file.write(f'::1 {test_hostname}\n')
            file.write(f'127.0.0.1 {test_hostname}\n')
        print(f"Done. To reverse, run '{sys.executable} -m {script} -reverse' {qual}.")

    else:
        # Remove the host entries.
        if not present():
            print('Not needed.')
            exit(0)
        if sys.platform.startswith('win') and ctypes.windll.shell32.IsUserAnAdmin() == 0:
            # On Windows, elevate and run this script again.
            assert 0 == os.system('powershell Common\\Test\\rmhosts.ps1')
            if not present():
                print('Done.')
                exit(1)
            else:
                print('Failed.')
                exit(0)

        # Edit the hosts file.
        if not sys.platform.startswith('win') and os.getuid() != 0:
            print('Need to run this script as root.')
            exit(1)
        accum = ''
        with open(hosts_file, 'r') as file:
            while True:
                line = file.readline()
                if line == '' or line == marker:
                    break
                accum += line
        with open(hosts_file, 'w') as file:
            file.write(accum)
        print('Reversed.')
