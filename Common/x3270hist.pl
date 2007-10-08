#!/usr/bin/perl
#
# Copyright 2005 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.
#
# x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
#
# x3270hist.pl
#    An x3270 history plugin.
#
# Uses the x3270 plugin protocol to maintain command history.
# Persistent history for a given <host> is kept in the file ~/.x3hist.<host>.
# Each line of the history file contains a context prefix and text.  Currently
# supported prefixes are 'cms' (VM/CMS Ready prompt), 'cp' (VM CP mode) and
# 'tso' (TSO READY prompt).  Other modes, such as support for application
# command history, may be added in the future.
#
# The x3270 plugin protocol starts the plugin process as a child of the
# emulator process, with the plugin process's stdin and stdout connected
# back to the emulator.
#
# The plugin is initially sent a string in the form:
#   x3270 host <hostname>
# If the plugin initializes successfully, it answers with the line "ok".
# Otherwise it answers with an error message and exits.
#
# Just before the emulator sends any AID to the host, the emulator sends the
# following text to the plugin:
#   aid <aid-name>
#   rows <rows> cols <cols> cursor <cursor-pos>
#   <screen dump in ReadBuffer() format>
# The plugin responds with a line of text, which is ignored (but logged) by
# the emulator.
#
# When the emulator user runs the Plugin(command,xxx) action, the emulator
# sends the following text to the plugin:
#   command <xxx>
#   rows <rows> cols <cols> cursor <cursor-pos>
#   <screen dump in ReadBuffer() format>
# By convention, the values for <xxx> are 'prev' and 'next', meaning that the
# user wants the previous or next saved command respectively.  Other commands
# may be added in the future.
# The plugin responds to the command with a line of text, which will be
# interpreted as an x3270 macro, and thus can contain actions like
# MoveCursor(), String(), etc.  If the plugin is unable to process the command,
# it responds with the Bell() action (to ring the terminal bell, if the problem
# is minor such as running out of stored commands), or with the Info() action
# (to pop up a dialog box to describe a more serious problem).
#
# Usage:
#   x3270hist.pl [-d]
#    The -d option causes debug information to be written to the file
#     /tmp/hist<pid>.
#

use strict;

my $histmax = 100;	# maximum history

# Hashes that hold stored commands.  The hash key is the host type or mode
# (cms, cp or tso)
my %hist;
my %ix;		# last returned index
my %direction;	# history direction

if ($ARGV[0] eq "-d") {
	# Open the debug trace, unbuffered.
	open DEBUG, ">/tmp/hist" . $$;
	select DEBUG;
	$| = 1;
	select STDOUT;
}

# Make stdout unbuffered.
$| = 1;

# Read the initial status and say hello.
my ($emu, $dummy, $host) = split /\s+/, <STDIN>;
if (!defined($host)) {
	die "Bad init string\n";
}
print "ok\n";
print DEBUG "emulator $emu host $host\n";

# See if there's a history file for this host.
if (defined($ENV{'HOME'}) && -d $ENV{'HOME'}) {
	my $histdir = "$ENV{'HOME'}/.x3hist";
	if (-d $histdir || mkdir $histdir) {
		if (open HFILE, "<$histdir/$host") {
			my $line;
			my $mode;
			my $remainder;

			# Read in the history file, keeping only the last
			# $history_max entries for each mode.
			while ($line = <HFILE>) {
				chomp $line;
				($mode, $remainder) = split(/\s+/, $line, 2);
				print DEBUG "file: got $mode $remainder\n";
				push @{$hist{$mode}}, $remainder;
				$ix{$mode} = scalar(@{$hist{$mode}}) - 1;
				if ($ix{$mode} >= $histmax) {
					shift @{$hist{$mode}};
					$ix{$mode}--;
				}
				$direction{$mode} = -1;
			}

			# Rewrite the history file.
			close HFILE;
			open HFILE, ">$histdir/$host";
			foreach $mode (keys(%hist)) {
				for (@{$hist{$mode}}) {
					print HFILE "$mode $_\n";
				}
			}
			close HFILE;
		}

		# Get ready to append to it.
		open HFILE, ">>$histdir/$host";
		select HFILE;
		$| = 1;
		select STDOUT;
	}
}

