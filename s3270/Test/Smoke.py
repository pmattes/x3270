#!/usr/bin/env python3

import unittest
from subprocess import Popen, PIPE, DEVNULL

# s3270 smoke tests
class TestS3270Smoke(unittest.TestCase):

    # s3270 NVT smoke test
    def test_s3270_nvt_smoke(self):

        # Start 'nc' to read s3270's output.
        nc = Popen(["nc", "-l", "127.0.0.1", "9999"], stdout=PIPE)

        # Start s3270.
        s3270 = Popen(["s3270", "a:c:t:127.0.0.1:9999"], stdin=PIPE,
                stdout=DEVNULL)

        # Feed s3270 some actions.
        s3270.stdin.write(b"String(abc)\n")
        s3270.stdin.write(b"Enter()\n")
        s3270.stdin.write(b"Disconnect()\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Make sure they are passed through.
        out = nc.stdout.read()
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        nc.stdout.close()
        nc.wait(timeout=2)
        s3270.stdin.close()
        s3270.wait(timeout=2)

if __name__ == '__main__':
    unittest.main()
