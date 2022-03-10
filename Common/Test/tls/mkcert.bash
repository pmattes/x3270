#!/bin/bash

# Generate the fake server certificate.

DOMAIN=${1-TEST}

echo "***** Generating the key $DOMAIN.key"
openssl genrsa -out $DOMAIN.key 2048 2>&1

echo "***** Generating the req $DOMAIN.csr"
openssl req -new -key $DOMAIN.key -out $DOMAIN.csr -subj '/C=US/ST=Minnesota/L=Saint Paul/O=Fake Company/OU=IT Department/CN=fakecompany.com'

cat > $DOMAIN.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names
[alt_names]
DNS.1 = $DOMAIN
EOF

echo "***** Generating the cert $DOMAIN.crt"
openssl x509 -req -in $DOMAIN.csr -CA myCA.pem -CAkey myCA.key -CAcreateserial \
-out $DOMAIN.crt -days 825 -sha256 -extfile $DOMAIN.ext -passin pass:1234
