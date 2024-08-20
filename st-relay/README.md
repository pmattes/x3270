# README for the st-relay service
## Overview
st-relay is a service that relays TELNET connections from one port (and host) to another, handling TLS encryption and TELNET STARTTLS negotiation. It is a bit like a single-purpose version of *stunnel*.

## Protocol negotiation
st-relay handles both client-side and server-side TLS initiation.

In the default negotiation mode, st-relay waits up to 2 seconds for the client to initiate a TLS tunnel. After 2 seconds, it sends a TELNET DO STARTTLS sequence, asking the client to initiate the tunnel. If the client does not agree to STARTTLE and create the tunnel within 5 seconds, st-relay closes the connection.

If the client sends other TELNET negotiations besides WILL STARTTLS, st-relay refuses them with a DONT. If the client refuses STARTTLS (WONT STARTTLS), st-relay closes the connection.

Once TLS negotiation has begun, it must complete within 0.5 seconds.

## Logging
By default, st-relay sends log messages of level WARNING and above to its standard output. It can be configured to send them to a rotating file instead, and the minimum logging level can also be set. Logging can also be turned off altogether.

## IPv6 support
If the platform supports IPv6-mapped addresses (e.g., Linux), then listening on **::** (the IPv6 any-host address) will allow both IPv4 and IPv6 connections to be accepted by the same st-relay instance. Otherwise, separate instances of st-relay will need to be created to accept IPv4 and IPv6 connections.

## Options
### --cert *cert*
Pathname of the server certificate file. (required)
### --key *key*
Pathname of the server certificate key file. (required)
### --fromadress *address*
Listen for connections on *address*. The default is **::**, the IPv6 any-host address.
### --fromport *port*
Listen for connections on *port*. The default is **8023**.
### --toaddress *address*
Relay connections to *address*. The default is **::1**, the IPv6 loopback address.
### --toport *port*
Relay connections to *port*. The default is **3270**.
### --log *level*
Log messages at *level* and above. Possible values are **DEBUG**, **INFO**, **WARNING** (the default) and **ERROR**, plus **NONE** to turn off logging alogether.
### --logfile *filename*
Send log messages to the specified *filename* (a full path) instead of to standard output. The file will be rotated when it reaches 128 Kbytes, and at most 10 copies will be kept.
### --tls *mode*
Operate in the specified TLS negotiation mode. **none** means no TLS support; the relay is completely passive. **immediate** means that the client must create a TLS tunnel immediately; there is no TELNET negotiation. **negotiated** (the default) means that st-relay operates as described above under *Protocol negotiation*.