# AID digester.
# Returns the row on the screen where the input field is, -1 if it can't
# recognize one, and 0 if it was given an AID to process and there's nothing
# to save.  Also returns the mode (cms, cp, tso).
sub digest {
	my $aid = shift;
	my $i;
	my @screen;
	my @sf;
	my $nsf = 0;
	my $cmd_row = -1;
	my $mode;

	my ($dummy, $rows, $dummy, $cols, $dummy, $cursor) = split /\s+/, <STDIN>;

	print DEBUG "AID $aid rows $rows cols $cols cursor $cursor\n";

	# Read, or skip, the screen data.
	#  @screen contains the ASCII text
	#  @sf contains the SF specifiers, with '*' indicating normal
	#                                       'z' indicating invisible, and
	#                                       '+' indicating highlighted
	foreach $i (0..$rows-1) {
		my $row = <STDIN>;
		#print DEBUG "got [$i] $row";

		# If it isn't an Enter or a command, we don't care.
		if ($aid ne "Enter" && $aid ne "command") {
			#print DEBUG "ignoring\n";
			next;
		}

		my $c;
		$screen[$i] = "";
		$sf[$i] = "";
		foreach $c (split /\s+/, $row) {
			# Treat SFs as spaces.
			if ($c =~ /SF/) {
				$screen[$i] .= " ";
				if ($c =~ /c0=(..)/ &&
					((hex("0x" . $1) & 0x0c) == 0x0c)) {
					#print DEBUG "invisible!\n";
					$sf[$i] .= "z";
				} elsif ($c =~ /c0=(..)/ &&
					((hex("0x" . $1) & 0x0c) == 0x08)) {
					#print DEBUG "highlighted!\n";
					$sf[$i] .= "+";
				} else {
					$sf[$i] .= "*";
				}
				$nsf++;
				next;
			}
			# Ignore SAs.
			if ($c =~ /SA/) {
				next;
			}
			# Translate NULLs to spaces (for now).
			if (hex("0x" . $c) < ord(" ")) {
				$c = "20";
			}

			# Accumulate the result.
			$screen[$i] .= chr(hex("0x" . $c));
			$sf[$i] .= " ";
		}
		#print DEBUG "done: ", $screen[$i] . "\n";
		#print DEBUG "done: ", $sf[$i] . "\n";
	}
	if ($aid ne "Enter" && $aid ne "command") {
		#print DEBUG "AID not Enter or command\n";
		return 0;
	}

	# The CMS input area is an unprotected field on the last two rows of
	# the screen, taking up all of the second-last row and all but the last
	# 21 positions of the last row.
	#
	# The TSO input area is either a cleared screen with an SF at the end
	# of the last row, or a highlighted 'READY ' prompt with an
	# unhighlighted empty field filling the rest of the screen.
	# If there is any highlighted text below the 'READY ', then there is a
	# subcommand active.

	if (substr($sf[$rows - 3], -1, 1) eq "*" &&
		substr($sf[$rows - 1], -21, 1) eq "*" &&
		$cursor >= ($cols * ($rows - 2)) &&
		$cursor < ($cols * $rows) - 21) {

		# CMS or CP command prompt.
		$cmd_row = $rows - 2;
		if (substr($screen[$rows - 1], -20, 7) eq "CP READ") {
			$mode = "cp";
		} else {
			$mode = "cms";
		}
	} elsif ($nsf == 1 && substr($sf[$rows - 1], $cols - 1, 1) eq "*") {
		# TSO cleared screen.
		$cmd_row = 0;
		$mode = "tso";
	} else {
		for ($i = $rows - 1; $i >= 0; $i--) { 
			if (($sf[$i] =~ /\+/) &&
			    substr($screen[$i], 1, 6) ne "READY ") {

			    # TSO Subcommand mode.
			    print DEBUG "TSO subcommand mode?\n";
			    last;
			}
			if ((substr($sf[$i], 0, 8) eq "+      *") &&
			    (substr($screen[$i], 1, 6) eq "READY ") &&
			    (substr($screen[$i], 8) =~ /^\s+$/) &&
			    ($cursor >= ($i + 1) * $cols)) {

				# TSO 'READY' prompt.
				$cmd_row = $i + 1;
				$mode = "tso";
				last;
			}
		}
	}
	if ($cmd_row == -1) {
		#print DEBUG "AID done not found\n";
		return -1;
	}

	if ($aid eq "Enter") {
		my $cmd = "";

		if ($mode eq "tso") {
			 my $j;

			 for ($j = $cmd_row; $j < $rows; $j++) {
				 $cmd .= $screen[$j];
			 }
		} else {
			$cmd = $screen[$rows-2] . substr($screen[$rows-1], 1, $cols-21);
		}
		while ($cmd =~ /^.*\s+$/) {
			chop $cmd;
		}
		while ($cmd =~ /^\s+.*$/) {
			$cmd = substr($cmd, 1);
		}
		print DEBUG "mode='$mode' cmd='$cmd'\n";
		if ($cmd ne "") {
			if (defined($hist{$mode})) {
				if ((@{$hist{$mode}})[-1] eq $cmd) {
					print DEBUG "duplicate\n";
					return -1;
				}
			}
			push @{$hist{$mode}}, $cmd;
			$ix{$mode} = scalar(@{$hist{$mode}}) - 1;
			if ($ix{$mode} >= $histmax) {
				shift @{@hist{$mode}};
				$ix{$mode}--;
			}
			$direction{$mode} = -1;
			print HFILE "$mode $cmd\n";
		} else {
			$cmd_row = -1;
		}
	}
	#print DEBUG "AID done row $cmd_row\n";
	return ($cmd_row, $mode);
}

