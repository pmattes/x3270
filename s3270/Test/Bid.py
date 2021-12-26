#!/usr/bin/env python3

import unittest
from subprocess import Popen, PIPE, DEVNULL
import TestCommon

# s3270 draft-04 BID tests
class TestS3270Bid(unittest.TestCase):

    # s3270 BID test
    def test_s3270_bid(self):

        # Start 'playback' to read s3270's output.
        playback = Popen(["playback", "-b", "-p", "9998",
            "s3270/Test/bid.trc"], stdout=DEVNULL)
        TestCommon.check_listen(9998)

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
        exit_code = s3270.wait(timeout=2)
        self.assertEqual(0, exit_code)

    # s3270 no-BID test
    def test_s3270_no_bid(self):

        # Start 'playback' to read s3270's output.
        playback = Popen(["playback", "-b", "-p", "9997",
            "s3270/Test/no_bid.trc"], stdout=DEVNULL)
        TestCommon.check_listen(9997)

        # Start s3270.
        s3270 = Popen(["s3270", "-xrm", "s3270.contentionResolution: false",
            "127.0.0.1:9997"], stdin=PIPE, stdout=DEVNULL)

        # Feed s3270 some actions.
        s3270.stdin.write(b"PF(3)\n")
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()

        # Wait for the processes to exit.
        rc = playback.wait(timeout=2)
        self.assertEqual(rc, 0)
        s3270.stdin.close()
        exit_code = s3270.wait(timeout=2)
        self.assertEqual(0, exit_code)

if __name__ == '__main__':
    unittest.main()
