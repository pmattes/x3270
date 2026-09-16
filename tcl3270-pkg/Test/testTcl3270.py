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
#     * Neither the name of Paul Mattes nor his contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES OR HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# tcl3270.tcl unit tests

import os
from subprocess import run, PIPE
import sys
import unittest

from Common.Test.cti import cti


@unittest.skipIf(sys.platform == "darwin", "macOS does not like tcl")
class TestTcl3270Package(cti):

    def run_tcl(self, script):
        return run(["tclsh"], input=script, text=True, stdout=PIPE,
            stderr=PIPE, cwd=os.getcwd(), check=False, timeout=10)

    def test_json_string_round_trip(self):
        script = r'''
source tcl3270-pkg/tcl3270.tcl
foreach value [list {} {a"b} {back\slash} {line
break} "\u03a9"] {
    set encoded [::tcl3270::_json_quote $value]
    set decoded [::tcl3270::_json_decode $encoded]
    if {$decoded ne $value} {
        error "round trip failed: '$value' -> '$encoded' -> '$decoded'"
    }
}
puts ok
'''
        result = self.run_tcl(script)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("ok\n", result.stdout)

    def test_json_values_and_command_encoding(self):
        script = r'''
source tcl3270-pkg/tcl3270.tcl
set value [::tcl3270::_json_decode {{"text":"hello","items":[true,false,null,42]}}]
if {[dict get $value text] ne "hello"} {error "bad object string"}
if {[lindex [dict get $value items] 0] ne "true"} {error "bad true"}
if {[lindex [dict get $value items] 1] ne "false"} {error "bad false"}
if {[lindex [dict get $value items] 2] ne ""} {error "bad null"}
if {[lindex [dict get $value items] 3] ne "42"} {error "bad number"}
if {[::tcl3270::_json_command String [list {a"b} {line
break}]] ne {"action":"String","args":["a\"b","line\nbreak"]}} {
    error "bad command encoding"
}
puts ok
'''
        result = self.run_tcl(script)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("ok\n", result.stdout)

    def test_json_decode_errors(self):
        script = r'''
source tcl3270-pkg/tcl3270.tcl
foreach value [list {} {"unterminated} {true trailing} {[1,]}] {
    if {![catch {::tcl3270::_json_decode $value} error]} {
        error "accepted invalid JSON '$value'"
    }
}
puts ok
'''
        result = self.run_tcl(script)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("ok\n", result.stdout)

    def test_actions_and_results(self):
        script = r'''
source tcl3270-pkg/tcl3270.tcl
if {![catch {Ascii} error] || $error ne "tcl3270 is not initialized"} {
    error "action before init did not fail correctly"
}
tcl3270::init
if {[lsearch -exact [info commands] Ascii] < 0} {error "Ascii was not created"}
if {[lsearch -exact [info commands] Query] < 0} {error "Query was not created"}
if {[interp alias {} Quit] ne "exit"} {error "Quit was not mapped to exit"}
if {[interp alias {} Exit] ne "exit"} {error "Exit was not mapped to exit"}
if {[Query LocalEncoding] ne "UTF-8"} {error "unexpected LocalEncoding"}
if {[Rows] ne "24" || [Cols] ne "80"} {error "unexpected screen size"}
if {![catch {Query Garbage} error]
        || $error ne "Query(): Unknown parameter 'Garbage'"} {
    error "s3270 error was not propagated"
}
if {![catch {tcl3270::init} error]
        || $error ne "tcl3270 is already initialized"} {
    error "duplicate init was not rejected"
}
tcl3270::close
if {![catch {Ascii} error] || $error ne "tcl3270 is not initialized"} {
    error "action after close did not fail correctly"
}
puts ok
'''
        result = self.run_tcl(script)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("ok\n", result.stdout)


if __name__ == '__main__':
    unittest.main()
