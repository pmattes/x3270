#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
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
# Display the size and SHA256 hash of a list of files.

import hashlib
import os
from pathlib import Path
import sys

suffixes = 'KMGTPEZYRQ'

def scale(size: int) -> str:
    '''Like the output of 'ls -h', but tighter (4 characters or less)'''
    # First use g format with 3 significant digits.
    # If that doesn't give us an exponent, we're done.
    g3 = f'{float(size):.3g}'
    if not 'e' in g3:
        return str(size)
    
    # We got an exponent. Split it from the mantissa.
    parts = g3.split('e+')
    mantissa = float(parts[0])
    exponent = int(parts[1])

    # Get the exponent to a multiple of 3.
    while exponent > 0 and (exponent % 3) != 0:
        mantissa *= 10.0
        exponent -= 1
    if exponent // 3 >= len(suffixes):
        return 'Huge'

    # Now we want 2 digits of precision if possible, 3 if not.
    x = f'{mantissa:.2g}'
    if 'e' in x:
        x = f'{mantissa:.3g}'
    return x + suffixes[exponent//3 - 1]

def hash(filename: str) -> str:
    '''Return the SHA256 hash of a file'''
    with open(filename, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()

for file in sys.argv[1:]:
    if not Path(file).is_file():
        print(f'No such file: {file}', file=sys.stderr)
        continue
    size = os.path.getsize(file)
    print(f'{scale(size):4}', hash(file), file)
