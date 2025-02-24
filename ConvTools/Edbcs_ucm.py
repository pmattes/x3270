# Translate a .ucm file to data structures for unicode_dbcs.c.

import os
import re
import sys
from typing import Dict

class dbcs_ucm:
    def __init__(self, file: str):
        self.filename = file
        self.cs = os.path.splitext(os.path.basename(self.filename))[0]
        self.cp = m.group(1) if (m := re.match(r'^ibm-([0-9]+)_.*', self.cs)) else "???"
        self.d2u = {}
        self.u2d = {}
    
    def read(self):
        '''Read the .ucm file'''
        dmatch = re.compile(r'^<U([0-9a-zA-Z]+)> +\\x([0-9a-zA-Z]+)\\x([0-9a-zA-Z]+).*')
        inside = False
        with open(self.filename) as f:
            for line in f:
                if line.startswith('#'):
                    continue
                if line.startswith('END CHARMAP'):
                    break
                if line.startswith('CHARMAP'):
                    inside = True
                    continue
                if not inside:
                    continue
                if m := dmatch.match(line):
                    u = int(m.group(1), 16)
                    d = int(m.group(2) + m.group(3), 16)
                    self.d2u[d] = u
                    self.u2d[u] = d

    def dump1(self, text: str, dict: Dict[int, int], end = ''):
        '''Write out one of the two tables'''
        print(f'/* {text} translation table for {self.cs} */ ' + '{')
        row = ''
        any = False
        for i in range(65536):
            if i > 0 and (i % 128) == 0:
                print(f'/* {i-128:04x} */ ', end='')
                if any:
                    print(f'"{row}",')
                else:
                    print('NULL,')
                row = ''
                any = False
            if i in dict:
                row += '\\x{0:02x}\\x{1:02x}'.format((dict[i] >> 8) & 0xff, dict[i] & 0xff)
                any = True
            else:
                row += '\\x00\\x00'
        print('/* ff80 */ ', end='')
        if any:
            print(f'"{row}"', end='')
        else:
            print('NULL', end='')
        print(' }' + end)

    def dump(self):
        '''Write out the translation of the file'''

        print('    { ' + f'"cp{self.cp}", "<codepage>" /* yyy, zzz */,')

        # Unicode to EBCDIC.
        self.dump1('Unicode to EBCDIC DBCS', self.u2d, ',')

        # EBCDIC to Unicode.
        self.dump1('EBCDIC DBCS to Unicode', self.d2u)

        print('    },')

if len(sys.argv) != 2:
    print(f'Usage: {sys.argv[0]} ucm-file', file=sys.stderr)
    exit(1)
d = dbcs_ucm(sys.argv[1])
d.read()
d.dump()

