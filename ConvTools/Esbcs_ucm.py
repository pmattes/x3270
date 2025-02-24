# Translate a .ucm file to data structures for unicode_sbcs.c.

import os
import re
import sys

class sbcs_ucm:
    def __init__(self, file: str):
        self.filename = file
        self.cs = os.path.splitext(os.path.basename(self.filename))[0]
        self.cp = m.group(1) if (m := re.match(r'^ibm-([0-9]+)_.*', self.cs)) else "???"
        self.d2u = {}
    
    def read(self):
        '''Read the .ucm file'''
        dmatch = re.compile(r'^<U([0-9a-zA-Z]+)> +\\x([0-9a-zA-Z]+) .*')
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
                    d = int(m.group(2), 16)
                    self.d2u[d] = u

    def dump(self):
        '''Write out the table'''
        any = False
        print('{ ' + f'"cp{self.cp}",' + ' {')
        print(' /* 0x40 */        ', end='')
        for i in range(0x41, 0xff):
            if ((i - 0x40) % 8) == 0:
                if i - 0x40 != 0:
                    print(',' if any else '')
                print(f' /* 0x{i:02x} */', end='')
                any = False
            print((',' if any else ''), end='')
            if i in self.d2u:
                print(f' 0x{self.d2u[i]:04x}', end='')
            else:
                print(' 0x0000', end='')
            any = True
        print()
        print('    }, ' + f'"{self.cp}", "<cgcsgid>" /* xxx, yyy */, true, "<5250-keyboard-type>"' + ' },')

if len(sys.argv) != 2:
    print(f'Usage: {sys.argv[0]} ucm-file', file=sys.stderr)
    exit(1)
d = sbcs_ucm(sys.argv[1])
d.read()
d.dump()

