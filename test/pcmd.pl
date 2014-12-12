#!/usr/bin/perl

use FindBin;
use lib "$FindBin::Bin/../lib";
use Pylon;

my $pylon = new Pylon;
print $pylon->command($ARGV[0]);

