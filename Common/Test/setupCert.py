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
# Set up root CA cert for TLS tests.

import sys
import os
from subprocess import Popen, PIPE, DEVNULL

script = 'Common.Test.setupCert'

if sys.platform == 'darwin':
    # macOS.
    warning = f"Need to set up root CA cert for TLS tests using '{sys.executable} -m '{script}' as root"
    def present():
        sec = Popen(["security", "dump-trust", "-d"], stdout=PIPE,
                stderr=DEVNULL)
        sec_out = sec.communicate()[0].decode('utf8').split('\n')
        return any('fakeca' in line for line in sec_out)

elif sys.platform.startswith('win'):
    # Windows.
    warning = f"Need to set up root CA cert for TLS tests using '{sys.executable} -m {script}' (runs elevated)"

    def present():
        cu = Popen(['certutil', '-store', 'root', 'fakeca.com'], stdout=DEVNULL, stderr=DEVNULL)
        return cu.wait() == 0

else:
    # Other platforms need nothing.
    warning = 'Not needed'
    def present():
        return True

if __name__ == '__main__':

    apply = not (len(sys.argv) > 1 and sys.argv[1] == '-reverse')

    if sys.platform == 'darwin':
        # macOS.
        if apply:
            if not present():
                if os.getuid() != 0:
                    print('Need to run this script as root.')
                    exit(1)
                assert 0 == os.system('security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain Common/Test/tls/myCA.pem')
                if present():
                    print(f"Done. To reverse, run '{sys.executable} -m {script} -reverse' as root")
                else:
                    print('Failed.')
            else:
                print('Not needed.')
            exit(0)

        # Revert.
        if present():
            if os.getuid() != 0:
                print('Need to run this script as root.')
                exit(1)
            assert 0 == os.system('security remove-trusted-cert -d Common/Test/tls/myCA.pem')
            if not present():
                print('Reversed.')
            else:
                print('Failed.')
        else:
            print('Not needed.')

    elif sys.platform.startswith('win'):
        # Windows.
        if apply:
            if not present():
                assert 0 == os.system('powershell Common\\Test\\tls\\addrootca.ps1')
                if present():
                    print(f"Done. To reverse, run '{sys.executable} -m {script} -reverse' (runs elevated)")
                    exit(0)
                else:
                    print('Failed.')
                    exit(1)
            else:
                print('Not needed.')
                exit(0)

        # Revert.
        if present():
            assert 0 == os.system('powershell Common\\Test\\tls\\rmrootca.ps1')
            if not present():
                print('Done.')
                exit(0)
            else:
                print('Failed.')
                exit(1)
        else:
            print('Not needed.')
            exit(0)

    else:
        print('Not needed on this platform.')
