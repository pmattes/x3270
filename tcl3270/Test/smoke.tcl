Wait inputfield
set fp [open [lindex $argv 0] w+]
foreach i [Ascii] {
    puts $fp $i
}
close $fp
Wait 60 seconds
