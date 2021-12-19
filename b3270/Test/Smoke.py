#!/usr/bin/env python3

import unittest
from subprocess import Popen, PIPE, DEVNULL

# b3270 smoke tests
class TestB3270Smoke(unittest.TestCase):

    # b3270 NVT smoke test
    def test_b3270_nvt_smoke(self):

        # Start 'nc' to read b3270's output.
        nc = Popen(["nc", "-v", "-l", "127.0.0.1", "9999"], stdout=PIPE)

        # Start b3270.
        b3270 = Popen(["b3270"], stdin=PIPE, stdout=DEVNULL)

        # Feed b3270 some actions.
        b3270.stdin.write(b"<b3270-in>\n")
        b3270.stdin.write(b'<run actions="Open(a:c:t:127.0.0.1:9999) String(abc) Enter() Disconnect()"/>\n')
        b3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.stdout.read()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        b3270.stdin.write(b'</b3270-in>\n')
        b3270.stdin.flush()
        b3270.stdin.close()
        b3270.wait(timeout=2)
        nc.stdout.close()
        nc.wait(timeout=2)

if __name__ == '__main__':
    unittest.main()
