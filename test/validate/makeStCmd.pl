print "testHarness\n";
foreach my $x (@ARGV) {
  print "run_seq_test \&${x}Test\n"
}
print "epicsExit\n";
