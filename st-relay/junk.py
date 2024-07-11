# send TELNET junk

import socket


s = socket.socket(socket.AF_INET)
s.connect(("127.0.0.1", 8023))
s.send(b'\xff\xfb\x00')
s.send(b'\xff\xfb\x2e')
s.send(b'\xff\xfa\x2e\x01\xff\xf0') # IAC SB STARTTLS FOLLOWS IAC SE
while True:
    data = s.recv(1024)
    print(data)
    if data == b'':
        break
s.close()