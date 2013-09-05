#!/usr/bin/perl

use lib "/site/lib";
use Socket;
use SERVERROLES::Server;
use SERVERROLES::Graph;
use FFI::Setup;
use FFI::CommonLib;
use SERVERROLES::Serverroles;
use RRDs; 
use Time::HiRes qw(gettimeofday tv_interval);
use Data::Dumper;
require "../pylon.pl";

my $MAX_SERVER_COUNT = 3000;
my $g_obj  = new SERVERROLES::Graph(1,1); #dummy object 
my $rrd_dir = "/admin/rrd";

eval { &main(); };
print "$@\n" if ($@);
exit;

sub main {
    my $ROLES   = new SERVERROLES::Serverroles;
    $SERVERROLES::DO_CACHE=1;
    my $status;
    my $start;

    $| = 1;

    my $count = 0;
    my @servers = ();
    push @servers, $ROLES->getServer('ii38-33');
    push @servers, $ROLES->getServer('ii38-34');
    push @servers, $ROLES->getServer('ii52-27');
    push @servers, $ROLES->getServers({cluster=>'dev'});
    #push @servers, sort { rand() <=> rand(); } $ROLES->getServers();
    #push @servers, $ROLES->getServers();

    foreach my $server ( @servers ) {
        $server_id = $server->id();
        
        $status = pylon("status");
        $status =~/servers=(\d+)/;
        my $server_count = $1;
        $count++;
        last if ($count > $MAX_SERVER_COUNT);

        print "\n$server_id ";
        my %valuelist = ();
        my %size = ();
        my %step = ();
        my %last = ();
        $start = [gettimeofday];
        foreach my $range (('e-48h', 'e-12d', 'e-48d', 'e-576d')) {
            my $rrds = loadAllFromRRD($server, $range);
            foreach my $graph_id (keys %$rrds) {
                #next unless ($graph_id eq 'cpu');
                my $last_time;
                foreach my $time (sort keys %{$rrds->{$graph_id}}) {
                    foreach my $sub_id (keys %{$rrds->{$graph_id}{$time}}) {
                        my $val = $rrds->{$graph_id}{$time}{$sub_id};
                        if (!defined $val) {
                            $val = 'nan';
                        }
                        $valuelist{$range}{"$graph_id,$sub_id"} .= "$val|";
                        if (!defined($first{$range}{"$graph_id,$sub_id"})) {
                            $first{$range}{"$graph_id,$sub_id"} = $time;
                        }
                        $size{$range}{"$graph_id,$sub_id"}++;
                        if ($last_time && !defined($step{$range}{"$graph_id,$sub_id"})) {
                            $step{$range}{"$graph_id,$sub_id"} = $time - $last_time;
                        }
                    }
                    $last_time = $time;
                }
            }
        }
        my $rrd_read_time = tv_interval($start);

        $start = [gettimeofday];
        foreach my $range (sort keys %valuelist) {
            foreach my $graph_id (keys %{$valuelist{$range}}) {
                my $list = $valuelist{$range}{$graph_id};
                $list =~s/\|$//;
                pylon("load|$graph_id|$server_id|$first{$range}{$graph_id}|$size{$range}{$graph_id}|$step{$range}{$graph_id}|$list");
            }
        }
        my $pylon_write_time = tv_interval($start);

        $start = [gettimeofday];
        foreach my $graph_id (keys %{$valuelist{'e-48h'}}) {
            my $output = pylon("dump|$graph_id|$server_id");
        }
        my $pylon_read_time = tv_interval($start);

        $status = pylon("status");
        $status =~/servers=(\d+)/;
        my $server_count = $1;
        $status =~/size=(\d+)/;
        my $size = $1;
        $status =~/checks=(\d+)/;
        my $checks = $1;
        last if ($count >= $MAX_SERVER_COUNT);

        print "$count $server_count $checks $size $rrd_read_time $pylon_write_time $pylon_read_time";
    }
    print "\n";
}

sub loadAllFromRRD {
    my $server = shift || return;
    my $range = shift || return;

    my $server_id = $server->id();

    my $rrds = ();
    my $graph_keys = $g_obj->graph_keys({ server_id => $server_id });
    #my $graph_keys = ['cpu'];
    #print join(",",@$graph_keys) . "\n";
    my $now = time();
    foreach my $graph_id (@$graph_keys) {
        opendir(DIR, "$rrd_dir/$graph_id");
        foreach my $file (grep { /^$server_id,$graph_id/; } readdir(DIR)) {
            my $sub_id = $graph_id;
            if ($file =~/$graph_id,(.+)\.rrd/) {
                $sub_id = $1;
            }
            my $rrd_file = "$rrd_dir/$graph_id/$file";

            my ($time,$step,$names,$data) = RRDs::fetch($rrd_file, "AVERAGE", '-s', $range);
            my $ERR=RRDs::error;
            if (!$ERR) {
                foreach my $line (@$data) {
                    $time += $step;
                    next if ($time > $now);
                    my $val = $line->[0];
                    if ($val eq '') {
                        $rrds->{$graph_id}{$time}{$sub_id} = undef;
                    } else {
                       $rrds->{$graph_id}{$time}{$sub_id} = $val;
                    }
                    #print "$val," if ($graph_id eq 'cpu');
                }
            } else {
                #print "Error: $ERR\n";
            }
        }
        #print "\n";
        closedir(DIR);
    }
    return $rrds;
}

sub loadAllFromPylon {
    my $server = shift || return;
    my $server_id = $server->id();
    my @checks = split(/\|/, pylon("checks|$server_id"));
    my %graph_ids = ();
    foreach my $check (@checks) {
        if ($check =~/^([^,]+),/) {
            $graph_ids{$1}++;
        }
    }

    foreach my $graph_id (keys %graph_ids) {
        foreach my $check (@checks) {
            my ($check_id, $sub_id) = split(/,/,$check);
            if (!$sub_id) {
                $sub_id = $check_id;
            }
            next if ($check_id ne $graph_id);
            my $response = pylon("get|$check|$server_id");
            $response =~s/([0-9]+)\|//;
            my $now = $1;

            foreach my $data (split(/\|/, $response)) {
                if ($data eq 'nan') {
                    $data = undef;
                }
                $rrds->{$graph_id}{$now}{$sub_id} = $data;
                $now = $now + 300;
            }
        }
    }
    return $rrds;
}
