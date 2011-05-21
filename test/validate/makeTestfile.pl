#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# The makeTestfile.pl script generates a file $target.t which is needed
# because some versions of the Perl test harness can only run test scripts
# that are actually written in Perl.  The script we generate execs the
# real test program which must be in the same directory as the .t file.

# Usage: makeTestfile.pl target.t executable
#     target.t is the name of the Perl script to generate
#     executable is the name of the file the script runs

use strict;

my ($target, $stem, $exe, $ioc) = @ARGV;

my $db = "../$stem.db";

open(my $OUT, '>', $target) or die "Can't create $target: $!\n";

if ($ioc eq "ioc") {
  print $OUT <<EOF;
exec '(softIoc -S -d $db &); ./$exe -S; killall -9 softIoc' or die 'exec failed';
EOF
} elsif (-r "$db") {
  print $OUT <<EOF;
exec './$exe -S -d $db' or die 'exec failed';
EOF
} else {
  print $OUT <<EOF;
exec './$exe -S' or die 'exec failed';
EOF
}

close $OUT or die "Can't close $target: $!\n";
