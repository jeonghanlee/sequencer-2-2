# Do not run this inside the source directory!
# Instead, run make_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $success = {
  tooLong => undef,
};

my $error = {
  varinit => undef,
  varinitOptr => undef,
};

sub do_test {
  my ($test) = @_;
  $_ = `make -B -s TESTPROD=$test 2>&1`;
  # uncomment this comment to find out what went wrong:
  #diag("$test result=$?, response=$_");
}

sub check_success {
  ok($? != -1 and $? == 0 and not /error/);
}

sub check_error {
  my $ne = 0;
  ok($? != -1 and $? != 0);
}

plan tests => keys(%$success) + keys(%$error);

my @alltests = (
  [\&check_success, $success],
  [\&check_error, $error],
);

foreach my $group (@alltests) {
  my ($check,$tests) = @$group;
  foreach my $test (sort(keys(%$tests))) {
    do_test($test);
    &$check();
  }
}
