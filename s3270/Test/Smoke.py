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

    # s3270 TLS smoke test
    def test_s3270_tls_smoke(self):

        # Start 'openssl s_server' to read s3270's output.
        server = Popen(["openssl", "s_server", "-cert",
            "s3270/Test/tls/TEST.crt", "-key", "s3270/Test/tls/TEST.key",
            "-port", "9998", "-quiet"], stdout=PIPE)

        # Start s3270.
        s3270 = Popen(["s3270", "-cafile", "s3270/Test/tls/myCA.pem",
            "l:a:c:t:127.0.0.1:9998=TEST" ], stdin=PIPE, stdout=DEVNULL)

        # Feed s3270 some actions.
        s3270.stdin.write(b"String(abc)\n")
        s3270.stdin.write(b"Enter()\n")
        s3270.stdin.write(b"Disconnect()\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Make sure they are passed through.
        out = server.stdout.read(5)
        self.assertEqual(b"abc\r\n", out)

        # Wait for the processes to exit.
        server.stdout.close()
        server.kill()
        server.wait(timeout=2)
        s3270.stdin.close()
        s3270.wait(timeout=2)
if __name__ == '__main__':
    unittest.main()
