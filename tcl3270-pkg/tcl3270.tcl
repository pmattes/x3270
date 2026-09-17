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

# Tcl interface to s3270.
#
# If this is installed as a package, a script should use 'package require tcl3270'.
# Otherwise it can just source this file.
# Then the script should call tcl3270::init.
# After that, it can use the dynamically-created action commands, for example:
#
#   source tcl3270.tcl
#   tcl3270::init 127.0.0.1:23
#   Wait InputField
#   String "hello"
#
# To terminate the session, the script should call tcl3270::close.

namespace eval ::tcl3270 {
    variable channel {}
    variable actions {}
    variable initialized 0
}

# Report a JSON parsing error.
proc ::tcl3270::_json_error {text position message} {
    return -code error "invalid JSON at character $position: $message"
}

# Advance a JSON parser position past whitespace.
proc ::tcl3270::_json_skip {text positionVar length} {
    upvar 1 $positionVar position
    while {$position < $length
            && [string first [string index $text $position] " \t\r\n"] >= 0} {
        incr position
    }
}

# Decode a JSON string at the current parser position.
proc ::tcl3270::_json_string {text positionVar length} {
    upvar 1 $positionVar position
    if {[string index $text $position] ne "\""} {
        return -code error "invalid JSON at character $position: expected string"
    }
    incr position
    set result {}
    while {$position < $length} {
        set character [string index $text $position]
        incr position
        if {$character eq "\""} {
            return $result
        }
        if {$character eq "\\"} {
            if {$position >= $length} {
                return -code error "invalid JSON at character $position: incomplete escape"
            }
            set escape [string index $text $position]
            incr position
            switch -- $escape {
                "\"" { append result "\""}
                "\\" { append result "\\"}
                "/"  { append result "/"}
                "b"  { append result "\b"}
                "f"  { append result "\f"}
                "n"  { append result "\n"}
                "r"  { append result "\r"}
                "t"  { append result "\t"}
                "u" {
                    if {$position + 4 > $length
                            || ![regexp -nocase {^[0-9a-f]{4}$} \
                                [string range $text $position [expr {$position + 3}]]]} {
                        return -code error "invalid JSON at character $position: bad Unicode escape"
                    }
                    scan [string range $text $position [expr {$position + 3}]] %x code
                    append result [format %c $code]
                    incr position 4
                }
                default {
                    return -code error "invalid JSON at character $position: bad escape"
                }
            }
        } elseif {[scan $character %c code] && $code < 0x20} {
            return -code error "invalid JSON at character $position: control character"
        } else {
            append result $character
        }
    }
    return -code error "invalid JSON at character $position: unterminated string"
}

# Decode one JSON value at the current parser position.
proc ::tcl3270::_json_value {text positionVar length} {
    upvar 1 $positionVar position
    ::tcl3270::_json_skip $text position $length
    if {$position >= $length} {
        return -code error "invalid JSON at character $position: expected value"
    }
    set character [string index $text $position]
    if {$character eq "\""} {
        return [::tcl3270::_json_string $text position $length]
    }
    if {$character eq "\{"} {
        incr position
        set result [dict create]
        ::tcl3270::_json_skip $text position $length
        if {$position < $length && [string index $text $position] eq "\}"} {
            incr position
            return $result
        }
        while {1} {
            ::tcl3270::_json_skip $text position $length
            set key [::tcl3270::_json_string $text position $length]
            ::tcl3270::_json_skip $text position $length
            if {$position >= $length || [string index $text $position] ne ":"} {
                return -code error "invalid JSON at character $position: expected colon"
            }
            incr position
            dict set result $key [::tcl3270::_json_value $text position $length]
            ::tcl3270::_json_skip $text position $length
            if {$position >= $length} {
                return -code error "invalid JSON at character $position: unterminated object"
            }
            set separator [string index $text $position]
            incr position
            if {$separator eq "\}"} {
                return $result
            }
            if {$separator ne ","} {
                return -code error "invalid JSON at character $position: expected comma"
            }
        }
    }
    if {$character eq "\["} {
        incr position
        set result {}
        ::tcl3270::_json_skip $text position $length
        if {$position < $length && [string index $text $position] eq "\]"} {
            incr position
            return $result
        }
        while {1} {
            lappend result [::tcl3270::_json_value $text position $length]
            ::tcl3270::_json_skip $text position $length
            if {$position >= $length} {
                return -code error "invalid JSON at character $position: unterminated array"
            }
            set separator [string index $text $position]
            incr position
            if {$separator eq "\]"} {
                return $result
            }
            if {$separator ne ","} {
                return -code error "invalid JSON at character $position: expected comma"
            }
        }
    }
    set remainder [string range $text $position end]
    foreach {literal value} {true true false false null {}} {
        if {[string first $literal $remainder] == 0} {
            incr position [string length $literal]
            return $value
        }
    }
    if {[regexp {^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?} \
            $remainder number]} {
        incr position [string length $number]
        return $number
    }
    return -code error "invalid JSON at character $position: expected value"
}

