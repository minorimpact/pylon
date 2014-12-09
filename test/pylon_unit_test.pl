#!/usr/bin/perl

use Socket;
use Data::Dumper;
use Cwd 'abs_path';
use Getopt::Long;

$| = 1;

my $abs_path = abs_path($0);
$abs_path =~/^(.*)\/test\/[^\/]+$/;
my $PYLON_HOME = $1;

require "$PYLON_HOME/lib/pylon.pl";

my $options = {};
my $rc = GetOptions($options, qw/debug force help verbose/);

if (! $rc || $options->{help}) {
    print "Usage: $0 <options/values>\n";
    print "  --debug      - debug\n";
    print "  --force      - bypass confirmation input\n";
    print "  --help       - this screen\n";
    print "  --verbose    - turn on verbose mode\n";
    exit;
}
my $debug = $options->{debug} || 0;
my $verbose = $options->{verbose} || $debug;

main();

sub main {
    my $result;

    unless ($options->{force}) {
        print "Running this script will remove all data from pylon. Continue? ";
        my $confirm = <STDIN>;
        die unless ($confirm =~/^y/i);
    }
    print "resetting server\n";
    $result = pylon("reset");
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); } 

    print "checking status\n";
    $result = pylon("status");
    print $result;
    unless ($result =~/servers=0/) { die("FAIL\n"); } 

    my $size = 4; # total number of points
    my $step = 5; # seconds between each data point

    waitForIt({step=>$step, verbose=>$verbose});

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
    unless ($result =~/OK/) { die("FAIL\n"); }

    print "checking status\n";
    $result = pylon("status");
    print $result;
    if ($result =~/servers=1/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "validating check list.\n";
    $result = pylon("checks|server1");
    print $result;
    if ($result eq "check1\n") { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|$test[0]\.0+/) { print "OK\n"; } 
    else { die("FAIL\n")}

    $start_time += $step;

    my $add_value = 55;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    if ($result =~/OK/) { print "OK\n"; } 
    else { die("FAIL\n")}

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    waitForIt({step=>$step, verbose=>$verbose});
    $start_time += $step;

    my $add_value = 65;
    print "adding a single value: $add_value\n";
    $result = pylon("add|check1|server1|$add_value");
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); }

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server1");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    my $load_string = "load|$result";
    $load_string =~s/server1/server2/;
    print "loading test data for second server\n";
    $result = pylon($load_string);
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); } 

    print "dumping and validating test data\n";
    $result = pylon("dump|check1|server2");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "checking status\n";
    $result = pylon("status");
    print $result;
    if ($result =~/servers=2/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "getting data for multiple servers\n";
    $result = pylon("get|check1|" . ($start_time - $step) . "|server1|server2");
    print $result;
    if ($result =~/\|$size\|$step\|60\.0+\|80\.0+\|110\.0+\|130\.0+/) { print "OK\n"; }
    else { die("FAIL\n"); }

    print "getting average data for multiple servers\n";
    $result = pylon("avg|check1|" . ($start_time - $step) . "|server1|server2");
    print $result;
    if ($result =~/$size\|$step\|.*\|40\.0+\|55\.0+\|65\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "resetting server\n";
    $result = pylon("reset");
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); } 

    waitForIt({step=>5, verbose=>$verbose});
    my $size = 575;
    my @steps = (5, 300, 1800, 7200, 86400);
    my %data;
    foreach my $step (@steps) {
        print "loading step $step\n";
        foreach my $pos (1..$size) {
            my $rand = rand(1000000);
            push (@{$data->{$step}}, $rand);
        }
        my $start_time = time() - ($now % $step) - ($size * $step);
        my $load_string = "load|check1|server1|$start_time|$size|$step|" . join("|", @{$data->{$step}});
        $result = pylon($load_string);
        print $result;
        unless ($result =~/OK/) { die("FAIL\n"); } 
    }

    foreach my $step (@steps) {
        my $start_time = time() - ($now % $step) - ($size * $step) - $step;
        my $get_string = "get|check1|$start_time|server1";
        $result = pylon($get_string);
        my @rdata = split(/\|/,$result);
        my $rtime = shift @rdata;
        my $rsize = shift @rdata;
        my $rstep = shift @rdata;
        print "count=" . scalar(@rdata) . ",original_data=" . @{$data->{$step}}[50] . ",new_data=" . $rdata[50] . "\n" if ($debug);
        print "validating count ($step)\n";
        if (scalar(@rdata) == 575) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
        print "validation data ($step)\n";
        if ($rdata[50] <  (@{$data->{$step}}[50] + 1) && $rdata[50] >  (@{$data->{$step}}[50]-1)) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
    }
}

sub start {
    `service pylon start`;
}
sub stop {
    `service pylon stop`;
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
