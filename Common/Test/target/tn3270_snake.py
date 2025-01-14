#!/usr/bin/env python3
#
# Copyright (c) 2023 Paul Mattes.
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
# x3270 test target host, TN3270 snake server.

import random
import threading
import time
from typing import Sequence

from ds import *
from ibm3270ds import *
import tn3270
import tn3270e_proto

snake_length = 10
#               N   NE  E SE  S  SW   W  NW
turn_row =    (-1,  -1, 0, 1, 1,  1,  0, -1)
turn_column = ( 0,   1, 1, 1, 0, -1, -1, -1)

class snake(tn3270.tn3270_server):
    '''TN3270 protocol server playing snake'''

    def rcv_data_cooked(self, data: bytes, mode=tn3270e_proto.data_type.d3270_data):
        '''Consume data'''
        if mode != tn3270e_proto.data_type.d3270_data or not self.in3270:
            return
        match data[0]:
            case aid.PF3.value:
                cmd = bytes([command.erase_write, wcc.reset | wcc.keyboard_restore])
                self.send_host(cmd)
                self.stop = True
                self.hangup()
            case aid.CLEAR.value:
                self.refresh()
            case aid.SF.value:
                # Parse the QueryReply, assuming that's what it is
                self.debug('snake', 'got SF')
                if (self.dinfo.parse_query_reply(data)[0]):
                    self.rows = self.dinfo.alt_rows
                    self.columns = self.dinfo.alt_columns
                    self.debug('snake', f'rows {self.rows} columns {self.columns}')
                    self.query_done()
            case _:
                self.unlock()

    def refresh(self):
        '''Refresh the screen'''
        cmd = bytes([command.erase_write_alternate, wcc.reset | wcc.keyboard_restore]) + sba_bytes(self.rows, self.columns, self.columns) + bytes([order.sf, fa.protect])
        cmd += sba_bytes(self.rows, 1, self.columns) + 'F3=END'.encode('cp037')
        first = True
        for segment in self.snake:
            row = segment[0]
            column = segment[1]
            cmd += sba_bytes(row, column, self.columns)
            cmd += b'\x4b' if first else b'\x5c'
            first = False
        cmd += sba_bytes(self.rows, self.columns, self.columns) + bytes([order.ic])
        self.send_host(cmd)

    def unlock(self):
        '''Unlock the keyboard'''
        self.send_host(bytes([command.write, wcc.reset | wcc.keyboard_restore]))

    def start3270(self):
        '''Start 3270 mode'''
        if self.dinfo.extended:
            self.query()
        else:
            self.query_done()

    def query_done(self):
        self.snake = []
        self.stop = False
        t = threading.Thread(target=self.slither)
        t.start()

    def explore(self, row0: int, column0: int, direction: int) -> List[int]:
        '''Explore three moves ahead to find a legal one'''
        '''Returns the set of legal next moves, or None'''
        ret = []
        for i in range(-1, 2):
            dir1 = (direction + i) % 8
            row1 = row0 + turn_row[dir1]
            column1= column0 + turn_column[dir1]
            if row1 < 1 or row1 > self.rows - 1 or column1 < 1 or column1 > self.columns:
                continue
            for j in range(-1, 2):
                if i in ret:
                    break
                dir2 = (dir1 + j) % 8
                row2 = row1 + turn_row[dir2]
                column2 = column1 + turn_column[dir2]
                if row2 < 1 or row2 > self.rows - 1 or column2 < 1 or column2 > self.columns:
                    continue
                for k in range(-1, 2):
                    dir3 = (dir2 + j) % 8
                    row3 = row2 + turn_row[dir3]
                    column3 = column2 + turn_column[dir3]
                    if not (row3 < 1 or row3 > self.rows - 1 or column3 < 1 or column3 > self.columns):
                        ret.append(i)
                        break
        if 0 in ret:
            ret.append(0)
        return ret if len(ret) > 0 else None


    def next(self, head_row: int, head_column: int) -> Sequence[int]:
        '''Compute the next head'''
        # Directions range from 0 to 7, clockwise. 0 is up, 1 is up and to the right, 2 is right, etc.
        # We change direction only by +1 (right), 0 (same) or -1 (left) for each iteration.
        # The chances of moving in the same direction are twice as good as turning left or right.
        direction = (self.direction + self.legal[random.randint(0, len(self.legal) - 1)]) % 8
        row = head_row + turn_row[direction]
        column = head_column + turn_column[direction]
        self.legal = self.explore(row, column, direction)
        self.direction = direction
        return (row, column)

    def slither(self):
        '''Draw the snake forever'''
        # Build it up.
        self.direction = 1 # up and to the right
        self.legal = [-1, 0, 0, 1] # all legal, straight preferred
        row = self.rows - 2
        column = 1
        self.snake = [(row, column)]
        for i in range(0, snake_length - 1):
            self.snake = [self.next(self.snake[0][0], self.snake[0][1])] + self.snake
        self.refresh()

        # Update forever.
        while not self.stop:
            cmd = bytes([command.write, wcc.reset | wcc.keyboard_restore])
            head = self.snake[0]
            head_row = head[0]
            head_column = head[1]
            (row, column) = self.next(head_row, head_column)
            tail = self.snake[-1]
            self.snake = [(row, column)] + self.snake[0 : snake_length]
            if not tail in self.snake:
                cmd += sba_bytes(tail[0], tail[1], self.columns) + b'\x40'
            cmd += sba_bytes(row, column, self.columns) + b'\x4b'
            cmd += sba_bytes(self.snake[1][0], self.snake[1][1], self.columns) + b'\x5c'
            cmd += sba_bytes(self.rows, self.columns, self.columns) + bytes([order.ic])
            self.send_host(cmd)
            time.sleep(0.05)
