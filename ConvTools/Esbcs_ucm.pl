#!/usr/bin/perl
# Grab the SBCS part of a SBCS or MBCS ucm file and output an SBCS translation
# table.

my $file = $ARGV[0];
my $in = 0;
my %d2u;
open TABLE, "<". $file . ".ucm" or die "No table.";
while (<TABLE>) {
    chomp;
    next if /^#/;
    last if /^END CHARMAP/;
    if (/^CHARMAP/) {
	$in = 1;
	next;
    }
    next if (! $in);
    # <U0020> \x20 #|0 SPACE
    if (/<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)\\x([0-9a-fA-F])+/) {
    } elsif (/<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)/) {
	my $u = hex($1);
	my $d = hex($2);
	#printf "got U+%04x -> X'%02X'\n", $u, $d;
	$d2u{$d} = $u;
    } else {
	die "Eh?";
    }
}

print "/* EBCDIC to Unicode translation table for $file */\n";
print "unsigned short e2u[] = {\n";
my $i;
foreach $i (0x41..0xfe) {
    if (!(($i - 0x41) % 8)) {
	if (($i - 0x41)) {
	    print "\n";
	}
	printf "/* %02x */ ", $i;
    }
    if (defined($d2u{$i})) {
	printf "0x%04x, ", $d2u{$i};
    } else {
	print "0x0000, ";
    }
}
print "};\n";
