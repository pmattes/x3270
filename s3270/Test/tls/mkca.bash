#!/bin/bash

# Generate the root cert for the fake CA

echo "***** Generating the key myCA.key"
openssl genrsa -des3 -out myCA.key -passout pass:1234 2048
echo "***** Generating the root certificate myCA.pem"
openssl req -x509 -new -nodes -key myCA.key -sha256 -days 1825 -out myCA.pem -subj '/C=US/ST=Minnesota/L=Saint Paul/O=My Fake CA/OU=IT Department/CN=fakeca.com' -passin pass:1234
