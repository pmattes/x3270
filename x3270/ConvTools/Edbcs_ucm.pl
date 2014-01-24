#!/usr/bin/perl
# Grab the DBCS part of a DBCS or MBCS ucm file and output DBCS translation
# tables.

my $file = $ARGV[0];
my $in = 0;
my %d2u;
my %u2d;
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
    if (/<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)\\x([0-9a-fA-F]+)/) {
	my $u = hex($1);
	my $d = hex($2 . $3);
	#printf "got U+%04x -> X'%02X'\n", $u, $d;
	$d2u{$d} = $u;
	$u2d{$u} = $d;
    } elsif (/<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)/) {
    } else {
	die "Eh?";
    }
}

print "/* Unicode to EBCDIC DBCS translation table for $file */\n";
my $row = "";
my $i;
my $any = 0;
foreach $i (0..65535) {
    if ($i && !($i % 128)) {
	printf "/* %04x */ ", $i - 128;
	if ($any) {
	    print "\"$row\",\n";
	} else {
	    print "NULL,\n";
	}
	$row = "";
	$any = 0;
    }
    if ($u2d{$i}) {
	$row .= sprintf("\\x%02x\\x%02x",
	    ($u2d{$i} >> 8) & 0xff, $u2d{$i} & 0xff);
	$any = 1;
    } else {
	$row .= "\\x00\\x00";
    }
}
print "/* ff80 */ ";
if ($any) {
    print "\"$row\"";
} else {
    print "NULL";
}
print " };\n";

print "/* EBCDIC DBCS to Unicode translation table for $file */\n";
$row = "";
$any = 0;
foreach $i (0..65535) {
    if ($i && !($i % 128)) {
	printf "/* %04x */ ", $i - 128;
	if ($any) {
	    print "\"$row\",\n";
	} else {
	    print "NULL,\n";
	}
	$row = "";
	$any = 0;
    }
    if ($d2u{$i}) {
	$row .= sprintf("\\x%02x\\x%02x",
	    ($d2u{$i} >> 8) & 0xff, $d2u{$i} & 0xff);
	$any = 1;
    } else {
	$row .= "\\x00\\x00";
    }
}
print "/* ff80 */ ";
if ($any) {
    print "\"$row\"";
} else {
    print "NULL";
}
print " };\n";
