#!/usr/bin/perl

require "pylon.pl";
my $pidfile = "/var/run/pylon_dumper.pid";
my $pid = $$;

if (-f $pidfile) {
    my $oldpid = `cat $pidfile`;
    chomp($oldpid);
    my $ps = `ps -o pid --no-heading -p $oldpid`;
    chomp($ps);
    if ($ps eq $oldpid) {
        exit;
    }
    unlink($pidfile);
}
open(PIDFILE, ">$pidfile");
print PIDFILE "$pid\n";
close(PIDFILE);

my $check_count = 0;
my $server_count = 0;
print "starting dump\n";
my $start_time = time();
open(DUMP,">/tmp/pylon_dump.tmp");
my $server_list = pylon('servers');
chomp($server_list);
my @servers = split(/\|/, $server_list);

foreach my $server_id (@servers) {
    print "$server_id\n";
    $server_count++;
    my @checks = split(/\|/, pylon("checks|$server_id"));

    foreach my $check_id (@checks) {
        print "    $check_id\n";
        $check_count++;
        my $dump = pylon("dump|$check_id|$server_id");
        chomp($dump);
        print DUMP "$dump\n";
    }
}
close(DUMP);
my $end_time = time();
my $dump_time = $end_time - $start_time;
rename("/tmp//pylon_dump.tmp","/tmp/pylon_dump");
my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks) = stat("/tmp/pylon_dump");

print "dump complete.\nservers: $server_count  checks:$check_count  time:${dump_time}s size:$size\n";

