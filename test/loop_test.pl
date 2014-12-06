#!/usr/bin/perl

my $PYLON_HOME = $ENV{PYLON_HOME};
die "\$PYLON_HOME is not set\n" unless ($PYLON_HOME);
die "PYLON_HOME='$PYLON_HOME': invalid directory\n" unless (-d $PYLON_HOME);


use Socket;
use Time::HiRes qw(gettimeofday tv_interval);
use Data::Dumper;
require "$PYLON_HOME/lib/pylon.pl";

my $MAX_SERVER_COUNT = 12500;

$| = 1;

main();

sub main {
    my $hostname = `/bin/hostname -s`;
    while(1) {
        my $now = time();
        my $uptime = `/usr/bin/uptime`;
        $uptime =~/load average: ([0-9\.]+),/;
        my $cpu = $1;

        print pylon("add|cpu|$hostname|$cpu");
        print pylon("add|time|$hostname|$now|counter");

        foreach my $server (1..100) {
            foreach my $check (1..10) {
                my $value = int(rand(1000)) + 1;
                print "add|check_$check|server_$server|$value:" . pylon("add|check_$check|server_$server|$value");
            }
        }
        sleep(300);
    }
}
