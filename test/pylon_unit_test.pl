#!/usr/bin/perl

use Socket;
use Time::HiRes qw(gettimeofday tv_interval);
use Data::Dumper;
use Cwd 'abs_path';

my $abs_path = abs_path($0);
$abs_path =~/^(.*)\/test\/[^\/]+$/;
my $PYLON_HOME = $1;

require "$PYLON_HOME/lib/pylon.pl";

$| = 1;

main();

sub main {
    my $get;
    my $result;

    print "resetting server\n";
    $result = pylon("reset");
    print $result;
    unless ($result =~/OK/) { print "FAIL\n"; stop(); } 

    print "checking status\n";
    $result = pylon("status");
    print $result;
    unless ($result =~/servers=0/) { print "FAIL\n"; stop(); } 

    my $size = 4; # total number of points
    my $step = 5; # seconds between each data point

    print "waiting for time window";
    while (time() % $step) {
        print ".";
        sleep(1);
    }
    print "\n";

    my $now = time();
    my $start_time = $now - ($now % $step) - (($size ) * $step); #the time of the first point of data
    my @test = ();
    foreach my $pos (1..$size) {
        push (@test, $pos*10);
    }
    my $load_string = "load|check1|server1|$start_time|$size|$step|" . join("|", @test);
    print "now=$now\n";
    print "loading test data:$load_string\n";
    $result = pylon($load_string);
    print $result;
    unless ($result =~/OK/) { print "FAIL\n"; stop(); }

    print "checking status\n";
    $result = pylon("status");
    print $result;
    if ($result =~/servers=1/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "validating check list.\n";
    $result = pylon("checks|server1");
    print $result;
    if ($result eq "check1\n") { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|$test[0]\.0+/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "waiting for time window";
    while ((time() % $step) || time() <= $start_time) {
        print ".";
        sleep(1);
    }
    print "\n";
    $start_time += $step;

    my $add_value = 55;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "waiting for time window";
    foreach (1..($step - 1)) {
        print ".";
        sleep(1);
    }
    print "\n";
    $start_time += $step;

    my $add_value = 65;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    print $result;
    unless ($result =~/OK/) { print "FAIL\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    my $load_string = "load|$result";
    $load_string =~s/server1/server2/;
    print "loading test data for second server\n";
    $result = pylon($load_string);
    print $result;
    unless ($result =~/OK/) { print "FAIL\n"; } 

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server2");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "checking status\n";
    $result = pylon("status");
    print $result;
    if ($result =~/servers=2/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    print "getting data for multiple servers\n";
    $result = pylon("get|check1|" . ($start_time - $step) . "|server1|server2");
    print $result;
    if ($result =~/\|$size\|$step\|60\.0+\|80\.0+\|110\.0+\|130\.0+/) { print "OK\n"; }
    else { print "FAIL\n"; stop(); }

    print "getting average data for multiple servers\n";
    $result = pylon("avg|check1|" . ($start_time - $step) . "|server1|server2");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { print "FAIL\n"; stop(); }

    stop();
}

sub stop {
    exit;
}

sub printstatus {
    $status = pylon("status");
    $status =~/servers=(\d+)/;
    my $servers = $1;
    $status =~/checks=(\d+)/;
    my $checks = $1;
    $status =~/size=(\d+)/;
    my $size = $1;
    print "servers=$servers checks=$checks size=${\ FFI::CommonLib::add_commas($size); }\n";
}
