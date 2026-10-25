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
# x3270a tests

import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

class TestX3270a(unittest.TestCase):

    # Run x3270a.py with test replacements for external commands.
    def run_x3270a(self, dpi=None, commands=None, environment=None,
                   arguments=None):
        with tempfile.TemporaryDirectory() as tempdir:
            directory = Path(tempdir)
            script = directory / 'x3270a'
            x3270a = shutil.which('x3270a')
            if x3270a is None:
                self.fail('x3270a is not in PATH')
            shutil.copy(x3270a, script)
            x3270 = directory / 'x3270'
            x3270.write_text(
                f'#!{sys.executable}\n'
                'import json\n'
                'import sys\n'
                'print(json.dumps(sys.argv[1:]))\n')
            x3270.chmod(x3270.stat().st_mode | stat.S_IXUSR)

            env = os.environ.copy()
            env['PATH'] = str(directory)
            for variable in ('XDG_SESSION_TYPE', 'WAYLAND_DISPLAY',
                             'XDG_CURRENT_DESKTOP'):
                env.pop(variable, None)
            if dpi is not None:
                env['DPI'] = str(dpi)
            else:
                env.pop('DPI', None)
            if environment is not None:
                env.update(environment)
            for command, output in (commands or {}).items():
                executable = directory / command
                executable.write_text(
                    f'#!{sys.executable}\n'
                    'import sys\n'
                    f'sys.stdout.write({output!r})\n')
                executable.chmod(executable.stat().st_mode | stat.S_IXUSR)

            result = subprocess.run(
                [sys.executable, script, *(arguments or [])],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                env=env, check=False)
            return result

    # Return the arguments passed to the replacement x3270.
    def x3270_arguments(self, result):
        return json.loads(result.stdout.splitlines()[-1])

    # Verify whether x3270 receives an Xft DPI resource.
    def assert_xft_dpi(self, result, dpi):
        arguments = self.x3270_arguments(result)
        resources = [
            arguments[index + 1]
            for index, argument in enumerate(arguments[:-1])
            if argument == '-xrm'
        ]
        expected = None if dpi is None else f'Xft.dpi: {dpi}'
        self.assertEqual(
            [resource for resource in resources if resource.startswith('Xft.dpi:')],
            [] if expected is None else [expected])
        return arguments

    # Verify native-DPI arguments are forwarded without font resources.
    def test_x3270a_native_dpi(self):
        result = self.run_x3270a(dpi=96, arguments=['-model', '4'])

        self.assertEqual(result.returncode, 0)
        self.assertEqual(self.assert_xft_dpi(result, None), ['-model', '4'])

    # Verify explicit DPI adds scaled fonts and the Xft resource.
    def test_x3270a_explicit_dpi(self):
        result = self.run_x3270a(dpi=144, arguments=['-utf8'])
        self.assertEqual(result.returncode, 0)
        arguments = self.assert_xft_dpi(result, 144)
        self.assertEqual(arguments[-3:], ['-efont', '3270gt16', '-utf8'])
        self.assertIn(
            'x3270*keyPad*large*font: -misc-fixed-medium-r-normal--13-*-*-*-*-*-iso8859-1',
            arguments)

    # Verify each scaled font threshold selects the expected emulator font.
    def test_x3270a_font_choices(self):
        for dpi, efont in [(72, '3270'), (160, '3270-20'),
                           (192, '3270gt24'), (240, '3270gt32')]:
            with self.subTest(dpi=dpi):
                result = self.run_x3270a(dpi=dpi)

                self.assertEqual(result.returncode, 0)
                self.assertEqual(self.assert_xft_dpi(result, dpi)[-1], efont)

    # Verify debug mode reports the calculated scale and font.
    def test_x3270a_debug(self):
        result = self.run_x3270a(dpi=72, arguments=['-d'])

        self.assertEqual(result.returncode, 0)
        self.assert_xft_dpi(result, 72)
        self.assertIn('display is 72dpi, scale is 75\n', result.stdout)
        self.assertIn('efont is 3270\n', result.stdout)

    # Verify xrdb DPI scales fonts without overriding the resource database.
    def test_x3270a_xrdb_dpi(self):
        result = self.run_x3270a(commands={'xrdb': 'Xft.dpi:\t192\n'})
        self.assertEqual(result.returncode, 0)
        arguments = self.assert_xft_dpi(result, None)
        self.assertEqual(arguments[-2:], ['-efont', '3270gt24'])

    # Verify GNOME Wayland sessions use the text scaling factor.
    def test_x3270a_wayland_gnome(self):
        result = self.run_x3270a(
            commands={'gsettings': '1.5\n'},
            environment={
                'XDG_SESSION_TYPE': 'wayland',
                'XDG_CURRENT_DESKTOP': 'GNOME',
            })
        self.assertEqual(result.returncode, 0)
        arguments = self.assert_xft_dpi(result, None)
        self.assertEqual(arguments[-1], '3270gt16')

    # Verify invalid GNOME scaling factors report a helpful error.
    def test_x3270a_invalid_gnome_scale(self):
        result = self.run_x3270a(
            commands={'gsettings': 'invalid\n'},
            environment={
                'XDG_SESSION_TYPE': 'wayland',
                'XDG_CURRENT_DESKTOP': 'GNOME',
            })

        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertEqual(result.stderr, 'Invalid GNOME display scale\n')

    # Verify non-GNOME Wayland sessions use the origin output scale.
    def test_x3270a_wayland_output(self):
        wayland_info = (
            "interface: 'wl_output', version: 4, name: 1\n"
            '\tx: 100, y: 0, scale: 3,\n'
            "interface: 'wl_output', version: 4, name: 2\n"
            '\tx: 0, y: 0, scale: 2,\n')
        result = self.run_x3270a(
            commands={'wayland-info': wayland_info},
            environment={'WAYLAND_DISPLAY': 'wayland-0'})

        self.assertEqual(result.returncode, 0)
        self.assertEqual(self.assert_xft_dpi(result, None)[-1], '3270gt24')

    # Verify non-origin Wayland output falls back to the first output.
    def test_x3270a_wayland_output_fallback(self):
        wayland_info = (
            "interface: 'wl_output', version: 4, name: 1\n"
            '\tx: 100, y: 0, scale: 2,\n')
        result = self.run_x3270a(
            commands={'wayland-info': wayland_info},
            environment={'WAYLAND_DISPLAY': 'wayland-0'})

        self.assertEqual(result.returncode, 0)
        self.assertEqual(self.assert_xft_dpi(result, None)[-1], '3270gt24')

    # Verify unavailable GNOME settings report a helpful error.
    def test_x3270a_missing_gsettings(self):
        result = self.run_x3270a(
            environment={
                'XDG_SESSION_TYPE': 'wayland',
                'XDG_CURRENT_DESKTOP': 'gnome',
            })

        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertEqual(
            result.stderr,
            'gsettings is not installed; install gsettings to determine the GNOME display scale\n')

    # Verify unavailable Wayland information reports a helpful error.
    def test_x3270a_missing_wayland_info(self):
        result = self.run_x3270a(
            environment={'XDG_SESSION_TYPE': 'wayland'})

        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertEqual(
            result.stderr,
            'wayland-info is not installed; install wayland-info to determine the Wayland display scale\n')

    # Verify a missing DPI reports an error instead of running x3270.
    def test_x3270a_unknown_dpi(self):
        result = self.run_x3270a(commands={'xrdb': 'Xft.antialias:\ttrue\n'})

        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertEqual(result.stderr, 'Display DPI is unknown\n')

if __name__ == '__main__':
    unittest.main()
