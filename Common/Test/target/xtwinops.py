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
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# x3270 test target host, XTWINOPS test page.

import aswitch
import consumer
import logging
import oopts
import re
import select
import server
import socketwrapper
from telnet_proto import telcmd, telopt
import time
from typing import Optional
from ttelnet import ttelnet

clear_screen = b'\033[2J\033[H'
prompt = b'==> '
quit = 'quit'
enter_to_continue = '\r\nPress Enter to continue '

operations = {
    'deiconify': ('1t', 'De-iconify window', '', False),
    'iconify': ('2t', 'Iconify window', '', False),
    'move': ('3;{0};{1}t', 'Move window (x y)', 'x y', False),
    'resize-pixels': ('4;{0};{1}t', 'Resize window in pixels (height width)', 'height width', False),
    'raise': ('5t', 'Raise window', '', False),
    'lower': ('6t', 'Lower window', '', False),
    'refresh': ('7t', 'Refresh window', '', False),
    'resize-chars': ('8;{0};{1}t', 'Resize window in characters (height width)', 'height width', False),
    'maximize': ('9;{0}t', 'Window restore (0) / maximize (1)', '0/1', False),
    'fullscreen': ('10;{0}t', 'Window restore (0) / fullscreen (1) / toggle (2)', '0/1/2', False),
    'window-state': ('11t', 'Report window state', '', True),
    'window-position': ('13t', 'Report window position', '', True),
    'window-size-pixels': ('14;{0}t', 'Report text area size (0) / window size (2) in pixels', '0/2', True),
    'screen-size-pixels': ('15t', 'Report screen size in pixels', '', True),
    'character-size-pixels': ('16t', 'Report character cell size in pixels', '', True),
    'textarea-chars': ('18t', 'Report text area size in characters', '', True),
    'screen-size-chars': ('19t', 'Report screen area size in characters', '', True),
    'icon-label': ('20t', 'Report icon label', '', True),
    'window-label': ('21t', 'Report window label', '', True),
    'push-title': ('22;{0}t', 'Push title both (0) / icon (1) / window (2)', '0/1/2', False),
    'pop-title': ('23;{0}t', 'Pop title both (0) / icon (1) / window (2)', '0/1/2', False),
}
response_csi = re.compile(rb'\033\[([0-9;]*)t')
response_osc = re.compile(rb'\033\]([Ll])(.*?)\033\\', re.DOTALL)
operations_by_opcode = {
    template.split(';', 1)[0].rstrip('t'): name
    for name, (template, _, _, _) in operations.items()
}
# Clean a command line without treating terminal responses as text.
def clean_command(data: bytes) -> str:
    return data.strip(b' \r\n').decode('ascii', errors='replace')

def safe_repr(s: str) -> str:
    '''Display a string with non-printable characters escaped'''
    named = {
        '\n': r'\n',
        '\t': r'\t',
        '\r': r'\r',
        '\b': r'\b',
        '\f': r'\f',
        '\v': r'\v',
        '\\': r'\\',
        '"': r'\"',
        "'": r"\'",
    }

    out = []
    for ch in s:
        code = ord(ch)

        # Python named escapes
        if ch in named:
            out.append(named[ch])
            continue

        # Printable ASCII → literal
        if 0x20 <= code <= 0x7e:
            out.append(ch)
            continue

        # Non-ASCII → literal UTF-8
        if code > 0x7f:
            out.append(ch)
            continue

        # Remaining control chars → C-style octal
        out.append(f'\\{code:03o}')

    return ''.join(out)

# Decode an XTWINOPS response, if one is present.
def response_text(data: bytes, operation: str) -> Optional[str]:
    match = response_csi.search(data)
    if match:
        raw = match.group(1).decode()
        values = raw.split(';')
        rt = f'\r\nResponse: \\033[{raw}t'
        rtn = rt + '\r\n '
        if operation == 'window-state' and len(values) == 1:
            state = {'1': 'normal', '2': 'iconified'}.get(values[0], values[0])
            return f'{rtn}state {state}'
        if operation == 'window-position' and len(values) == 3 and values[0] == '3':
            return f'{rtn}x {values[1]}, y {values[2]}'
        if operation == 'screen-size-pixels' and len(values) == 3 and values[0] == '5':
            return f'{rtn}width {values[2]}, height {values[1]}'
        if operation in ('textarea-chars', 'screen-size-chars'):
            if len(values) == 3 and values[0] in ('8', '9'):
                return f'{rtn}rows {values[1]}, columns {values[2]}'
        if operation in ('window-size-pixels', 'character-size-pixels'):
            if len(values) == 3:
                return f'{rtn}height {values[1]}, width {values[2]}'
        return rt
    match = response_osc.search(data)
    if match:
        kind = 'icon label' if match.group(1) == b'L' else 'window label'
        label = match.group(2).decode('ascii', errors='replace')
        raw = safe_repr(data[match.start():match.end()].decode('ascii', errors='backslashreplace'))
        return f'\r\nResponse: {raw}\r\n {kind} "{label}"'
    return None

