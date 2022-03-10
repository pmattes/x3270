#!/bin/bash

# Run a test server to exercise the cert.
openssl s_server -cert TEST.crt -key TEST.key -port 9998
