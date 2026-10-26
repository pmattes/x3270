#!/usr/bin/env python3
#
# Copyright (c) 2026 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Unit tests for pcap2trace.

import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from unittest.mock import patch

from Common.Test.pcap import pcap2trace


# Construct a pcapng block in little-endian byte order.
def block(block_type, body):
    '''Construct a pcapng block.'''
    length = len(body) + 12
    return struct.pack('<II', block_type, length) + body + struct.pack('<I', length)


# Construct an Ethernet IPv4 TCP packet.
def tcp_packet(source, destination, source_port, destination_port, sequence, flags, payload=b''):
    '''Construct an Ethernet IPv4 TCP packet.'''
    tcp = struct.pack(
        '!HHIIBBHHH',
        source_port,
        destination_port,
        sequence,
        0,
        0x50,
        flags,
        0,
        0,
        0,
    ) + payload
    ipv4 = bytes((0x45, 0)) + struct.pack('!H', 20 + len(tcp)) + b'\0\0\0\0\x40\x06\0\0'
    ipv4 += bytes(source) + bytes(destination) + tcp
    return b'\0' * 12 + b'\x08\0' + ipv4


# Create a pcapng capture with Ethernet enhanced packet blocks.
def capture(packets):
    '''Create a pcapng capture.'''
    section = block(0x0a0d0d0a, b'\x4d\x3c\x2b\x1a' + struct.pack('<HHq', 1, 0, -1))
    interface = block(1, struct.pack('<HHI', 1, 0, 65535))
    enhanced = []
    for packet in packets:
        padding = b'\0' * ((-len(packet)) % 4)
        enhanced.append(block(6, struct.pack('<IIIII', 0, 0, 0, len(packet), len(packet)) + packet + padding))
    return section + interface + b''.join(enhanced)


class TestPcap2Trace(unittest.TestCase):
    '''Test pcapng-to-trace conversion.'''

    # Verify direction, TCP segmentation, and trace line wrapping.
    def test_pcap2trace_stream(self):
        client = (192, 0, 2, 1)
        host = (192, 0, 2, 2)
        packets = [
            tcp_packet(client, host, 1000, 23, 10, 0x02),
            tcp_packet(host, client, 23, 1000, 70, 0x12),
            tcp_packet(client, host, 1000, 23, 11, 0x10, b'\xff\xfb\x28'),
            tcp_packet(host, client, 23, 1000, 71, 0x10, b'\xff\xfd'),
            tcp_packet(host, client, 23, 1000, 73, 0x10, b'\x28'),
            tcp_packet(host, client, 23, 1000, 71, 0x10, b'\xff\xfd\x28'),
        ]
        connections = pcap2trace.extract_connections(capture(packets))
        self.assertEqual(1, len(connections))
        self.assertEqual(
            [
                '> 0x0   fffb28',
                '< 0x0   fffd',
                '< 0x0   28',
            ],
            pcap2trace.connection_trace(connections[0]),
        )

    # Verify malformed pcapng input is rejected.
    def test_pcap2trace_invalid_capture(self):
        with self.assertRaisesRegex(pcap2trace.PcapError, 'does not begin'):
            pcap2trace.extract_connections(b'\0' * 12)

    # Verify the offset is a five-column field.
    def test_pcap2trace_offset_width(self):
        self.assertEqual(
            [
                '< 0x0   ' + '00' * 32,
                '< 0x20  ' + '00',
            ],
            pcap2trace.format_trace('<', b'\0' * 33),
        )

    # Verify endpoint roles can be supplied when a capture starts mid-stream.
    def test_pcap2trace_without_syn(self):
        emulator = pcap2trace.Endpoint(bytes((192, 0, 2, 1)), 1000)
        host = pcap2trace.Endpoint(bytes((192, 0, 2, 2)), 23)
        packets = [
            tcp_packet((192, 0, 2, 1), (192, 0, 2, 2), 1000, 23, 10, 0x10, b'\xff\xfb'),
            tcp_packet((192, 0, 2, 2), (192, 0, 2, 1), 23, 1000, 50, 0x10, b'\xff\xfd'),
        ]
        connections = pcap2trace.extract_connections(capture(packets), emulator, host)
        self.assertEqual(
            [
                '> 0x0   fffb',
                '< 0x0   fffd',
            ],
            pcap2trace.connection_trace(connections[0]),
        )

    # Verify endpoint options accept IPv4 and bracketed IPv6 addresses.
    def test_pcap2trace_parse_endpoint(self):
        self.assertEqual(
            pcap2trace.Endpoint(bytes((192, 0, 2, 1)), 23),
            pcap2trace.parse_endpoint('192.0.2.1:23'),
        )
        self.assertEqual(
            pcap2trace.Endpoint(bytes.fromhex('20010db8000000000000000000000001'), 23),
            pcap2trace.parse_endpoint('[2001:db8::1]:23'),
        )
        with self.assertRaisesRegex(pcap2trace.PcapError, 'invalid endpoint'):
            pcap2trace.parse_endpoint('2001:db8::1:23')

    # Verify candidate output includes connections without an opening SYN.
    def test_pcap2trace_print_connections(self):
        client = (192, 0, 2, 1)
        host = (192, 0, 2, 2)
        partial_client = (192, 0, 2, 3)
        partial_host = (192, 0, 2, 4)
        packets = [
            tcp_packet(client, host, 1000, 23, 10, 0x02),
            tcp_packet(client, host, 1000, 23, 11, 0x10, b'\xff'),
            tcp_packet(partial_client, partial_host, 2000, 3000, 20, 0x10, b'\xfe'),
        ]
        output = StringIO()
        with redirect_stdout(output):
            connections = pcap2trace.print_connections(capture(packets))
        self.assertEqual(1, len(connections))
        self.assertEqual(
            '0: 192.0.2.1:1000 -> 192.0.2.2:23\n'
            'no SYN: 192.0.2.3:2000 <-> 192.0.2.4:3000 (use --emulator and --host)\n',
            output.getvalue(),
        )

    # Verify a capture with only partial flows lists candidates and fails.
    def test_pcap2trace_only_partial_flows(self):
        packets = [
            tcp_packet((192, 0, 2, 1), (192, 0, 2, 2), 1000, 23, 10, 0x10, b'\xff'),
        ]
        with tempfile.NamedTemporaryFile() as trace:
            trace.write(capture(packets))
            trace.flush()
            output = StringIO()
            error = StringIO()
            with (
                patch.object(sys, 'argv', ['pcap2trace', trace.name]),
                redirect_stdout(output),
                redirect_stderr(error),
            ):
                self.assertEqual(1, pcap2trace.main())
        self.assertIn('use --emulator and --host', error.getvalue())
        self.assertIn('no SYN: 192.0.2.1:1000 <-> 192.0.2.2:23', output.getvalue())


if __name__ == '__main__':
    unittest.main()
