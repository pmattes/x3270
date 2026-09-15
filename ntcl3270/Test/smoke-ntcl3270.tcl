#
# Exercise ntcl3270.tcl against a playback s3270 session.
#

source ntcl3270/ntcl3270.tcl

if {$argc != 2} {
    error "Usage: $argv0 output-file host"
}

set output [lindex $argv 0]
set host [lindex $argv 1]

ntcl3270::init $host
Wait InputField

set fp [open $output w+]
foreach line [Ascii] {
    puts $fp $line
}
close $fp

ntcl3270::close
