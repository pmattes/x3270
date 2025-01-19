#!/usr/bin/env python3
#
# Copyright (c) 2021-2025 Paul Mattes.
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
# s3270 RPQNAMES tests

import os
import socket
from subprocess import Popen, PIPE, DEVNULL
import time
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback
import Common.Test.rpq as rpq

def split_list(input_list, match):
    '''Split a list by a match'''
    result = []
    accum = None
    for elt in input_list:
        if elt == match:
            if accum == None:
                accum = []
            result.append(accum)
            accum = []
        else:
            if accum == None:
                accum = [elt]
            else:
                accum.append(elt)
    if accum != None:
        result.append(accum)
    return result

# Define the AF value for IPv6.
v6family = f'{int(socket.AF_INET6):02x}'

@requests_timeout
class TestS3270RpqNames(cti):

    # s3270 RPQNAMES test, multi-session
    def s3270_rpqnames_multi_session(self, rpq: str, sessions):
        '''Test with multiple sessions, dicts provide reply, v6, stderr_count, twice_same_session, set_value'''

        # Start s3270.
        env = os.environ.copy()
        if rpq != None:
            env['X3270RPQ'] = rpq
        s3270 = Popen(vgwrap(['s3270']), env=env, stdin=PIPE, stdout=DEVNULL, stderr=PIPE)
        self.children.append(s3270)

        delimiter = b'-----'

        for i, t in enumerate(sessions):
            if i > 0:
                # Send a marker to stderr to separate each session's output.
                # This is a little obscure. s3270 async errors go to stderr, so we use a variant of the Fail()
                # action to generate one.
                s3270.stdin.write(b'Fail(-async,' + delimiter + b')\n')
                s3270.stdin.flush()

            reply = t['reply']
            ipv6 = t['ipv6']

            # Start 'playback' to read s3270's output.
            port, ts = unused_port()
            with playback(self, 's3270/Test/rpqnames.trc', port=port, ipv6=ipv6) as p:
                ts.close()

                # Connect s3270 to playback.
                set_value = t['set_value']
                if set_value != None:
                    s3270.stdin.write(f'Set(rpq,{set_value})'.encode())
                loopback = '[::1]' if ipv6 else '127.0.0.1'
                s3270.stdin.write(f'Open(c:{loopback}:{port})\n'.encode())
                s3270.stdin.flush()

                # Write to the mark in the trace and discard whatever comes back until this point.
                p.send_to_mark()

                # Send the Query WSF.
                p.send_records(1, send_tm=False)

                # Get the response.
                ret = p.send_tm()

                if t['twice_same_session']:
                    p.send_records(1)

                # Parse the response.
                prefix = ret[:10]
                self.assertTrue(prefix.startswith('88')) # QueryReply
                self.assertTrue(prefix.endswith('81a1')) # RPQ names
                ret = ret[10:]
                self.assertTrue(ret.endswith('ffeffffc06')) # IAC EOR IAC WONT TM
                ret = ret[:-10]
                self.assertEqual(reply, ret)

        # Wait for s3270 to exit.
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270)

        # Verify stderr.
        stderr = s3270.stderr.readlines()
        s3270.stderr.close()

        # Split the errors.
        if len(sessions) > 1:
            split_stderr = split_list(stderr, delimiter + os.linesep.encode())
            # print('sessions', sessions, 'stderr', stderr, 'split_stderr', split_stderr)
            self.assertEqual(len(sessions), len(split_stderr))
            self.assertSequenceEqual([t['stderr_count'] for t in sessions], [len(t) for t in split_stderr])
        else:
            self.assertEqual(sessions[0]['stderr_count'], len(stderr))

    # s3270 RPQNAMES test, single session
    def s3270_rpqnames(self, reply: str, rpq='', ipv6=False, stderr_count=0, twice_same_session=False, set_value=None):
        self.s3270_rpqnames_multi_session(rpq,
            [{'reply': reply, 'ipv6': ipv6, 'stderr_count': stderr_count, 'twice_same_session': twice_same_session, 'set_value': set_value}])

    def s3270quick(self, action:str):
        '''Get the output of an s3270 action'''
        port, ts = unused_port()
        s3270 = Popen(['s3270', '-httpd', str(port)], stdin=DEVNULL, stdout=DEVNULL)
        ts.close()
        self.children.append(s3270)
        self.check_listen(port)
        r = self.get(f'http://127.0.0.1:{port}/3270/rest/json/{action}')
        self.assertTrue(r.ok)
        res = r.json()['result']
        self.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        s3270.wait(timeout=2)
        return res

    def get_timestamp(self) -> str:
        '''Get the build timestamp'''
        timestamp = time.strptime(' '.join(self.s3270quick('Show(version)')[0].split(' ')[2:][:-1]), '%a %b %d %H:%M:%S UTC %Y')
        tsf = f'{timestamp.tm_year:04d}{timestamp.tm_mon:02d}{timestamp.tm_mday:02d}{timestamp.tm_hour:02d}{timestamp.tm_min:02d}{timestamp.tm_sec:02d}'
        return rpq.add_len(rpq.RpqName.Timestamp.encode() + tsf)

    def get_version(self) -> str:
        '''Get the version'''
        version = self.s3270quick('Show(version)')[0].split(' ')[1][1:]
        return rpq.add_len(rpq.RpqName.Version.encode() + rpq.ebcdic(version))
    
    def get_timezone(self) -> str:
        '''Get the timezone'''
        tz = -(time.timezone // 60) & 0xffff
        return rpq.add_len(rpq.RpqName.Timezone.encode() + f'{tz:04x}')

    def get_address(self, ipv6=False) -> str:
        '''Get the address'''
        return rpq.add_len(rpq.RpqName.Address.encode() + ('00' + v6family + '00000000000000000000000000000001' if ipv6 else '00027f000001'))

    # Default, no fields
    def test_s3270_rpqnames(self):
        self.s3270_rpqnames(rpq.make_rpq(''))

    # IPv4 address
    def test_s3270_rpqnames_address(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_address()), rpq='ADDRESS')

    # IPv6 address
    def test_s3270_rpqnames_address_ipv6(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_address(ipv6=True)), rpq='ADDRESS', ipv6=True)

    # IPv4 address override
    def test_s3270_rpqnames_address_override(self):
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Address.encode() + '000201020304')), rpq='ADDRESS=1.2.3.4')

    # IPv6 address override
    def test_s3270_rpqnames_address_override_ipv6(self):
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Address.encode() + '00' + v6family + '00010002000300000000000000000004')), rpq=r'ADDRESS=1\:2\:3\:\:4')

    # Program version
    def test_s3270_rpqnames_version(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_version()), rpq='VERSION')

    # Program build timestamp
    def test_s3270_rpqnames_timestamp(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_timestamp()), rpq='TIMESTAMP')

    # Time zone
    def test_s3270_rpqnames_timezone(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_timezone()), rpq='TIMEZONE')

    # Time zone, with lots of contradictory noise beforehand
    def test_s3270_rpqnames_timezone(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_timezone()), rpq='TIMEZONE:NOTIMEZONE:ALL:NOALL:TIMEZONE')

    # Time zone override, negative offset
    def test_s3270_rpqnames_timezone_override1(self):
        tz = -(60 * 6) & 0xffff
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Timezone.encode() + f'{tz:04x}')), rpq='TIMEZONE=-0600')

    # Time zone override, positive offset
    def test_s3270_rpqnames_timezone_override2(self):
        tz = (60 * 6) & 0xffff
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Timezone.encode() + f'{tz:04x}')), rpq='TIMEZONE=0600')

    # Time zone override, nonsense text
    def test_s3270_rpqnames_bad_timezone_override1(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=fred', stderr_count=1)

    # Time zone override, minutes > 59
    def test_s3270_rpqnames_bad_timezone_override2(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=0099', stderr_count=1)

    # Time zone override, more than 12 hours
    def test_s3270_rpqnames_bad_timezone_override3(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=1201', stderr_count=1)

    # Time zone override, garbage after value
    def test_s3270_rpqnames_bad_timezone_override4(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=0600 junk!', stderr_count=1)

    # Time zone override, overflow
    def test_s3270_rpqnames_bad_timezone_override5(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=9999999999999999999999999999999999999999999999999999999999', stderr_count=1)

    # Time zone specifying NO form and override
    def test_s3270_rpqnames_bad_timezone_override6(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='NOTIMEZONE=-0600', stderr_count=1)

    # User override in hex
    def test_s3270_rpqnames_user_override_hex(self):
        hex_user = '010203'
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + hex_user)), rpq=f'USER=0x{hex_user}')

    # User override in EBCDIC
    def test_s3270_rpqnames_user_override_ebcdic(self):
        user = 'bob'
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))), rpq=f'USER={user}')

    # ALL meaning everything
    def test_s3270_rpqnames_all(self):
        str = self.get_address() + \
              self.get_timestamp() + \
              self.get_timezone() + \
              self.get_version()
        self.s3270_rpqnames(rpq.make_rpq(str), rpq='ALL')

    # NOALL meaning nothing
    def test_s3270_rpqnames_noall(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='NOALL')
    
    # ALL in lowercase
    def test_s3270_rpqnames_all2(self):
        str = self.get_address() + \
              self.get_timestamp() + \
              self.get_timezone() + \
              self.get_version()
        self.s3270_rpqnames(rpq.make_rpq(str), rpq='all')

    # NO form or keywords
    def test_s3270_rpqnames_no(self):
        str = self.get_address() + \
              self.get_timestamp() + \
              self.get_version()
        self.s3270_rpqnames(rpq.make_rpq(str), rpq='ALL:NOTIMEZONE')
    
    # NO form of keywords, mixed case
    def test_s3270_rpqnames_no2(self):
        str = self.get_address() + \
              self.get_timestamp() + \
              self.get_version()
        self.s3270_rpqnames(rpq.make_rpq(str), rpq='AlL:NoTimeZone')

    # User override too long, in EBCDIC
    def test_s3270_rpqnames_overflow_user_ebcdic(self):
        user = ''.join(['x' for x in range(1, 512)])
        self.s3270_rpqnames(rpq.make_rpq(''), rpq=f'USER={user}', stderr_count=1)

    # User override too long, in hex
    def test_s3270_rpqnames_overflow_user_hex(self):
        user = ''.join([f'{x:02x}' for x in range(0, 256)])
        self.s3270_rpqnames(rpq.make_rpq(''), rpq=f'USER=0x{user}', stderr_count=1)

    # User override, garbage value in hex
    def test_s3270_rpqnames_bad_user_hex(self):
        user = '0a0b0c'
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + user)), rpq=f'USER=0x{user}J', stderr_count=1)

    # Overflow with field suppressed
    def test_s3270_rpqnames_overflow(self):
        # Because the fields are always processed in order, the code has lots of untestable logic around space overflows.
        # The one case that's actually possible is if there is a USER field that fills the buffer, followed by the VERSION field.
        user = ''.join(['x' for x in range(1, 247)])
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))), rpq=f'USER={user}:VERSION', stderr_count=1)

    # Multiple RPQNAMES generation attempts, verifying error messages are not repeated
    def test_s3270_rpqnames_single_errmsg(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE=fred', stderr_count=1, twice_same_session=True)

    # Overflow with VERSION field suppressed, works with v4, fails with v6, uses more-generic infra
    def test_s3270_rpqnames_changed_errmsg(self):
        user = ''.join(['x' for x in range(1, 220)])
        rpq_spec = 'ADDRESS:USER=' + user + ':VERSION'
        # With and IPv4 address, there is room for the VERSION. With an IPv6 address, there is not.
        reply_v4 = rpq.make_rpq(self.get_address() + rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user)) + self.get_version())
        reply_v6 = rpq.make_rpq(self.get_address(ipv6=True) + rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user)))
        self.s3270_rpqnames_multi_session(rpq_spec,
            [{ 'reply': reply_v4, 'ipv6': False, 'stderr_count': 0, 'twice_same_session': False, 'set_value': None },
             { 'reply': reply_v6, 'ipv6': True, 'stderr_count': 1, 'twice_same_session': False, 'set_value': None }])

    # White space inside the environment variable, should be ignored
    def test_s3270_rpqnames_whitespace1(self):
        tz = -(60 * 6) & 0xffff
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Timezone.encode() + f'{tz:04x}')), rpq='  TIMEZONE  =  -0600  ')

    # White space inside the environment variable, *not* ignored for USER after the =
    def test_s3270_rpqnames_whitespace2(self):
        user = ' a b  '
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))), rpq=f'  USER  ={user}:')

    # White space inside the environment variable, ADDRESS override
    def test_s3270_rpqnames_whitespace3(self):
        self.s3270_rpqnames(rpq.make_rpq(rpq.add_len(rpq.RpqName.Address.encode() + '00027f000002')), rpq='ADDRESS = 127.0.0.2 ')

    # No match on term
    def test_s3270_rpqnames_no_match(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='123', stderr_count=1)

    # Term is NO with nothing following
    def test_s3270_rpqnames_no_no(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='NO', stderr_count=1)

    # Partial match on term
    def test_s3270_rpqnames_partial_match(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_timestamp()), rpq='TIME')

    # Something other than = after a term
    def test_s3270_rpqnames_junk_after(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='TIMEZONE*', stderr_count=1)

    # Something other than a term
    def test_s3270_rpqnames_junk(self):
        self.s3270_rpqnames(rpq.make_rpq(''), rpq='*', stderr_count=1)

    # Use Set() instread of X3270RPQ
    def test_s3270_rpqnames_set_basic(self):
        self.s3270_rpqnames(rpq.make_rpq(self.get_version()), rpq='Blorf!', set_value='VERSION')

    # Use Set() to test several combinations and errors
    def test_s3270_rpqnames_set_multi(self):
        timezone_reply = self.get_timezone()
        address_reply = self.get_address()
        user='fred'
        user_reply = rpq.add_len(rpq.RpqName.User.encode() + rpq.ebcdic(user))
        self.s3270_rpqnames_multi_session('fred',
            [{ 'reply': rpq.make_rpq(timezone_reply), 'ipv6': False, 'stderr_count': 0, 'twice_same_session': False, 'set_value': 'TIMEZONE' },
             { 'reply': rpq.make_rpq(address_reply + timezone_reply), 'ipv6': False, 'stderr_count': 0, 'twice_same_session': False, 'set_value': 'ADDRESS:TIMEZONE' },
             { 'reply': rpq.make_rpq(user_reply), 'ipv6': False, 'stderr_count': 0, 'twice_same_session': False, 'set_value': f'USER={user}' },
             { 'reply': rpq.make_rpq(''), 'ipv6': False, 'stderr_count': 1, 'twice_same_session': False, 'set_value': '123' }])

    # Make sure the command-line option works
    def test_s3270_rpqnames_set_see(self):
        s3270 = Popen(vgwrap(['s3270', '-set', 'rpq=foo']), stdin=PIPE, stdout=PIPE, stderr=PIPE)
        self.children.append(s3270)

        # Push some commands to s3270.
        s3270.stdin.write(b'Set(rpq)\n')
        s3270.stdin.write(b'Set(rpq,bar)\n')
        s3270.stdin.write(b'Set(rpq)\n')

        # Wait for s3270 to exit.
        s3270.stdin.write(b"Quit()\n")
        s3270.stdin.flush()
        s3270.stdin.close()
        self.vgwait(s3270)

        # Check stdout and stderr.
        stdout = s3270.stdout.readlines()
        result = [x for x in stdout if x.startswith(b'data: ')]
        self.assertEqual([b'data: foo' + os.linesep.encode(), b'data: bar' + os.linesep.encode()], result)
        s3270.stdout.close()

        stderr = s3270.stderr.readlines()
        self.assertEqual([], stderr)
        s3270.stderr.close()

if __name__ == '__main__':
    unittest.main()
