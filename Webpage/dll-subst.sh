#!/usr/bin/env bash

set -e
. openssl-url.txt
sed -e "s@%URL32%@$URL32@g" -e "s@%URL64%@$URL64@g"
