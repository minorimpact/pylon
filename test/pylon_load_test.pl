#!/usr/bin/perl

use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);
use Data::Dumper;
use Getopt::Long;
use Cwd 'abs_path';

$| = 1;

my $abs_path = abs_path($0);
$abs_path =~/^(.*)\/test\/[^\/]+$/;
my $PYLON_HOME = $1;

my $MAX_SERVERS = 1000;
my $MAX_CHECKS = 10;
my $MAX_PROCESSES = 5;
my $PID = $$;

require "$PYLON_HOME/lib/pylon.pl";

my $options = {};
my $rc = GetOptions($options, qw/force help verbose debug/);

if (! $rc || $options->{help}) {
    print "Usage: $0 <options/values>\n";
    print "  --force      - force\n";
    print "  --help       - this screen\n";
    print "  --verbose    - turn on debugging\n";
    print "  --debug      - Turn debugging output ON.\n";
    return;
    exit;
}

our $debug = $options->{debug} || 0;
our $verbose = $options->{verbose} || $debug || 0;

main();

sub main {
    my $hostname = `/bin/hostname -s`;
    chomp($hostname);
    my $size = 575;
    my $step = 10;
    my $result;

    unless ($options->{force}) {
        print "Running this script will remove all data from pylon. Continue? ";
        my $confirm = <STDIN>;
        die unless ($confirm =~/^y/i);
    }
    pylon("reset");
    
    for (my $i=0;$i<$MAX_PROCESSES;$i++) {
        my $pid = fork();
        unless ($pid) {
            while(1) {
                my $now = time();
                foreach my $server_num (1 .. $MAX_SERVERS) {
                    foreach my $check_num (1 .. $MAX_CHECKS) {
                        my $rand = rand(10000);
                        my $add_string = "add|check-$check_num|$hostname-$server_num-$$|$rand";
                        $result = pylon($add_string);
                    }
                    last if (((time() % $step) == 0 && time() > $now) || !ps());
                }
            }
            exit;
        }
    }
    while (1) {
        #servers=4516 checks=45067 size=830931114 uptime=723 connections=175160 commands=175160 adds=175129 gets=0 dumps=0
        my $status = pylon("status");
        chomp($status);
        print "$status\r";
        ($servers, $checks) = ($status =~/servers=(\d+) checks=(\d+)/);
        print "servers:$servers, checks:$checks\n";
        sleep(5);
        for (my $i=0; $i<length($status); $i++) { print " ";  }
        print "\r";
    }
    while (wait() > 0) {}
}

sub ps {
    my $ps = `/bin/ps -p $PID -o comm=`;
    return ($ps?"1":"0");
}