# Ring the terminal bell with our response, but don't display any text.
sub bell
{
	my $msg = shift;

	# Print it to the debug file.
	print DEBUG "Bell - $msg\n";

	# Tell the emulator.
	print 'Bell("', $msg, '")', "\n";
}

# Pop up an error on the terminal.
sub info
{
	my $msg = shift;

	# Print it to the debug file.
	print DEBUG "Info - $msg\n";

	# Tell the emulator.
	print 'Info("', $msg, '")', "\n";
}

# Respond successfully to a command.
sub success
{
	my $msg = shift;

	# Print it to the debug file.
	print DEBUG "Success - $msg\n";

	# Tell the emulator.
	print "$msg\n";
}

# Quote a string for String().
sub quoteit
{
	my $s = shift;

	$s =~ s/([\\"])/\\\1/g;
	return '"' . $s . '"';
}

# Read indications from the emulator.
while (<STDIN>) {
	# Get the verb.  We can do verb-specific splitting again later.
	print DEBUG "Got $_";
	my ($verb) = split;
	if ($verb eq "aid") {
		# Split again.
		my ($dummy, $aid) = split;
		my ($okrow, $mode) = digest($aid);
		if ($okrow <= 0) {
			print "no new command\n";
		} else {
			print "saved ", $mode, " ", (@{$hist{$mode}})[-1], "\n";
		}
	} elsif ($verb eq "command") {
		my ($dummy, $what) = split;
		my ($okrow, $mode) = digest("command");
		my $jump = 0;
		if ($what eq "prev") {
			if ($okrow < 0) {
				bell("Not in an input field.");
				next;
			}
			if (!defined($hist{$mode})) {
				bell("No saved '" .  $mode . "' commands.");
				next;
			}
			if ($direction{$mode} > 0) {
				$direction{$mode} = -1;
				$jump = 2;
			} else {
				$jump = 1;
			}
			if ($ix{$mode} - ($jump - 1) < 0) {
				bell("No more '" . $mode . "' commands.");
			} else {
				success("MoveCursor($okrow,0) EraseEOF() String(" .
				    quoteit((@{$hist{$mode}})[$ix{$mode} - ($jump - 1)]) .
				    ")");
				$ix{$mode} -= $jump;
			}
			$direction{$mode} = -1;
		} elsif ($what eq "next") {
			if ($okrow < 0) {
				bell("Not in an input field.");
				next;
			}
			if (!defined($hist{$mode})) {
				bell("No saved '" . $mode . "' commands.");
				next;
			}
			if ($direction{$mode} < 0) {
				$direction{$mode} = 1;
				$jump = 2;
			} else {
				$jump = 1;
			}
			if ($ix{$mode} + $jump > scalar @{$hist{$mode}}) {
				bell("No more '" . $mode . "' commands.");
			} else {
				$ix{$mode} += $jump;
				success("MoveCursor($okrow,0) EraseEOF() String(" .
				    quoteit((@{$hist{$mode}})[$ix{$mode}]) .  ")");
				$direction{$mode} = 1;
			}
		} else {
			info("Unknown history command: '" . $what . "'");
		}
	} else {
		info("Unknown history verb '" . $verb . "'");
	}
}

print DEBUG "EOF, exiting\n";
