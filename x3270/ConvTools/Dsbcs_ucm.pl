#!/usr/bin/perl

my $in = 0;
my %d2u;
open TABLE, "<jisx-201.ucm" or die "No table.";
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
    /<U([0-9a-fA-F]+)> +\\x([0-9a-fA-F]+)/;
    my $u = hex($1);
    my $d = hex($2);
    $d2u{$d} = $u;
}

my $i;
foreach $i (0..256) {
    if (defined($d2u{$i})) {
	printf "0x%08x,", $d2u{$i};
    } else {
	print "0x00000000,";
    }
}
print "\n";
