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
# Convert an emulator-initiated TCP connection in a pcapng capture to an
# x3270 data stream trace.

import argparse
from dataclasses import dataclass, field
import ipaddress
import struct
import sys
from typing import Iterator, Optional


PCAPNG_SECTION_HEADER = 0x0A0D0D0A
PCAPNG_INTERFACE_DESCRIPTION = 0x00000001
PCAPNG_SIMPLE_PACKET = 0x00000003
PCAPNG_ENHANCED_PACKET = 0x00000006

DLT_NULL = 0
DLT_EN10MB = 1
DLT_RAW = 101
DLT_LINUX_SLL = 113
DLT_IPV4 = 228
DLT_IPV6 = 229
DLT_LINUX_SLL2 = 276


class PcapError(Exception):
    '''An invalid or unsupported packet capture.'''


@dataclass(frozen=True)
class Endpoint:
    '''A TCP endpoint.'''

    address: bytes
    port: int

    # Format an endpoint for diagnostics.
    def __str__(self) -> str:
        address = ipaddress.ip_address(self.address)
        return f'[{address}]:{self.port}' if address.version == 6 else f'{address}:{self.port}'


@dataclass
class TcpPacket:
    '''TCP data and metadata extracted from one captured packet.'''

    source: Endpoint
    destination: Endpoint
    sequence: int
    syn: bool
    ack: bool
    payload: bytes


@dataclass
class Connection:
    '''An emulator-initiated TCP connection and its captured payload.'''

    emulator: Endpoint
    host: Endpoint
    initial_sequence: dict[Endpoint, int] = field(default_factory=dict)
    packets: list[TcpPacket] = field(default_factory=list)

    # Return the trace marker for traffic in this direction.
    def direction(self, source: Endpoint) -> str:
        return '>' if source == self.emulator else '<'


@dataclass
class TcpFlow:
    '''A bidirectional TCP flow before its endpoints are assigned roles.'''

    first: Endpoint
    second: Endpoint
    packets: list[TcpPacket] = field(default_factory=list)


class Reassembler:
    '''Reassemble one TCP byte stream without reproducing retransmissions.'''

    # Initialize a reassembler immediately after its SYN.
    def __init__(self, sequence: int):
        self.sequence = sequence
        self.next_offset = 0
        self.pending: dict[int, int] = {}

    # Add a TCP payload and return newly contiguous data.
    def feed(self, sequence: int, payload: bytes) -> bytes:
        offset = _sequence_offset(sequence, self.sequence)
        for index, value in enumerate(payload):
            position = offset + index
            if position >= self.next_offset and position not in self.pending:
                self.pending[position] = value

        output = bytearray()
        while self.next_offset in self.pending:
            output.append(self.pending.pop(self.next_offset))
            self.next_offset += 1
        return bytes(output)


# Calculate a signed offset relative to a TCP sequence number.
def _sequence_offset(sequence: int, base: int) -> int:
    difference = (sequence - base) & 0xffffffff
    return difference - 0x100000000 if difference & 0x80000000 else difference


# Return an unsigned 32-bit value from a buffer.
def _u32(data: bytes, offset: int, order: str) -> int:
    return struct.unpack_from(f'{order}I', data, offset)[0]


# Return an unsigned 16-bit value from a buffer.
def _u16(data: bytes, offset: int, order: str) -> int:
    return struct.unpack_from(f'{order}H', data, offset)[0]


# Iterate through the blocks in a pcapng file.
def _blocks(data: bytes) -> Iterator[tuple[str, int, bytes]]:
    offset = 0
    order: Optional[str] = None
    while offset < len(data):
        if len(data) - offset < 12:
            raise PcapError(f'truncated pcapng block at offset {offset}')

        block_type_bytes = data[offset:offset + 4]
        if block_type_bytes == b'\x0a\x0d\x0d\x0a':
            magic = data[offset + 8:offset + 12]
            if magic == b'\x4d\x3c\x2b\x1a':
                order = '<'
            elif magic == b'\x1a\x2b\x3c\x4d':
                order = '>'
            else:
                raise PcapError(f'invalid byte-order magic at offset {offset}')
            block_type = PCAPNG_SECTION_HEADER
        elif order is None:
            raise PcapError('pcapng file does not begin with a section header')
        else:
            block_type = _u32(data, offset, order)

        length = _u32(data, offset + 4, order)
        if length < 12 or length % 4:
            raise PcapError(f'invalid pcapng block length {length} at offset {offset}')
        if length > len(data) - offset:
            raise PcapError(f'truncated pcapng block at offset {offset}')
        if _u32(data, offset + length - 4, order) != length:
            raise PcapError(f'mismatched pcapng block length at offset {offset}')

        yield order, block_type, data[offset + 8:offset + length - 4]
        offset += length


