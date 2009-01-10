#!/usr/bin/perl
# Convert a DBCS ucm file to a translation table.

my $file = $ARGV[0];
my $in = 0;
#my %d2u;
my %u2d;
open TABLE, "<" . $file . ".ucm" or die "No table.";
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
    /<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)\\x([0-9a-fA-F]+)/;
    my $u = hex($1);
    my $d = hex($2 . $3);
    if ($d & 0xff00) {
	#$d2u{$d} = $u;
	$u2d{$u} = $d;
    }
}

# Dump Unicode-to-DBCS. */
print "/* Unicode-to-DBCS translation table for $file */\n";
print "unsigned char *u2d[] = {\n";
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
print "/* ff00 */ ";
if ($any) {
    print "\"$row\"";
} else {
    print "NULL";
}
print " };\n";
