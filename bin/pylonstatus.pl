#!/usr/bin/perl

use Pylon;

my $pylon = new Pylon;
print $pylon->command("status") . "\n";

