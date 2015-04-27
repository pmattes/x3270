#!/bin/bash

set -e
URL=$(<openssl-url.txt)
sed -e "s@%URL%@$URL@g"
