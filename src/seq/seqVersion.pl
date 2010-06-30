#!/usr/bin/perl
#
# create the seq version header file
#
$version = $ARGV[0];
$now = localtime;
print "#define SEQ_VERSION \"SEQ Version $version, compiled $now\"\n";
($major,$minor,$patch) = split(/[.]/, $version);
printf "#define MAGIC %d%03d%03d\n", $major,$minor,$patch;
