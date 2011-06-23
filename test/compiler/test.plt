# Do not run this inside the source directory!
# Instead, run test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $host_arch = $ENV{EPICS_HOST_ARCH};
my $snc = "../../../bin/$host_arch/snc";

my $success = {
  sncExOpt_DuplOpt => undef,
};

my $warning = {
  sncExOpt_UnrecOpt => 1,
  syncq_no_size => 1,
};

my $error = {
  misplacedExit => 2,
  syncq_not_assigned => 1,
  syncq_not_monitored => 1,
  syncq_size_out_of_range => 1,
  varinit => [1,9],
  varinitOptr => [1,10],
  efArray => 1,
  efPointer => 1,
  efGlobal => 3,
  foreignGlobal => 3,
  pvNotAssigned => 20,
};

if ($host_arch =~ /64/) {
  $error->{tooLong} = 1;
} else {
  $success->{tooLong} = undef;
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
  my ($num_warnings) = @_;
  my $nw = 0;
  $nw++ while (/warning/g);
  #diag("num_warnings=$nw(expected:$num_warnings)\n");
  ok($? != -1 and $? == 0 and not /error/ and $nw == $num_warnings);
}

sub check_error {
  my ($num_errors) = @_;
  my $ne = 0;
  $ne++ while (/error/g);
  #diag("num_errors=$ne (expected:$num_errors)\n");
  if (ref $num_errors) {
    ok($? != -1 and $? != 0 and $ne >= $num_errors->[0] and $ne <= $num_errors->[1]);
  } else {
    ok($? != -1 and $? != 0 and $ne == $num_errors);
  }
}

plan tests => keys(%$success) + keys(%$warning) + keys(%$error);

my @alltests = (
  [\&check_success, $success],
  [\&check_warning, $warning],
  [\&check_error, $error],
);

foreach my $group (@alltests) {
  my ($check,$tests) = @$group;
  foreach my $test (sort(keys(%$tests))) {
    make($test);
    &$check($tests->{$test});
  }
}
