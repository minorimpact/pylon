#!/usr/bin/perl

require "/home/pgillan/pylon/pylon.pl";

my %servers = ();
my %checks = ();
my $record_count = 0;
my $start = time();
if (-f "/tmp/pylon_dump") {
    open(DUMP,"</tmp/pylon_dump");

    while(my $server_check = <DUMP>) {
        $record_count++;
        chomp($server_check);
        my @items = split(/\|/, $server_check);
        my $server_id = shift @items;
        $servers{$server_id}++;

        my $check_id = shift @items;
        $checks{$check_id}++;

        print "$server_id, $check_id\n";
        pylon("load|$server_check");
    }
    close(DUMP);
    my $end = time();
    print "loaded $record_count records in " . ($end - $start) . "s\n";
} else {
    print "nothing to load.\n";
}

