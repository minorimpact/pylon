#!/usr/bin/perl

use FindBin;
use lib "$FindBin::Bin/../lib";
use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);
use Data::Dumper;
use Getopt::Long;
use Pylon;

$| = 1;


my $MAX_SERVERS = 100; # per process
my $MAX_CHECKS = 25;
my $PID = $$;
my $slope = 1;
my $rand_max = 1000;

my $options = {};
my $rc = GetOptions($options, qw/debug help host=s procs=s verbose/);

if (! $rc || $options->{help}) {
    print "Usage: $0 <options/values>\n";
    print "  --debug      Turn debugging output ON.\n";
    print "  --help       this screen\n";
    print "  --host       pylon server to connect to\n";
    print "  --procs      how many processes to fork for testing\n";
    print "  --verbose    turn on debugging\n";
    return;
    exit;
}

my $debug = $options->{debug} || 0;
my $verbose = $options->{verbose} || $debug || 0;
my $procs = $options->{procs} || 5;
my $host = $options->{host} || '';

main();

sub main {
    my $pylon = new Pylon({verbose=>$verbose, debug=>$debug, host=>$host});
    my $hostname = `/bin/hostname -s`;
    chomp($hostname);
    my $size = 575;
    my $step = 10;
    my $result;

    for (my $i=0; $i<$procs; $i++) {
        my $proc_id = $i;
        my $pid = fork();
        unless ($pid) {
            my $now = time();
            foreach my $server_num (1 .. $MAX_SERVERS) {
                foreach my $check_num (1 .. $MAX_CHECKS) {
                    my @data = ();
                    foreach my $pos (1..$size) {
                        my $rand = getRand();
                        push (@data, $rand);
                    }
                    my $start_time = time() - ($now % $step) - (($size) * $step);
                    my $load_string = "load|check-$check_num|$hostname-$proc_id-$server_num|$start_time|$size|$step|" . join("|", @data);
                    $result = $pylon->command($load_string);
                    exit unless (parentIsAlive());
                }
            }

            while(parentIsAlive()) {
                my $now = time();
                foreach my $server_num (1 .. $MAX_SERVERS) {
                    foreach my $check_num (1 .. $MAX_CHECKS) {
                        my $rand = getRand();
                        my $add_string = "add|check-$check_num|$hostname-$proc_id-$server_num|$rand";
                        $result = $pylon->command($add_string);
                        last if (((time() % $step) == 0 && time() > $now) || !parentIsAlive());
                    }
                    last if (((time() % $step) == 0 && time() > $now) || !parentIsAlive());
                }
            }
            exit;
        }
    }
    my $last_connections = 0;
    my $last_time = 0;
    my $last_output = '';
    while (1) {
        my $now = time();
        my $status = $pylon->command("status");
        my ($servers, $checks, $size, $connections) = ($status =~/servers=(\d+) checks=(\d+) size=(\d+) uptime=\d+ connections=(\d+)/);
        $output = localtime($now) . " servers: $servers checks: $checks size: $size";
        if ($last_connections && $last_time) {
            my $new_connections = $connections - $last_connections;
            my $connections_per_sec = ($new_connections/($now - $last_time));
            $output .= sprintf(" connections/sec: %.2f", $connections_per_sec);
        }
        $last_connections = $connections;
        $last_time = $now;
        $last_output = $output;
        for (my $i=0; $i<length($last_output) + 10; $i++) { print " ";  }
        print "\r";
        print "$output\r";
        sleep(2);
    }
    while (wait() > 0) {}
}

sub getRand {
    if ($rand_max >= 10000) {
        $slope = -1;
    } elsif ($rand_max <= 0) {
        $slope = 1;
    } elsif (rand(10) < 1) {
        $slope *= -1;
    }
    $rand_max += (10 * $slope);
    return rand($rand_max);
}

sub parentIsAlive {
    my $ps = `/bin/ps -p $PID -o comm=`;
    return ($ps?"1":"0");
}
