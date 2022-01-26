#!/usr/bin/env python3
# Simple test executed by the Script() action
#  simple.py [-http|-socket|-pipe] text...

import os
import requests
import socket
import sys

if len(sys.argv) < 3:
    print('Must specify at least two arguments\n')
    sys.exit(1)

opts = ['-http', '-socket', '-pipe']
if not sys.argv[1] in opts:
    print('Must specify one of:', ', '.join(opts))
    sys.exit(1)

# Run the action using the HTTP server.
def run_http(command):
    r = requests.get(os.environ['X3270URL'] + 'json/' + command)
    assert(r.status_code == requests.codes.ok)

# Run the action over a socket.
def run_socket(command):
    s = socket.socket()
    s.connect(("127.0.0.1", int(os.environ['X3270PORT'])))
    s.send((command + '\n').encode('utf8'))
    res = b''
    while True:
        out = s.recv(1024)
        if out.endswith(b'ok\n') or out.endswith(b'error\n'):
            break
        res += out
    assert(res.endswith(b'ok\n'))

# Run the action over pipes.
def run_pipe(command):
    os.write(int(os.environ['X3270INPUT']), (command + '\n').encode('utf8'))
    rfd = int(os.environ['X3270OUTPUT'])
    res = b''
    while True:
        out = os.read(rfd, 1024)
        if out.endswith(b'ok\n') or out.endswith(b'error\n'):
            break
        res += out
    assert(res.endswith(b'ok\n'))

# The actions are String(args) + Disconnect()
action = 'String("' + ' '.join(sys.argv[2:]) + '") Disconnect() Quit()'

# Run them.
if sys.argv[1] == '-http':
    run_http(action)
elif sys.argv[1] == '-socket':
    run_socket(action)
elif sys.argv[1] == '-pipe':
    run_pipe(action)
