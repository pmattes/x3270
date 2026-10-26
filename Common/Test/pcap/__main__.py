#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
# All rights reserved.
#

if __package__:
    from .pcap2trace import main
else:
    from pcap2trace import main


if __name__ == '__main__':
    raise SystemExit(main())
