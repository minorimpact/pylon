#!/usr/bin/perl

srand(1);
my $PYLON_HOME = $ENV{PYLON_HOME};
die "\$PYLON_HOME is not set\n" unless ($PYLON_HOME);
die "PYLON_HOME='$PYLON_HOME': invalid directory\n" unless (-d $PYLON_HOME);

use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);
use Data::Dumper;
require "$PYLON_HOME/lib/pylon.pl";

my $MAX_SERVERS = 1000;
my $MAX_CHECKS = 10;
my $PID = $$;
my $verbose = 0;

main();

sub main {
    my $hostname = `/bin/hostname -s`;
    chomp($hostname);
    my $size = 575;
    my $step = 10;
    my $result;
    print "deleting all existing data\n" if ($verbose);
    pylon('reset');

    print "waiting for the next graph window\n" if ($verbose);
    waitForIt($step);
    my $now = time();
    foreach my $server_num (1 .. $MAX_SERVERS) {
        foreach my $check_num (1 .. $MAX_CHECKS) {
            my @data = ();
            foreach my $pos (1..$size) {
                my $rand = rand(10000);
                push (@data, $rand);
            }
            $start_time = time() - ($now % $step) - ($size * $step);
            my $load_string = "load|check-$check_num|$hostname-$server_num-$PID|$start_time|$size|$step|" . join("|", @data);
            $result = pylon($load_string);
            print "loaded $hostname-$server_num-$PID.check-$check_num:$result" if ($verbose);
        }
        last if ((time() % $step) == 0 && time() > $now);
    }
    while(1) {
        waitForIt($step);
        my $now = time();
        foreach my $server_num (1 .. $MAX_SERVERS) {
            foreach my $check_num (1 .. $MAX_CHECKS) {
                my $rand = rand(10000);
                my $add_string = "add|check-$check_num|$hostname-$server_num|$rand";
                $result = pylon($add_string);
                print "added $hostname-$server_num-$PID.check-$check_num = $rand:$result" if ($verbose);
            }
            last if ((time() % $step) == 0 && time() > $now);
        }
    }
}
sub waitForIt {
    my $step = shift || return;
    my $last = time();
    while ((time() % $step) > 0) {
        if (time() != $last) {
            print localtime(time())  . "\n" if ($verbose);
            $last = time();
        }
        usleep(10000);
    }
}
