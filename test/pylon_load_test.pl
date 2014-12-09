#!/usr/bin/perl

use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);
use Data::Dumper;
use Getopt::Long;
use Cwd 'abs_path';

my $abs_path = abs_path($0);
$abs_path =~/^(.*)\/test\/[^\/]+$/;
my $PYLON_HOME = $1;

my $MAX_SERVERS = 1000;
my $MAX_CHECKS = 10;
my $PID = $$;

require "$PYLON_HOME/lib/pylon.pl";

my $options = {};
my $rc = GetOptions($options, qw/help verbose debug/);

if (! $rc || $options->{help}) {
    print "Usage: $0 <options/values>\n";
    print "  --help       - this screen\n";
    print "  --verbose    - turn on debugging\n";
    print "  --debug      - Turn debugging output ON.\n";
    return;
    exit;
}

my $debug = $options->{debug} || 0;
my $verbose = $options->{verbose} || $debug || 0;

main();

sub main {
    srand(1);
    my $hostname = `/bin/hostname -s`;
    chomp($hostname);
    my $size = 575;
    my $step = 10;
    my $result;

    print "waiting for the next graph window\n" if ($verbose);
    while(1) {
        waitForIt({step=>$step, verbose=>$verbose});
        my $now = time();
        foreach my $server_num (1 .. $MAX_SERVERS) {
            foreach my $check_num (1 .. $MAX_CHECKS) {
                my $rand = rand(10000);
                my $add_string = "add|check-$check_num|$hostname-$server_num|$rand";
                $result = pylon($add_string);
                print localtime() . " added $hostname-$server_num-$PID.check-$check_num = $rand:$result" if ($verbose);
                last if ((time() % $step) == 0 && time() > $now);
            }
            last if ((time() % $step) == 0 && time() > $now);
        }
    }
}
