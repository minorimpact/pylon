#!/usr/bin/perl

use strict;
use lib $ENV{PERL5LIB} || '/site/lib';
use SERVERROLES::Serverroles;
use COMMON::Cron;
use COMMON::Util;
use COMMON::Options;
use SNMP;
use Data::Dumper;
use Storable qw( lock_store lock_retrieve );
require "/filer/home/pgillan/pylon/pylon.pl";

my $SNMP_community = 'V@r10uS';
my $SNMP_version = '2c';
my $SNMP_baseoid = '.1.3.6.1.4.1.39086.1';

my $options = new COMMON::Options(
                    config=>{'force' => 'Force a run, regardless.',
                             'cluster_id:s' => 'Collect graph info for a particular cluster, or a comma separated list of clusters.',
                             'verbose' => 'Verbose output.',
                             'silo_id:s' => 'Collect graph info for a particular silo (or a comma separated list of silos).',
                             'server_id:s' => 'Bridge a particular server (or a comma separated list of servers).'
                         }
              );

exit if ($options->print_usage_on_errors());
my $verbose = $options->get('verbose');
my $silo_id = $options->get('silo_id');
my $server_id = $options->get('server_id');
my $cluster_id = $options->get('cluster_id');

my @silo_ids = split(/,/, $silo_id);
my @server_ids = split(/,/, $server_id);
my @cluster_ids = split(/,/, $cluster_id);

if (! $options->get('force') && &COMMON::Util::CheckForDuplicates(1,0,0,290)) {
    print "Already running one of these\n" if ($options->{verbose});
    exit;
}

eval { COMMON::Cron::execCron(\&main, {no_log=>$verbose}); };
print "$@\n" if ($@);
exit;

sub main {
    my $ROLES = new SERVERROLES::Serverroles;
    $SERVERROLES::DO_CACHE = 1;

    my @servers = ();
    if (scalar(@silo_ids) || scalar(@server_ids) || scalar(@cluster_ids)) {
        foreach my $silo_id (@silo_ids){
            push (@servers, $ROLES->getServers({silo_id=>$silo_id}));
        }
        foreach my $server_id (@server_ids){
            push (@servers, $ROLES->getServer($server_id));
        }
        foreach my $cluster_id (@cluster_ids){
            push (@servers, $ROLES->getServers({cluster_id=>$cluster_id}));
        }
    } else {
        # By default, will collect data from all servers in the same silo.
        push (@servers, $ROLES->getServers());
    }

    foreach my $server (@servers) {
        my $server_id = $server->id();
        my $backnet_ip = $server->backnetIP();
        my $cluster_id = ($server->getClusters())[0]->id();
        my $type_id = ($server->getTypes())[0]->id();
        next if ($type_id eq "website" || $type_id eq "aggregate" || $type_id eq "vserver");

        print "$server_id($cluster_id/$type_id): $backnet_ip\n" if ($verbose);
        
        my $sess = new SNMP::Session(DestHost => $backnet_ip, Community => $SNMP_community, Version => $SNMP_version, UseNumeric=>1, Retries=>1);
        if (!$sess || $sess->{ErrorNum}) {
            print "can't create session object.\n";
            print "$sess->{ErrorNum}:'$sess->{ErrorStr}'\n";
            next;
        }

        my $graphdatacheck = $sess->get("$SNMP_baseoid.0");
        if (!$graphdatacheck) {
            print "$server_id doesn't support snmp graph data.\n";
            next;
        } elsif ($sess->{ErrorNum}) {
            print "$sess->{ErrorNum}:'$sess->{ErrorStr}'\n";
            next;
        }


        my %servers = ();
        my %checks = ();
        my %subchecks = ();

        my $vb = new SNMP::Varbind(["$SNMP_baseoid.1.2"]);
        for (my $i=0; !$sess->{ErrorNum}; $i++) {
            my $value = $sess->getnext($vb);
            $value =~s/^\"//;
            $value =~s/\"$//;

            last unless (${$vb}[0] =~ /^$SNMP_baseoid\.1\.2\.*/);

            my $server_key = ${$vb}[1];
            if (!defined($servers{$server_key})) {
                #print "\$servers{$server_key} = \$ROLES->getServer($value)\n";
                $servers{$server_key} = $ROLES->getServer($value);
            }
        }
        if ($sess->{ErrorNum}) {
            print $sess->{ErrorStr} . "\n";
            return -1;
        }

        my $vb = new SNMP::Varbind(["$SNMP_baseoid.1.4"]);
        for (my $i=0; !$sess->{ErrorNum}; $i++) {
            my $value = $sess->getnext($vb);
            $value =~s/^\"//;
            $value =~s/\"$//;

            last unless (${$vb}[0] =~ /^$SNMP_baseoid\.1\.4\.([\d]+)/);
            my $server_key = $1;
            my $check_key = ${$vb}[1];

            if (!defined($checks{$server_key}{$check_key})) {
                #print "\$checks{$server_key}{$check_key} = $value\n";
                $checks{$server_key}{$check_key} = $value;
            }
        }
        if ($sess->{ErrorNum}) {
            print $sess->{ErrorStr} . "\n";
            return -1;
        }

        my $vb = new SNMP::Varbind(["$SNMP_baseoid.1.6"]);
        for (my $i=0; !$sess->{ErrorNum}; $i++) {
            my $value = $sess->getnext($vb);
            $value =~s/^\"//;
            $value =~s/\"$//;

            last unless (${$vb}[0] =~ /^$SNMP_baseoid\.1\.6\.([\d]+)\.([\d]+)/);
            my $server_key = $1;
            my $check_key = $2;
            my $subcheck_key = ${$vb}[1];

            if (!defined($subchecks{$server_key}{$check_key}{$subcheck_key})) {
                #print "\$subchecks{$server_key}{$check_key}{$subcheck_key} = $value\n";
                $subchecks{$server_key}{$check_key}{$subcheck_key} = $value;
            }
        }
        if ($sess->{ErrorNum}) {
            print $sess->{ErrorStr} . "\n";
            return -1;
        }

        my $vb = new SNMP::Varbind(["$SNMP_baseoid.1.7"]);
        for (my $i=0; !$sess->{ErrorNum}; $i++) {
            my $value = $sess->getnext($vb);
            $value =~s/^\"//;
            $value =~s/\"$//;

            last unless (${$vb}[0] =~ /^$SNMP_baseoid\.1\.7\.([\d]+)\.([\d]+)/);
            my $server_key = $1;
            my $check_key = $2;
            my $subcheck_key = ${$vb}[1];

            my $graph_server = $servers{$server_key};
            if ($graph_server) {
                my $graph_id = $checks{$server_key}{$check_key} . "," . $subchecks{$server_key}{$check_key}{$subcheck_key}; 
                print "graph:" . $graph_server->id() . "." . $graph_id . "=" . $value . "\n" if ($verbose);
                pylon("add|$graph_id|$server_id|$value");
            }
        }
        if ($sess->{ErrorNum}) {
            print $sess->{ErrorStr} . "\n";
            return -1;
        }
    }
}

