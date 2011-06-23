# Do not run this inside the source directory!
# Instead, run test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $host_arch = $ENV{EPICS_HOST_ARCH};
my $snc = "../../../bin/$host_arch/snc";

my @success = qw(
  sncExOpt_DuplOpt
);

my @warning = qw(
  sncExOpt_UnrecOpt
  syncq_no_size
);

my @error = qw(
  misplacedExit
  syncq_not_assigned
  syncq_not_monitored
  syncq_size_out_of_range
  varinit
  varinitOptr
  efArray
  efPointer
  efGlobal
  foreignGlobal
  pvNotAssigned
);

if ($host_arch =~ /64/) {
  push(@error,"tooLong");
} else {
  push(@success,"tooLong");
}

sub make {
  my ($test) = @_;
  $_ = `make -B -s TESTPROD=$test 2>&1`;
  # uncomment this comment to find out what went wrong:
  #diag("$test result=$?, response=$_");
}

sub check_success {
  ok($? != -1 and $? == 0 and not /error/ and not /warning/);
}

sub check_warning {
  ok($? != -1 and $? == 0 and not /error/ and /warning/);
}

sub check_error {
  ok($? != -1 and $? != 0 and /error/);
}

my @alltests = (
  [\&check_success, \@success],
  [\&check_warning, \@warning],
  [\&check_error, \@error],
);

plan tests => @success + @warning + @error;

foreach my $group (@alltests) {
  my ($check, $tests) = @$group;
  foreach my $test (@$tests) {
    make($test);
    &$check($test);
  }
}
