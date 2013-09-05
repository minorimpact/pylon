#!/usr/bin/perl

use Socket;
use Time::HiRes qw(gettimeofday tv_interval);
use Data::Dumper;
require "../pylon.pl";

my $MAX_SERVER_COUNT = 12500;

$| = 1;

main();

sub main {
    while(1) {
        my $now = time();
        my $uptime = `/usr/bin/uptime`;
        $uptime =~/load average: ([0-9\.]+),/;
        my $cpu = $1;

        print "CPU:$cpu:";
        print pylon("add|cpu|radon|$cpu");
        print "\n";
        print pylon("dump|cpu|radon");
        print "TIME:$now:";
        print pylon("add|time|radon|$now|counter");
        print "\n";

        print pylon("dump|time|radon");
        sleep(60);
    }
}
