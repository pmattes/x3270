#!/usr/bin/env python3

from subprocess import Popen, PIPE, DEVNULL
import requests
import time
import re
import sys

# Try f periodically until seconds elapse.
def try_until(f, seconds, errmsg):
    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    while True:
        now = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        if now - start > seconds * 1e9:
            print(f"***** {errmsg}", file=sys.stderr, flush=True)
            assert False
        if f():
            return
        time.sleep(0.1)

# Check for a particular port being listened on.
def check_listen(port):
    r = re.compile(rf':{port} .* LISTEN ')
    def test():
        netstat = Popen('netstat -ant', shell=True, stdout=PIPE)
        stdout = netstat.communicate()[0].decode('utf8').split('\n')
        return any(r.search(line) for line in stdout)
    try_until(test, 2, f"Port {port} is not bound")

# Write a string to playback after verifying s3270 is blocked.
def to_playback(p, port, s):
    # Wait for the CursorAt command to block.
    def test():
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(Tasks)').json()
        return any('Wait(' in line for line in j['result'])
    try_until(test, 2, "emulator did not block")
    
    # Trigger the host output.
    p.stdin.write(s)
    p.stdin.flush()

# Check for pushed data.
def check_push(p, port, count):
    # Send a timing mark.
    p.stdin.write(b"t\n")
    p.stdin.flush()
    def test():
        j = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Query(TimingMarks)').json()
        return j['result'][0] == str(count)
    try_until(test, 2, "emulator did not accept the data")
