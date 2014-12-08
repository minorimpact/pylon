#!/usr/bin/perl

use Cwd 'abs_path';

my $abs_path = abs_path($0);
$abs_path =~/^(.*)\/[^\/]+$/;
my $PYLON_HOME = $1;

require "$PYLON_HOME/lib/pylon.pl";

print pylon("status");

