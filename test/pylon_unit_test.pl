#!/usr/bin/perl

use Socket;
use Data::Dumper;
use Pylon;
use Getopt::Long;

$| = 1;

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
our $debug = $options->{debug} || 0;
our $verbose = $options->{verbose} || $debug;
my $pylon = new Pylon({verbose=>$verbose, debug=>$debug});

main();

sub main {
    my $result;
    my $step = 5; # seconds between each data point
    my $size = 575; # total number of points
    my $add_value1 = 5755;
    my $add_value2 = 5765;
    my $test_pos = int(rand($size - 10)) + 10;
    my $dump_file = "/tmp/pylon_dump";

    unless ($options->{force}) {
        print "Running this script will remove all data from pylon. Continue? ";
        my $confirm = <STDIN>;
        die unless ($confirm =~/^y/i);
    }

    $pylon->stop();
    print "checking status\n";
    eval {
        $result = $pylon->command("status");
    };
    print "$result\n";
    if ($result) { die("FAIL\n"); } 

    unlink($dump_file);
    $pylon->start();
    print "checking status\n";
    $result = $pylon->command("status");
    print $result;
    unless ($result =~/servers=0/) { die("FAIL\n"); } 
    $pylon->command("loglevel|10");

    $pylon->waitForIt({step=>$step});

    my $now = time();
    $start_time{$step} = time() - ($size * $step); #the time of the first point of data
    my @test = ();
    foreach my $pos (1..$size) {
        push (@test, $pos*10);
    }
    my $load_string = "load|graph1|server1|$start_time{$step}|$size|$step|" . join("|", @test);
    print "loading test data\n";
    $result = $pylon->command($load_string);
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); }

    print "checking status\n";
    $result = $pylon->command("status");
    print $result;
    if ($result =~/servers=1/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "validating graph list.\n";
    $result = $pylon->command("graphs|server1");
    print $result if ($debug);
    if ($result eq "graph1\n") { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "dumping and validating test data\n";
    $result = $pylon->command("dump|graph1|server1");
    print $result if ($debug);
    if ($result =~/$size\|$step\|$test[0]\.0+\|.*\|$test[$size-1]\.0+/) { print "OK\n"; } 
    else { die("FAIL\n")}

    print "adding a single value: $add_value1\n";
    $result = $pylon->command("add|graph1|server1|$add_value1");
    if ($result =~/OK/) { print "OK\n"; } 
    else { die("FAIL\n")}

    print "dumping and validating test data\n";
    $result = $pylon->command("dump|graph1|server1");
    print $result if ($debug);
    if ($result =~/$size\|$step\|$test[1]\.0+\|.*\|$test[$size-1]\.0+\|$add_value1\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    $pylon->waitForIt({step=>$step});
    $start_time{$step} += $step;

    print "adding a single value: $add_value2\n";
    $result = $pylon->command("add|graph1|server1|$add_value2");
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); }

    print "dumping and validating test data\n";
    $result = $pylon->command("dump|graph1|server1");
    print $result if ($debug);
    if ($result =~/$size\|$step\|$test[2]\.0+\|.*\|$test[$size-1]\.0+\|$add_value1\.0+\|$add_value2\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    my $load_string = "load|$result";
    $load_string =~s/server1/server2/;
    print "loading test data for second server\n";
    $result = $pylon->command($load_string);
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); } 

    print "dumping and validating test data\n";
    $result = $pylon->command("dump|graph1|server2");
    print $result if ($debug);
    if ($result =~/$size\|$step\|.*\|5755\.0+\|5765\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "checking status\n";
    $result = $pylon->command("status");
    print $result;
    if ($result =~/servers=2/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "getting data for multiple servers\n";
    $result = $pylon->command("get|graph1|$start_time{$step}|server1|server2");
    print $result if ($debug);
    if ($result =~/\|$size\|$step\|.*\|11500\.0+\|11510\.0+\|11530\.0+/) { print "OK\n"; }
    else { die("FAIL\n"); }

    print "getting average data for multiple servers\n";
    $result = $pylon->command("avg|graph1|$start_time{$step}|server1|server2");
    print $result if ($debug);
    if ($result =~/$size\|$step\|.*\|5755\.0+\|5765\.0+/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "resetting server\n";
    $result = $pylon->command("reset");
    print $result;
    unless ($result =~/OK/) { die("FAIL\n"); } 

    $pylon->waitForIt({step=>$step});
    $now = time();
    my @steps = (5, 300, 1800, 7200, 86400);
    my %data;
    foreach my $step (@steps) {
        print "loading step $step\n";
        foreach my $pos (1..$size) {
            my $rand = rand(1000000000) + 1000000000;
            push (@{$data->{$step}}, $rand);
        }
        $start_time{$step} = time() - ($now % $step) - (($size) * $step);
        my $load_string = "load|graph1|server1|$start_time{$step}|$size|$step|" . join("|", @{$data->{$step}});
        $result = $pylon->command($load_string);
        print $result;
        unless ($result =~/OK/) { die("FAIL\n"); } 
    }

    foreach my $step (@steps) {
        my $get_string = "get|graph1|" . ($start_time{$step} - $step) . "|server1";
        $result = $pylon->command($get_string);
        print "$result" if ($debug);
        my @rdata = split(/\|/,$result);
        my $rtime = shift @rdata;
        my $rsize = shift @rdata;
        my $rstep = shift @rdata;
        print "count=" . scalar(@rdata) . ",original_data=" . @{$data->{$step}}[$test_pos-1] . ",new_data=" . $rdata[$test_pos-1] . "\n" if ($verbose);
        print "validating count ($step)\n";
        if (scalar(@rdata) == $size) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
        print "validation data ($step)\n";
        if ($rdata[$test_pos-1] <  (@{$data->{$step}}[$test_pos-1] + 1) && $rdata[$test_pos-1] >  (@{$data->{$step}}[$test_pos-1]-1)) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
    }

    my $options = $pylon->options();
    print "waiting for disk dump:\n";
    for (my $i=$options->{dump_interval} + 5; $i>0;$i--) {
        print " " if ($i < 10);
        print "$i\r";
        sleep(1);
    }
    print "\n";

    $pylon->stop();
    $pylon->start();
    $pylon->waitForIt({step=>$step});
    $now = time();
    foreach my $step (@steps) {
        my $start_time = time() - ($now % $step) - ($size * $step) - $step;
        my $get_string = "get|graph1|$start_time|server1";
        $result = $pylon->command($get_string);
        print "$result" if ($debug);
        chomp($result);
        my @rdata = split(/\|/,$result);
        my $rtime = shift @rdata;
        my $rsize = shift @rdata;
        my $rstep = shift @rdata;
        my $new_pos = $test_pos - int(($start_time - $start_time{$step}) / $step) - 1;
        for (my $i=1;$i<=$size;$i++) {
            print "$i,@{$data->{$step}}[$i-1],$rdata[$i-1]\n" if ($debug);
        }
        print "$test_pos/$new_pos,count=" . scalar(@rdata) . ",original_data=" . @{$data->{$step}}[$test_pos-1] . ",new_data=" . $rdata[$new_pos-1] . "\n" if ($verbose);
        print "validating reloaded count ($step)\n";
        if (scalar(@rdata) == $size) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
        print "validatiing reloaded data ($step)\n";
        if ($rdata[$new_pos-1] <  (@{$data->{$step}}[$test_pos-1] + 1) && $rdata[$new_pos-1] >  (@{$data->{$step}}[$test_pos-1]-1)) {
            print "OK\n";
        } else {
            die("FAIL\n");
        }
    }

    my $cleanup_time = 10;
    print "reseting server\n";
    $pylon->command("reset");
    print "setting cleanup time to $cleanup_time seconds\n";
    $pylon->command("loglevel|10");
    $pylon->command("cleanup|$cleanup_time");
    $options = $pylon->options();
    die("FAIL\n") unless ($cleanup_time == $options->{cleanup});
    print "OK\n";

    print "Adding test server data\n";
    $pylon->command("add|graph1|server1|$cleanup_time");
    print "checking status\n";
    $result = $pylon->command("status");
    print $result;
    if ($result =~/servers=1/) { print "OK\n"; } 
    else { die("FAIL\n"); }

    print "waiting for timeout\n";
    for (my $i=$cleanup_time + 5; $i>0;$i--) {
        print " " if ($i < 10); print "$i\r"; sleep(1);
    }
    print "\n";

    print "forcing a cleanup\n";
    $result = $pylon->command("cleanup|0");
    print $result;
    die("FAIL\n") unless ($result eq "OK\n");
    print "checking status\n";
    $result = $pylon->command("status");
    print $result;
    if ($result =~/servers=0/) { print "OK\n"; } 
    else { die("FAIL\n"); }
}

