#!/usr/bin/perl

use Socket;
use Time::HiRes qw(gettimeofday tv_interval);
use Data::Dumper;
require "../pylon.pl";

my $MAX_SERVER_COUNT = 12500;

$| = 1;

main();

sub main {
    start();

    print "loading garbage data\n";
    my $result = pylon("load|check1|server1|" . (time() - 5) . "|2|5|1|2");

    print "resetting server\n";
    $result = pylon("reset");
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "checking status\n";
    $result = pylon("status");
    if ($result =~/servers=0/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

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
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "checking status\n";
    $result = pylon("status");
    if ($result =~/servers=1/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "validating check list.\n";
    $result = pylon("checks|server1");
    if ($result eq "check1\n") { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    if ($result =~/$size\|$step\|$test[0]\.0+/) { print "OK:$result"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "waiting for time window";
    while ((time() % $step) || time() <= $start_time) {
        print ".";
        sleep(1);
    }
    print "\n";

    my $add_value = 55;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+/) { print "OK:$result"; } 
    else { print "FAIL:$result"; stop(); }

    print "waiting for time window";
    foreach (1..($step - 1)) {
        print ".";
        sleep(1);
    }
    print "\n";

    my $add_value = 65;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK:$result"; } 
    else { print "FAIL:$result"; stop(); }
    stop();

    my $load_string = "load|$result";
    $load_string =~s/server1/server2/;
    print "loading test data for second server:$load_string";
    $result = pylon($load_string);
    if ($result =~/OK/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "checking status\n";
    $result = pylon("status");
    if ($result =~/servers=2/) { print "OK\n"; } 
    else { print "FAIL:$result\n"; stop(); }

    print "getting data for multiple servers\n";
    $result = pylon("get|check1|" . (time() - 300) . "|server1|server2");
    if ($result =~/\|4\|5\|60\.0+\|80\.0+\|110\.0+\|135\.0+/) { print "OK:$result"; }
    else { print "FAIL:$result\n"; stop(); }

    print "getting average data for multiple servers\n";
    $result = pylon("avg|check1|" . (time() - 300) . "|server1|server2");
    if ($result =~/\|4\|5\|30\.0+\|40\.0+\|55\.0+\|67\.5+/) { print "OK:$result"; }
    else { print "FAIL:$result\n"; stop(); }

    stop();
}

sub start {
    #print `~pgillan/pylon/init start`;
}

sub stop {
    #print `~pgillan/pylon/init stop`;
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
