#!/bin/bash

# Run a test client, exercising the generated certificate.
openssl s_client -CAfile myCA.pem -port 9998
