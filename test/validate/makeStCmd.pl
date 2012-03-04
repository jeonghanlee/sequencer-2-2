print "testHarness\n";
foreach my $x (@ARGV) {
  print "run_seq_test \&${x}Test\n"
}
foreach my $x (@ARGV) {
  print "run_seq_test \&${x}Test, 1\n"
}
print "epicsExit\n";
