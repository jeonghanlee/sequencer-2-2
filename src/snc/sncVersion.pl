#!/bin/bin/perl
#
# sncVersion - create the snc version module
#

$version = $ARGV[0];
$date = `date`;
chomp($date);
#($sec,$min,$hour,$mday,$mon,$year) = localtime(time);
print "/* sncVersion.c - version & date */\n";
print "/* Created by sncVersion.pl */\n";
print "char *sncVersion = \"SNC Version ${version}: ${date}\";\n";
