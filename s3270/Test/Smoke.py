#!/usr/bin/env python3

import unittest
from subprocess import Popen, PIPE, DEVNULL
import requests

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

    # s3270 3270 smoke test
    def test_s3270_3270_smoke(self):

        # Start 'playback' to read s3270's output.
        playback = Popen(["playback", "-b", "-p", "9998",
            "s3270/Test/ibmlink.trc"], stdout=DEVNULL)

        # Start s3270.
        s3270 = Popen(["s3270", "127.0.0.1:9998"], stdin=PIPE, stdout=DEVNULL)

        # Feed s3270 some actions.
        s3270.stdin.write(b"PF(3)\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Wait for the processes to exit.
        rc = playback.wait(timeout=2)
        self.assertEqual(rc, 0)
        s3270.stdin.close()
        s3270.wait(timeout=2)

    # s3270 TLS smoke test
    def test_s3270_tls_smoke(self):

        # Start 'openssl s_server' to read s3270's output.
        server = Popen(["openssl", "s_server", "-cert",
            "s3270/Test/tls/TEST.crt", "-key", "s3270/Test/tls/TEST.key",
            "-port", "9997", "-quiet"], stdout=PIPE)

        # Start s3270.
        s3270 = Popen(["s3270", "-cafile", "s3270/Test/tls/myCA.pem",
            "l:a:c:t:127.0.0.1:9997=TEST" ], stdin=PIPE, stdout=DEVNULL)

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

    # s3270 httpd smoke test
    def test_s3270_httpd_smoke(self):

        # Start s3270.
        s3270 = Popen(["s3270", "-httpd", "127.0.0.1:9996"])

        # Send it a JSON GET.
        r = requests.get('http://127.0.0.1:9996/3270/rest/json/Set(monoCase)')
        s = r.json()
        self.assertEqual(s['result'], ['false'])
        self.assertEqual(s['status'], 'L U U N N 4 24 80 0 0 0x0 0.000')

        # Send it a JSON POST.
        r = requests.post('http://127.0.0.1:9996/3270/rest/post',
                json={'action': 'set', 'args': ['monoCase']})
        s = r.json()
        self.assertEqual(s['result'], ['false'])
        self.assertEqual(s['status'], 'L U U N N 4 24 80 0 0 0x0 0.000')

        # Wait for the process to exit.
        s3270.kill()
        s3270.wait();

if __name__ == '__main__':
    unittest.main()