# Iterate through captured packets and their link-layer types.
def _pcap_packets(data: bytes) -> Iterator[tuple[int, bytes]]:
    interfaces: list[int] = []
    for order, block_type, body in _blocks(data):
        if block_type == PCAPNG_SECTION_HEADER:
            interfaces = []
        elif block_type == PCAPNG_INTERFACE_DESCRIPTION:
            if len(body) < 8:
                raise PcapError('truncated interface description block')
            interfaces.append(_u16(body, 0, order))
        elif block_type == PCAPNG_ENHANCED_PACKET:
            if len(body) < 20:
                raise PcapError('truncated enhanced packet block')
            interface_id = _u32(body, 0, order)
            captured_length = _u32(body, 12, order)
            if interface_id >= len(interfaces):
                raise PcapError(f'unknown interface {interface_id} in enhanced packet block')
            if captured_length > len(body) - 20:
                raise PcapError('truncated packet data in enhanced packet block')
            yield interfaces[interface_id], body[20:20 + captured_length]
        elif block_type == PCAPNG_SIMPLE_PACKET:
            if not interfaces:
                raise PcapError('simple packet block appears before an interface description')
            if len(body) < 4:
                raise PcapError('truncated simple packet block')
            packet_length = _u32(body, 0, order)
            captured_length = min(packet_length, len(body) - 4)
            yield interfaces[0], body[4:4 + captured_length]


# Remove the link-layer header and return an IP packet.
def _ip_packet(link_type: int, packet: bytes) -> Optional[bytes]:
    if link_type == DLT_EN10MB:
        if len(packet) < 14:
            return None
        protocol = int.from_bytes(packet[12:14], 'big')
        offset = 14
        while protocol in (0x8100, 0x88a8, 0x9100):
            if len(packet) < offset + 4:
                return None
            protocol = int.from_bytes(packet[offset + 2:offset + 4], 'big')
            offset += 4
    elif link_type == DLT_NULL:
        if len(packet) < 4:
            return None
        family_le = int.from_bytes(packet[:4], 'little')
        family_be = int.from_bytes(packet[:4], 'big')
        if family_le in (2, 24, 28, 30):
            family = family_le
        elif family_be in (2, 24, 28, 30):
            family = family_be
        else:
            return None
        protocol = 0x0800 if family == 2 else 0x86dd
        offset = 4
    elif link_type == DLT_RAW:
        protocol = 0x0800 if packet and packet[0] >> 4 == 4 else 0x86dd
        offset = 0
    elif link_type == DLT_IPV4:
        protocol = 0x0800
        offset = 0
    elif link_type == DLT_IPV6:
        protocol = 0x86dd
        offset = 0
    elif link_type == DLT_LINUX_SLL:
        if len(packet) < 16:
            return None
        protocol = int.from_bytes(packet[14:16], 'big')
        offset = 16
    elif link_type == DLT_LINUX_SLL2:
        if len(packet) < 20:
            return None
        protocol = int.from_bytes(packet[:2], 'big')
        offset = 20
    else:
        return None

    return packet[offset:] if protocol in (0x0800, 0x86dd) else None


# Parse an IPv4 packet into source, destination, protocol and payload.
def _ipv4(packet: bytes) -> Optional[tuple[bytes, bytes, int, bytes]]:
    if len(packet) < 20 or packet[0] >> 4 != 4:
        return None
    header_length = (packet[0] & 0x0f) * 4
    total_length = int.from_bytes(packet[2:4], 'big')
    fragment = int.from_bytes(packet[6:8], 'big')
    if header_length < 20 or total_length < header_length or total_length > len(packet):
        return None
    if fragment & 0x1fff:
        return None
    return packet[12:16], packet[16:20], packet[9], packet[header_length:total_length]


# Parse an IPv6 packet into source, destination, protocol and payload.
def _ipv6(packet: bytes) -> Optional[tuple[bytes, bytes, int, bytes]]:
    if len(packet) < 40 or packet[0] >> 4 != 6:
        return None
    payload_end = 40 + int.from_bytes(packet[4:6], 'big')
    if payload_end > len(packet):
        return None
    protocol = packet[6]
    offset = 40
    while protocol in (0, 43, 44, 50, 51, 60):
        if protocol == 44:
            if offset + 8 > payload_end:
                return None
            if int.from_bytes(packet[offset + 2:offset + 4], 'big') & 0xfff8:
                return None
            protocol = packet[offset]
            offset += 8
        elif protocol == 51:
            if offset + 2 > payload_end:
                return None
            length = (packet[offset + 1] + 2) * 4
            if offset + length > payload_end:
                return None
            protocol = packet[offset]
            offset += length
        elif protocol == 50:
            return None
        else:
            if offset + 2 > payload_end:
                return None
            length = (packet[offset + 1] + 1) * 8
            if offset + length > payload_end:
                return None
            protocol = packet[offset]
            offset += length
    return packet[8:24], packet[24:40], protocol, packet[offset:payload_end]


