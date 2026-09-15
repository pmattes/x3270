#
# Exercise tcl3270.tcl against a playback s3270 session.
#

source tcl3270/tcl3270.tcl

if {$argc != 2} {
    error "Usage: $argv0 output-file host"
}

set output [lindex $argv 0]
set host [lindex $argv 1]

tcl3270::init $host
Wait InputField

set fp [open $output w+]
foreach line [Ascii] {
    puts $fp $line
}
close $fp

tcl3270::close
