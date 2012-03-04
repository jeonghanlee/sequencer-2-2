print "testHarness\n";
foreach my $x (@ARGV) {
  print "# ...with lowered priority\n";
  print "run_seq_test \&${x}Test, -1\n";
  print "# ...with normal priority\n";
  print "run_seq_test \&${x}Test\n";
  print "# ...with raised priority\n";
  print "run_seq_test \&${x}Test, 1\n";
}
print "epicsExit\n";
