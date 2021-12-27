#!/usr/bin/env python3

import unittest
from subprocess import Popen, PIPE, DEVNULL
import tempfile
import os
import pathlib
import time
import sys
import TestCommon

# pr3287 smoke tests
class TestPr3287Smoke(unittest.TestCase):

    # pr3287 smoke test
    def test_pr3287_smoke(self):

        # Start 'playback' to feed data to pr3287.
        playback = Popen(["playback", "-w", "-p", "9998",
            "pr3287/Test/smoke.trc"], stdin=PIPE, stdout=DEVNULL)
        TestCommon.check_listen(9998)

        # Start pr3287.
        (po_handle, po_name) = tempfile.mkstemp()
        (sy_handle, sy_name) = tempfile.mkstemp()
        pr3287 = Popen(["pr3287", "-command",
            f"cat >'{po_name}'; date >'{sy_name}'", "127.0.0.1:9998"])

        # Play the trace to pr3287.
        playback.stdin.write(b'm\n')
        playback.stdin.flush()

        # Wait for the sync file to appear.
        start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        while True:
            now = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
            if now - start > 2e9:
                print("***** pr3287 did not produce output", file=sys.stderr, flush=True)
                self.assertTrue(False)
            if os.lseek(sy_handle, 0, os.SEEK_END) > 0:
                break
            time.sleep(0.1)
        os.close(sy_handle)
        os.unlink(sy_name)

        # Wait for the processes to exit.
        pr3287.kill()
        pr3287.wait(timeout=2)
        playback.stdin.close()
        playback.wait(timeout=2)

        # Read back the file.
        os.lseek(po_handle, 0, os.SEEK_SET)
        new_printout = os.read(po_handle, 65536)
        os.close(po_handle)
        os.unlink(po_name)

        # Compare.
        with open('pr3287/Test/smoke.out', 'rb') as file:
            ref_printout = file.read()

        self.assertEqual(new_printout, ref_printout)

if __name__ == '__main__':
    unittest.main()
