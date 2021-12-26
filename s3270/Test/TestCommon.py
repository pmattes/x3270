#!/usr/bin/env python3

from subprocess import Popen, PIPE, DEVNULL
import requests
import time
import re
import sys

# Check for a particular port being listened on.
def check_listen(port):
    r = re.compile(rf':{port} .* LISTEN ')
    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    while True:
        now = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        if (now - start > 2e9):
            print(f"***** Port {port} is not bound", file=sys.stderr, flush=True)
            assert False
        netstat = Popen('netstat -ant', shell=True, stdout=PIPE)
        stdout = netstat.communicate()[0].decode('utf8').split('\n')
        if any(r.search(line) for line in stdout):
            break
        time.sleep(0.1)

# Write a string to playback after verifying s3270 is blocked.
def to_playback(p, port, s):
    # Wait for the CursorAt command to block.
    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    while True:
        now = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        if (now - start > 2e9):
            print("***** s3270 did not block", file=sys.stderr, flush=True)
            assert False
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
        if any('Wait(' in line for line in j['result']):
            break
        time.sleep(0.1)
    # Trigger the host output.
    p.stdin.write(s)
    p.stdin.flush()

# Check for pushed data.
def check_push(p, port, count):
    # Send a timing mark.
    p.stdin.write(b"t\n")
    p.stdin.flush()
    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    while True:
        now = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        if (now - start > 2e9):
            print("***** s3270 did not accept the data", file=sys.stderr, flush=True)
            assert False
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(TimingMarks)').json()
        if j['result'][0] == str(count):
            break
        time.sleep(0.1)