# Parse a TCP packet from a captured link-layer packet.
def _tcp_packet(link_type: int, packet: bytes) -> Optional[TcpPacket]:
    ip_packet = _ip_packet(link_type, packet)
    if ip_packet is None:
        return None
    parsed = _ipv4(ip_packet) if ip_packet[0] >> 4 == 4 else _ipv6(ip_packet)
    if parsed is None:
        return None
    source_address, destination_address, protocol, payload = parsed
    if protocol != 6 or len(payload) < 20:
        return None
    header_length = (payload[12] >> 4) * 4
    if header_length < 20 or header_length > len(payload):
        return None
    flags = payload[13]
    return TcpPacket(
        Endpoint(source_address, int.from_bytes(payload[:2], 'big')),
        Endpoint(destination_address, int.from_bytes(payload[2:4], 'big')),
        int.from_bytes(payload[4:8], 'big'),
        bool(flags & 0x02),
        bool(flags & 0x10),
        payload[header_length:],
    )


# Return an unordered key for a bidirectional TCP connection.
def _connection_key(source: Endpoint, destination: Endpoint) -> frozenset[Endpoint]:
    return frozenset((source, destination))


# Extract all TCP flows from a pcapng capture.
def tcp_flows(data: bytes) -> list[TcpFlow]:
    '''Extract TCP flows without assigning emulator and host roles.'''
    flows: dict[frozenset[Endpoint], TcpFlow] = {}
    for link_type, raw_packet in _pcap_packets(data):
        packet = _tcp_packet(link_type, raw_packet)
        if packet is None:
            continue
        key = _connection_key(packet.source, packet.destination)
        flow = flows.get(key)
        if flow is None:
            flow = TcpFlow(packet.source, packet.destination)
            flows[key] = flow
        flow.packets.append(packet)
    return [flow for flow in flows.values() if any(packet.payload for packet in flow.packets)]


# Return the endpoint that sent a connection's opening SYN, if it was captured.
def _syn_initiator(flow: TcpFlow) -> Optional[Endpoint]:
    for packet in flow.packets:
        if packet.syn and not packet.ack:
            return packet.source
    return None


# Assign endpoint roles and initial sequence numbers to a TCP flow.
def _connection(flow: TcpFlow, emulator: Endpoint, host: Endpoint) -> Connection:
    connection = Connection(emulator, host)
    first_payload: dict[Endpoint, int] = {}
    for packet in flow.packets:
        if packet.syn:
            connection.initial_sequence.setdefault(packet.source, (packet.sequence + 1) & 0xffffffff)
        if packet.payload:
            first_payload.setdefault(packet.source, packet.sequence)
            connection.packets.append(packet)
    for endpoint, sequence in first_payload.items():
        connection.initial_sequence.setdefault(endpoint, sequence)
    return connection


# Extract TCP connections, assigning roles from SYNs or explicit endpoints.
def extract_connections(
    data: bytes,
    emulator: Optional[Endpoint] = None,
    host: Optional[Endpoint] = None,
) -> list[Connection]:
    '''Extract TCP connections, using a SYN or explicit endpoints to assign roles.'''
    if (emulator is None) != (host is None):
        raise PcapError('both emulator and host endpoints are required')
    flows = tcp_flows(data)
    if emulator is not None:
        key = _connection_key(emulator, host)
        for flow in flows:
            if _connection_key(flow.first, flow.second) == key:
                return [_connection(flow, emulator, host)]
        raise PcapError(f'connection {emulator} -> {host} was not found')

    connections = []
    for flow in flows:
        initiator = _syn_initiator(flow)
        if initiator is not None:
            peer = flow.second if initiator == flow.first else flow.first
            connections.append(_connection(flow, initiator, peer))
    return connections


# Print all TCP flows that could be converted to a trace.
def print_connections(data: bytes) -> list[Connection]:
    '''Print candidate connections and return the SYN-identified connections.'''
    connections = extract_connections(data)
    for index, connection in enumerate(connections):
        print(f'{index}: {connection.emulator} -> {connection.host}')
    for flow in tcp_flows(data):
        if _syn_initiator(flow) is None:
            print(
                f'no SYN: {flow.first} <-> {flow.second} '
                f'(use --emulator and --host)'
            )
    return connections


