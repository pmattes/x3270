#!/usr/bin/env python3
#
# Copyright (c) 2021-2024 Paul Mattes.
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

from enum import IntEnum
import itertools
import os
from subprocess import Popen, PIPE, DEVNULL
import requests
import time
import unittest

import Common.Test.playback as playback
import Common.Test.cti as cti

def add_len(body: str) -> str:
    '''Prepend the length byte to a field'''
    lenf = (len(body) // 2) + 1
    return f'{lenf:02x}' + body

def iac_quote(body: str) -> str:
    '''Double TELNET IAC bytes'''
    return ''.join([b + b if b == 'ff' else b for b in [body[i:i+2] for i in range(0, len(body), 2)]])
    
def make_rpq(body: str) -> str:
    '''Construct the RPQ Names reply, getting the fixed fields and length right'''
    return iac_quote('0000000000000000' + add_len(ebcdic('x3270') + body))

def ebcdic(text: str) -> str:
    '''Convert text to hex EBCDIC'''
    return ''.join([f'{c:02x}' for c in text.encode('cp037')])

class RpqName(IntEnum):
    Address = 0,
    Timestamp = 1,
    Timezone = 2,
    User = 3,
    Version = 4
    def encode(self) -> str:
        return f'{self.value:02x}'

def split_list(input_list, match):
    '''Split a list by a match'''
    output = []
    for k, g in itertools.groupby(input_list, lambda x: x != match):
        if k:
            output.append(list(g))
        else:
            output.append([])
    return output
        
class TestS3270RpqNames(cti.cti):

    # s3270 RPQNAMES test, multi-session
    def s3270_rpqnames_multi_session(self, rpq: str, sessions):
        '''Test with multiple sessions, tuples provide reply, v6, stderr_count, twice_same_session'''

        # Start s3270.
        env = os.environ
        env['X3270RPQ'] = rpq
        s3270 = Popen(cti.vgwrap(['s3270']), env=env, stdin=PIPE, stdout=DEVNULL, stderr=PIPE)
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
            port, ts = cti.unused_port()
            with playback.playback(self, 's3270/Test/rpqnames.trc', port=port, ipv6=ipv6) as p:
                ts.close()

                # Connect s3270 to playback.
                loopback = '[::1]' if ipv6 else '127.0.0.1'
                s3270.stdin.write(f'Open(c:{loopback}:{port})\n'.encode())
                s3270.stdin.flush()

                # Write to the mark in the trace and discard whatever comes back until this point.
                p.send_to_mark()

                # Send the Query WSF.
                p.send_records(1, send_tm=False)
                # if t['twice_same_session']:
                    # p.send_records(1, send_tm=False)

                # Get the response.
                ret = p.send_tm()

                if t['twice_same_session']:
                    p.send_records(1, send_tm=False)

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
            split_stderr = split_list(stderr, delimiter + b'\n')
            self.assertEqual(len(sessions), len(split_stderr))
            self.assertSequenceEqual([t['stderr_count'] for t in sessions], [len(t) for t in split_stderr])
        else:
            self.assertEqual(sessions[0]['stderr_count'], len(stderr))

    # s3270 RPQNAMES test, single session
    def s3270_rpqnames(self, reply: str, rpq='', ipv6=False, stderr_count=0, twice_same_session=False):

        self.s3270_rpqnames_multi_session(rpq,
            [{'reply': reply, 'ipv6': ipv6, 'stderr_count': stderr_count, 'twice_same_session': twice_same_session}])

    def s3270quick(self, action:str):
        '''Get the output of an s3270 action'''
        port, ts = cti.unused_port()
        s3270 = Popen(['s3270', '-httpd', str(port)], stdin=DEVNULL, stdout=DEVNULL)
        ts.close()
        self.children.append(s3270)
        self.check_listen(port)
        r = requests.get(f'http://127.0.0.1:{port}/3270/rest/json/{action}')
        self.assertTrue(r.ok)
        res = r.json()['result']
        requests.get(f'http://127.0.0.1:{port}/3270/rest/json/Quit()')
        s3270.wait(timeout=2)
        return res

    def get_ts(self) -> str:
        '''Get the build timestamp'''
        timestamp = time.strptime(' '.join(self.s3270quick('Show(version)')[0].split(' ')[2:][:-1]), '%a %b %d %H:%M:%S UTC %Y')
        tsf = f'{timestamp.tm_year:04d}{timestamp.tm_mon:02d}{timestamp.tm_mday:02d}{timestamp.tm_hour:02d}{timestamp.tm_min:02d}{timestamp.tm_sec:02d}'
        return add_len(RpqName.Timestamp.encode() + tsf)

    def get_version(self) -> str:
        '''Get the version'''
        version = self.s3270quick('Show(version)')[0].split(' ')[1][1:]
        return add_len(RpqName.Version.encode() + ebcdic(version))
    
    def get_timezone(self) -> str:
        '''Get the timezone'''
        tz = -(time.timezone // 60) & 0xffff
        return add_len(RpqName.Timezone.encode() + f'{tz:04x}')

    def get_address(self, ipv6=False) -> str:
        '''Get the address'''
        return add_len(RpqName.Address.encode() + ('000a00000000000000000000000000000001' if ipv6 else '00027f000001'))

    # s3270 RPQNAMES test
    def test_s3270_rpqnames(self):
        '''Default, no fields.'''
        self.s3270_rpqnames(make_rpq(''))

    def test_s3270_rpqnames_address(self):
        '''IPv4 address'''
        self.s3270_rpqnames(make_rpq(self.get_address()), rpq='ADDRESS')

    def test_s3270_rpqnames_address_ipv6(self):
        '''IPv6 address'''
        self.s3270_rpqnames(make_rpq(self.get_address(ipv6=True)), rpq='ADDRESS', ipv6=True)

    def test_s3270_rpqnames_address_override(self):
        '''IPv4 address override'''
        self.s3270_rpqnames(make_rpq(add_len(RpqName.Address.encode() + '000201020304')), rpq='ADDRESS=1.2.3.4')

    def test_s3270_rpqnames_address_override_ipv6(self):
        '''IPv6 address override'''
        self.s3270_rpqnames(make_rpq(add_len(RpqName.Address.encode() + '000a00010002000300000000000000000004')), rpq=r'ADDRESS=1\:2\:3\:\:4')

    def test_s3270_rpqnames_version(self):
        '''Program version'''
        self.s3270_rpqnames(make_rpq(self.get_version()), rpq='VERSION')

    def test_s3270_rpqnames_timestamp(self):
        '''Program build timestamp'''
        self.s3270_rpqnames(make_rpq(self.get_ts()), rpq='TIMESTAMP')

    def test_s3270_rpqnames_timezone(self):
        '''Time zone'''
        self.s3270_rpqnames(make_rpq(self.get_timezone()), rpq='TIMEZONE')

    def test_s3270_rpqnames_timezone_override1(self):
        '''Time zone override, negative offset'''
        tz = -(60 * 6) & 0xffff
        self.s3270_rpqnames(make_rpq(add_len(RpqName.Timezone.encode() + f'{tz:04x}')), rpq='TIMEZONE=-0600')

    def test_s3270_rpqnames_timezone_override2(self):
        '''Time zone override, positive offset'''
        tz = (60 * 6) & 0xffff
        self.s3270_rpqnames(make_rpq(add_len(RpqName.Timezone.encode() + f'{tz:04x}')), rpq='TIMEZONE=0600')

    def test_s3270_rpqnames_bad_timezone_override1(self):
        '''Time zone override, nonsense text'''
        self.s3270_rpqnames(make_rpq(''), rpq='TIMEZONE=fred', stderr_count=1)

    def test_s3270_rpqnames_bad_timezone_override2(self):
        '''Time zone override, minutes > 59'''
        self.s3270_rpqnames(make_rpq(''), rpq='TIMEZONE=0099', stderr_count=1)

    def test_s3270_rpqnames_bad_timezone_override3(self):
        '''Time zone override, more than 12 hours'''
        self.s3270_rpqnames(make_rpq(''), rpq='TIMEZONE=1201', stderr_count=1)

    def test_s3270_rpqnames_bad_timezone_override4(self):
        '''Time zone override, overflow'''
        self.s3270_rpqnames(make_rpq(''), rpq='TIMEZONE=9999999999999999999999999999999999999999999999999999999999', stderr_count=1)

    def test_s3270_rpqnames_user_override_hex(self):
        '''User override in hex'''
        hex_user = '010203'
        self.s3270_rpqnames(make_rpq(add_len(RpqName.User.encode() + hex_user)), rpq=f'USER=0x{hex_user}')

    def test_s3270_rpqnames_user_override_ebcdic(self):
        '''User override in EBCDIC'''
        user = 'bob'
        self.s3270_rpqnames(make_rpq(add_len(RpqName.User.encode() + ebcdic(user))), rpq=f'USER={user}')

    def test_s3270_rpqnames_all(self):
        str = self.get_address() + \
              self.get_ts() + \
              self.get_timezone() + \
              self.get_version()
        self.s3270_rpqnames(make_rpq(str), rpq='ALL')

    def test_s3270_rpqnames_no(self):
        '''NO form of keywords'''
        str = self.get_address() + \
              self.get_ts() + \
              self.get_version()
        self.s3270_rpqnames(make_rpq(str), rpq='ALL:NOTIMEZONE')

    def test_s3270_rpqnames_overflow_user_ebcdic(self):
        '''User override too long, in EBCDIC'''
        user = ''.join(['x' for x in range(1, 512)])
        self.s3270_rpqnames(make_rpq(''), rpq=f'USER={user}', stderr_count=1)

    def test_s3270_rpqnames_overflow_user_hex(self):
        '''User override too long, in hex'''
        user = ''.join([f'{x:02x}' for x in range(0, 256)])
        self.s3270_rpqnames(make_rpq(''), rpq=f'USER=0x{user}', stderr_count=1)

    def test_s3270_rpqnames_bad_user_hex(self):
        '''User override, garbage value in hex'''
        user = '0a0b0c'
        self.s3270_rpqnames(make_rpq(add_len(RpqName.User.encode() + user)), rpq=f'USER=0x{user}J', stderr_count=1)

    def test_s3270_rpqnames_overflow(self):
        '''Overflow with field suppressed'''
        # Because the fields are always processed in order, the code has lots of untestable logic around space overflows.
        # The one case that's actually possible is if there is a USER field that fills the buffer, followed by the VERSION field.
        user = ''.join(['x' for x in range(1, 247)])
        self.s3270_rpqnames(make_rpq(add_len(RpqName.User.encode() + ebcdic(user))), rpq=f'USER={user}:VERSION', stderr_count=1)

    def test_s3270_rpqnames_single_errmsg(self):
        '''Multiple RPQNAMES generation attempts, verifying error messages are not repeated'''
        self.s3270_rpqnames(make_rpq(''), rpq='TIMEZONE=fred', stderr_count=1, twice_same_session=True)

    def test_s3270_rpqnames_changed_errmsg(self):
        '''Overflow with VERSION field suppressed, works with v4, fails with v6, uses more-generic infra'''
        user = ''.join(['x' for x in range(1, 220)])
        rpq = 'ADDRESS:USER=' + user + ':VERSION'
        # With and IPv4 address, there is room for the VERSION. With an IPv6 address, there is not.
        reply_v4 = make_rpq(self.get_address() + add_len(RpqName.User.encode() + ebcdic(user)) + self.get_version())
        reply_v6 = make_rpq(self.get_address(ipv6=True) + add_len(RpqName.User.encode() + ebcdic(user)))
        self.s3270_rpqnames_multi_session(rpq,
            [{ 'reply': reply_v4, 'ipv6': False, 'stderr_count': 0, 'twice_same_session': False },
             { 'reply': reply_v6, 'ipv6': True, 'stderr_count': 1, 'twice_same_session': False }])

    def test_s3270_rpqnames_whitespace1(self):
        '''White space inside the environment variable, should be ignored'''
        tz = -(60 * 6) & 0xffff
        self.s3270_rpqnames(make_rpq(add_len(RpqName.Timezone.encode() + f'{tz:04x}')), rpq='  TIMEZONE  =  -0600  ')

    def test_s3270_rpqnames_whitespace2(self):
        '''White space inside the environment variable, *not* ignored for USER after the ='''
        user = ' a b  '
        self.s3270_rpqnames(make_rpq(add_len(RpqName.User.encode() + ebcdic(user))), rpq=f'  USER  ={user}:')

if __name__ == '__main__':
    unittest.main()
