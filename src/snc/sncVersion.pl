#!/usr/bin/perl
#
# sncVersion - create the snc version module
#

$version = $ARGV[0];
$now = localtime;
print "/* sncVersion.c - version & date */\n";
print "/* Created by sncVersion.pl */\n";
print "static char *snc_version = \"SNC Version ${version}, compiled ${now}\";\n";
