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
# A queue-based pipe reader that allows timed reads

import queue
import threading
from Common.Test.cti import cti

# Queue-based pipe reader.
class pipeq():

    pipe = None
    queue = None
    limit = -1
    count = 0

    # Initialization.
    def __init__(self, cti: cti, pipe, limit=-1):
        self.pipe = pipe
        self.limit = limit
        self.cti = cti
        self.queue = queue.Queue()
        self.thread = threading.Thread(target=self.shuttle)
        self.thread.start()

    def shuttle(self):
        '''Shuttle data from the pipe to the queue'''
        while True:
            try:
                rdata = self.pipe.readline()
            except ValueError:
                return
            if len(rdata) == 0:
                return
            self.queue.put(rdata.strip())
            if self.limit > 0:
                self.count += 1
                if self.count >= self.limit:
                    break

    def get(self, timeout=2, error='Pipe read timed out', block=True) -> bytes:
        '''Timed read'''
        try:
            r = self.queue.get(block=block, timeout=timeout)
        except queue.Empty:
            r = None
        # self.cti.assertIsNotNone(r, error)
        return r
    
    def close(self):
        self.thread.join()