# Parse an ADDRESS:PORT endpoint option.
def parse_endpoint(value: str) -> Endpoint:
    '''Parse an IPv4 or bracketed IPv6 address with a TCP port.'''
    if value.startswith('['):
        end = value.find(']')
        if end < 0 or end + 1 >= len(value) or value[end + 1] != ':':
            raise PcapError(f'invalid endpoint {value!r}; use [IPv6-address]:port')
        address_text = value[1:end]
        port_text = value[end + 2:]
    else:
        address_text, separator, port_text = value.rpartition(':')
        if not separator or ':' in address_text:
            raise PcapError(f'invalid endpoint {value!r}; use address:port')
    try:
        address = ipaddress.ip_address(address_text)
        port = int(port_text, 10)
    except ValueError:
        raise PcapError(f'invalid endpoint {value!r}; use address:port') from None
    if not 0 <= port <= 65535:
        raise PcapError(f'invalid port in endpoint {value!r}')
    return Endpoint(address.packed, port)


# Format one trace direction's contiguous TCP data.
def format_trace(direction: str, data: bytes) -> list[str]:
    '''Format bytes as x3270 data stream trace lines.'''
    return [
        f'{direction} {f"0x{offset:x}":<5} {data[offset:offset + 32].hex()}'
        for offset in range(0, len(data), 32)
    ]


# Convert one connection's payload packets into trace lines.
def connection_trace(connection: Connection) -> list[str]:
    '''Reassemble a connection and return its emulator-relative trace lines.'''
    reassemblers: dict[Endpoint, Reassembler] = {}
    trace: list[str] = []
    for packet in connection.packets:
        if packet.source not in reassemblers:
            sequence = connection.initial_sequence.get(packet.source)
            if sequence is None:
                raise PcapError(f'missing SYN for {packet.source}')
            reassemblers[packet.source] = Reassembler(sequence)
        data = reassemblers[packet.source].feed(packet.sequence, packet.payload)
        if data:
            trace.extend(format_trace(connection.direction(packet.source), data))
    return trace


# Parse command-line options.
def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Convert an emulator-initiated TCP connection in pcapng format to an x3270 data stream trace.'
    )
    parser.add_argument('capture', help='pcapng capture file')
    parser.add_argument('-o', '--output', help='write the trace to this file instead of standard output')
    parser.add_argument(
        '-c',
        '--connection',
        type=int,
        help='zero-based connection number, as displayed by --list-connections',
    )
    parser.add_argument(
        '--list-connections',
        action='store_true',
        help='list TCP connections and exit; <-> means no opening SYN was captured',
    )
    parser.add_argument(
        '--emulator',
        metavar='ADDRESS:PORT',
        help='emulator endpoint for a trace without an opening SYN; bracket IPv6 addresses',
    )
    parser.add_argument(
        '--host',
        metavar='ADDRESS:PORT',
        help='host endpoint for a trace without an opening SYN; bracket IPv6 addresses',
    )
    return parser.parse_args()


# Run the command-line converter.
def main() -> int:
    '''Run pcapng-to-trace conversion.'''
    arguments = _arguments()
    try:
        with open(arguments.capture, 'rb') as capture:
            capture_data = capture.read()
        if arguments.list_connections:
            print_connections(capture_data)
            return 0
        emulator = parse_endpoint(arguments.emulator) if arguments.emulator else None
        host = parse_endpoint(arguments.host) if arguments.host else None
        connections = extract_connections(capture_data, emulator, host)
        if not connections:
            if tcp_flows(capture_data):
                print(
                    f'{sys.argv[0]}: no emulator-initiated TCP connection with payload found; '
                    'use --emulator and --host',
                    file=sys.stderr,
                )
                print_connections(capture_data)
                return 1
            raise PcapError('no emulator-initiated TCP connection with payload found')
        if arguments.connection is None:
            if len(connections) != 1:
                print(
                    f'{sys.argv[0]}: found {len(tcp_flows(capture_data))} connections; '
                    'use --list-connections and --connection',
                    file=sys.stderr,
                )
                print_connections(capture_data)
                return 1
            connection = connections[0]
        elif arguments.connection < 0 or arguments.connection >= len(connections):
            raise PcapError(f'connection number must be between 0 and {len(connections) - 1}')
        else:
            connection = connections[arguments.connection]
        output = '\n'.join(connection_trace(connection))
        if output:
            output += '\n'
        if arguments.output:
            with open(arguments.output, 'w', encoding='utf-8') as trace:
                trace.write(output)
        else:
            sys.stdout.write(output)
        return 0
    except (OSError, PcapError) as error:
        print(f'{sys.argv[0]}: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