# Decode a complete JSON document into Tcl values.
proc ::tcl3270::_json_decode {text} {
    set position 0
    set length [string length $text]
    set result [::tcl3270::_json_value $text position $length]
    ::tcl3270::_json_skip $text position $length
    if {$position != $length} {
        return -code error "invalid JSON at character $position: trailing data"
    }
    return $result
}

# Encode a Tcl string as a JSON string.
proc ::tcl3270::_json_quote {input} {
    set result "\""
    foreach character [split $input ""] {
        switch -- $character {
            "\"" {append result {\"}}
            "\\" {append result {\\}}
            "\b" {append result {\b}}
            "\f" {append result {\f}}
            "\n" {append result {\n}}
            "\r" {append result {\r}}
            "\t" {append result {\t}}
            default {
                scan $character %c code
                if {$code < 0x20} {
                    append result [format "\\u%04x" $code]
                } else {
                    append result $character
                }
            }
        }
    }
    append result "\""
    return $result
}

# Encode an s3270 action and its arguments as a JSON request.
proc ::tcl3270::_json_command {action arguments} {
    set json "{\"action\":[::tcl3270::_json_quote $action],\"args\":\["
    set encoded {}
    foreach item $arguments {
        lappend encoded [::tcl3270::_json_quote $item]
    }
    append json [join $encoded ,] "]}"
    return $json
}

# Read and validate one JSON response from s3270.
proc ::tcl3270::_response {} {
    variable channel
    if {$channel eq {}} {
        return -code error "tcl3270 is not initialized"
    }
    if {[gets $channel line] < 0} {
        return -code error "s3270 closed its output"
    }
    if {[catch {::tcl3270::_json_decode $line} response]} {
        return -code error "could not decode s3270 response: $response"
    }
    if {![dict exists $response success] || ![dict exists $response status]} {
        return -code error "invalid s3270 response"
    }
    return $response
}

# Invoke one s3270 action and return its result.
proc ::tcl3270::_invoke {action args} {
    variable channel
    if {$channel eq {}} {
        return -code error "tcl3270 is not initialized"
    }
    puts $channel [::tcl3270::_json_command $action $args]
    flush $channel
    set response [::tcl3270::_response]
    if {![dict get $response success]} {
        set result {}
        if {[dict exists $response result]} {
            set result [join [dict get $response result] "\n"]
        }
        return -code error $result
    }
    if {![dict exists $response result]} {
        return {}
    }
    set result [dict get $response result]
    if {[llength $result] == 1} {
        return [lindex $result 0]
    }
    return $result
}

# Return the current s3270 status line.
proc ::tcl3270::_status {} {
    puts [set ::tcl3270::channel] "\"\""
    flush [set ::tcl3270::channel]
    return [dict get [::tcl3270::_response] status]
}

# Return the current screen row count.
proc ::tcl3270::_rows {} {
    return [lindex [split [::tcl3270::_status] " "] 6]
}

# Return the current screen column count.
proc ::tcl3270::_cols {} {
    return [lindex [split [::tcl3270::_status] " "] 7]
}

# Stop s3270 and remove the dynamically-created action commands.
proc ::tcl3270::close {} {
    variable channel
    variable actions
    if {$channel ne {}} {
        catch {close $channel}
        set channel {}
    }
    foreach action $actions {
        catch {rename ::$action {}}
    }
    set actions {}
    set ::tcl3270::initialized 0
}

# Start s3270 and create Tcl commands for its supported actions.
proc ::tcl3270::init {args} {
    variable channel
    variable actions
    variable initialized
    if {$initialized} {
        return -code error "tcl3270 is already initialized"
    }
    set command [linsert $args 0 s3270 -utf8 -alias tcl3270]
    if {[catch {open |$command r+} newChannel]} {
        return -code error "could not start s3270: $newChannel"
    }
    set channel $newChannel
    fconfigure $channel -buffering line -translation lf -encoding utf-8
    if {[catch {::tcl3270::_invoke Query [list Actions]} response]} {
        ::tcl3270::close
        return -code error "could not discover s3270 actions: $response"
    }
    set discovered {}
    foreach line $response {
        foreach {whole match} [regexp -all -inline {([^\s()]+)\([^)]*\)} $line] {
            lappend discovered $match
        }
    }
    foreach action $discovered {
        if {[string equal -nocase $action Quit]
                || [string equal -nocase $action Exit]} {
            interp alias {} ::$action {} exit
        } elseif {[lsearch -exact $actions $action] < 0
                && [catch {interp alias {} ::$action {} ::tcl3270::_invoke $action}]} {
            ::tcl3270::close
            return -code error "cannot create Tcl command for s3270 action $action"
        }
        lappend actions $action
    }
    foreach {name implementation} {Rows _rows Cols _cols Status _status} {
        interp alias {} ::$name {} ::tcl3270::$implementation
        lappend actions $name
    }
    set initialized 1
    return
}

interp alias {} ::tcl3270::initialize {} ::tcl3270::init

package provide tcl3270 1.0
