#!/usr/bin/env python3
#
# Copyright (c) 2022 Paul Mattes.
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
# Valgrind log parsing and whitelisting

import re
import sys

class valpass():
    '''Valgrind log parsing and whitelisting'''

    # The set of whitelisted walkbacks.
    whitelist = [
        # Leaks from getaddrinfo_a().
        'calloc:allocate_dtv:_dl_allocate_tls:allocate_stack:pthread_create:__gai_create_helper_thread:__gai_enqueue_request:getaddrinfo_a:',
        'calloc:allocate_dtv:_dl_allocate_tls:allocate_stack:pthread_create:__gai_notify_only:__gai_notify:handle_requests:start_thread',
        'malloc:__libc_alloc_buffer_allocate:alloc_buffer_allocate:__resolv_conf_allocate:__resolv_conf_load:__resolv_conf_get_current:__res_vinit:maybe_init:context_get:context_get:__resolv_context_get:gaih_inet.constprop.0:getaddrinfo:handle_requests:start_thread',
        'malloc:__libc_alloc_buffer_allocate:alloc_buffer_allocate:__resolv_conf_allocate:__resolv_conf_load:__resolv_conf_get_current:__res_vinit:maybe_init:context_get:context_get:__resolv_context_get:gethostbyname2_r:gaih_inet.constprop.0:getaddrinfo:handle_requests:start_thread',
        # Bad read from gmtime().
        'getenv:tzset_internal:__tz_convert:get_utc_time:',
        # This is necessary so c3270 can call setupterm separately.
        'calloc:_nc_setupterm:',
        # This one seems to be some confusion on valgrind's part. I could be wrong about it.
        'malloc:__vasprintf_internal:xs_vbuffer:xs_buffer:prompt_init:',
        # These are possibly lost by the Tcl library.
        'malloc:???:TclpAlloc:',
        'malloc:???:???:???:???:???:Tcl_CreateInterp:main',
        # tcl3270 s3270 watcher thread.
        'calloc:allocate_dtv:_dl_allocate_tls:allocate_stack:pthread_create:tcl3270_main:',
        # X11 library leaks.
        'malloc:XtMalloc:_XawImInitialize',
        # So far unexplained unininitalized memory error.
        'bcmp:resync_display:screen_disp',
        # Apparent bug in getenv().
        'getenv:__gconv_load_cache',
        'getenv:_rl_init_locale',
    ]

    def walkbacks(self, fileName):
        '''Extract the leak walkbacks from a Valgrind log'''

        # Read in the file.
        with open (fileName, 'r') as file:
            data = file.readlines()

        # Slice off the PID prefix.
        data = [re.sub('^==[0-9]+== *', '', line).strip('\n') for line in data]

        # Break it into chunks and filter them to find each leak report.
        chunk = []
        chunks = []
        for line in data + ['']:
            if line == '':
                if len(chunk) > 0 and (re.match('[0-9,]+ bytes in [0-9,]+ blocks', chunk[0]) or chunk[0].startswith('Invalid')):
                    chunks.append(chunk)
                chunk = []
            else:
                chunk.append(line)

        # Filter the chunks to get the walkbacks.
        return [':'.join([re.sub('@@.*', '', wb.split(' ')[2]) for wb in chunk[1:]]) for chunk in chunks]

    # Returns True if the log is okay, False if it contains non-whitelisted walkbacks
    def check(self, fileName: str):
        '''Check a Valgrind log for non-whitelisted leak walkbacks'''

        # Check the whitelist.
        wk = self.walkbacks(fileName)
        if all(any(wb.startswith(white) for white in self.whitelist) for wb in wk):
            return (True, [])
        return (False, [wb for wb in wk if not any(wb.startswith(white) for white in self.whitelist)])

def Usage():
    print("Usage: valpass [-check][-gen] <logfile>", file=sys.stderr)
    exit(1)
    
if __name__ == '__main__':
    if len(sys.argv) < 2:
        Usage()
    v = valpass()
    if sys.argv[1] == '-check':
        success, mismatch = v.check(sys.argv[2])
        if success:
            print('Pass')
        else:
            print('Fail')
            for m in mismatch:
                print(m)
            exit(1)
    elif sys.argv[1] == '-gen':
        for walkback in v.walkbacks(sys.argv[2]):
            print(walkback)
    else:
        Usage()