class xtwinops(ttelnet, server.server, consumer.consumer):
    '''XTWINOPS test page using plain NVT mode'''

    # Initialize the page.
    def __init__(self, conn: socketwrapper.socketwrapper, logger: logging.Logger,
                 peername: str, tls: bool, switch: aswitch.aswitch,
                 opts: oopts.oopts):
        super().__init__(conn, logger, peername, self, switch)
        self.awaiting_enter = False
        self.collecting_response = False
        self.response_data = bytearray()
        self.set_cooked_input(True)
        self.quit_help = ('Return to previous menu'
                          if self.switch.is_switched(self.peername)
                          else 'Disconnect from test target')

    # Display the page.
    def ready(self) -> bool:
        self.conn.send(self.menu_text())
        return True

    # Create the XTWINOPS menu.
    def menu_text(self, last: str = '') -> bytes:
        text = clear_screen.decode() + 'XTWINOPS operations\r\n'
        if last:
            text += last + '\r\n\r\n'
        width = max(len(name) for name in operations)
        for name, (template, description, _, _) in operations.items():
            opcode = template.split(';', 1)[0].rstrip('t')
            text += f'{opcode.rjust(3)} {name.ljust(width + 1)}{description}\r\n'
        text += f'{"":3} {quit.ljust(width + 1)}{self.quit_help}\r\n'
        return text.encode() + prompt

    # Wait for the client to acknowledge character-at-a-time input.
    def wait_for_character_mode(self):
        deadline = time.monotonic() + 2.0
        while not self.character_mode_acked() and time.monotonic() < deadline:
            readable, _, _ = select.select([self.conn], [], [],
                                            deadline - time.monotonic())
            if not readable:
                break
            self.process(self.conn.recv(4096))
        return self.character_mode_acked()

    # Process a complete user command.
    def process_command(self, data: bytes):
        command = clean_command(data)
        if command == '':
            self.conn.send(prompt)
            return
        if command == quit:
            self.switch.revert(self.peername, self.conn)
            return

        fields = command.split()
        name = fields[0]
        name = operations_by_opcode.get(name, name)
        if name not in operations:
            self.conn.send(f'No such operation: {name}\r\n'.encode() + prompt)
            return

        template, description, usage, is_report = operations[name]
        argument_count = template.count('{')
        optional_param = name in ('window-size-pixels', 'push-title', 'pop-title') and len(fields[1:]) == 0
        if len(fields[1:]) != argument_count and not optional_param:
            usage_text = f' {usage}' if usage else ''
            self.conn.send(f'Usage: {name}{usage_text}\r\n'.encode() + prompt)
            return
        try:
            parameters = [int(value) for value in fields[1:]]
        except ValueError:
            usage_text = f' {usage}' if usage else ''
            self.conn.send(f'Usage: {name}{usage_text}\r\n'.encode() + prompt)
            return
        sequence = template.replace(';{0}', '') if optional_param else template.format(*parameters)
        raw_sequence = f'\\033[{sequence}'

        self.conn.send(clear_screen + f'XTWINOPS: {description}\r\n'.encode())
        self.conn.send(f'Parameters: {" ".join(fields[1:]) or "(none)"}\r\n'.encode())
        self.conn.send(f'Sent: {raw_sequence}\r\n'.encode())
        self.conn.send(b'Waiting for response...\r\n' if is_report else b'')
        if not is_report:
            self.conn.send(f'\033[{sequence}'.encode())
            self.conn.send(clear_screen)
            self.conn.send(f'XTWINOPS: {description}\r\n'.encode())
            self.conn.send(f'Parameters: {" ".join(fields[1:]) or "(none)"}\r\n\r\n'.encode())
            self.conn.send(f'Sent: {raw_sequence}\r\n'.encode())
            self.conn.send(enter_to_continue.encode())
            self.awaiting_enter = True
            return

        # Temporarily switch the terminal to character-at-a-time mode so the
        # report can arrive without waiting for an input line.
        self.response_data.clear()
        self.collecting_response = True
        self.set_cooked_input(False)
        try:
            self.enter_character_mode()
            self.wait_for_character_mode()
            self.conn.send(f'\033[{sequence}'.encode())
            result = self.wait_for_response(name)
        finally:
            self.enter_normal_mode()
            self.set_cooked_input(True)
            self.collecting_response = False
        self.conn.send(clear_screen)
        self.conn.send(f'XTWINOPS: {description}\r\n'.encode())
        self.conn.send(f'Parameters: {" ".join(fields[1:]) or "(none)"}\r\n'.encode())
        self.conn.send(f'Sent: {raw_sequence}\r\n'.encode())
        self.conn.send((result or 'Response timeout').encode()
                       + b'\r\n' + enter_to_continue.encode())
        self.awaiting_enter = True

    # Wait for and decode a terminal response.
    def wait_for_response(self, operation: str) -> Optional[str]:
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            result = response_text(bytes(self.response_data), operation)
            if result is not None:
                return result
            readable, _, _ = select.select([self.conn], [], [],
                                            deadline - time.monotonic())
            if not readable:
                break
            self.process(self.conn.recv(4096))
        if self.response_data and self.logger is not None:
            self.logger.debug(
                f'target:{self.peername}: XTWINOPS response timeout, '
                f'received {bytes(self.response_data)!r}')
        return None

    # Consume application data from TELNET.
    def rcv_data(self, data: bytes):
        if self.collecting_response:
            self.response_data.extend(data)
            return
        if self.awaiting_enter:
            self.awaiting_enter = False
            self.conn.send(self.menu_text())
            return
        self.process_command(data)

    # Reject unsolicited TELNET options.
    def rcv_will(self, option: telopt) -> bool:
        return False

    # Accept notification that a TELNET option was disabled.
    def rcv_wont(self, option: telopt) -> bool:
        return True

    # Reject unsolicited TELNET options.
    def rcv_do(self, option: telopt) -> bool:
        return False

    # Accept notification that a TELNET option was disabled.
    def rcv_dont(self, option: telopt) -> bool:
        return True

    # Ignore TELNET subnegotiation.
    def rcv_sb(self, option: telopt, data: bytes):
        pass

    # Ignore TELNET commands.
    def rcv_cmd(self, cmd: telcmd):
        pass
