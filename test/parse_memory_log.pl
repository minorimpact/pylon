#!/usr/bin/perl


my %malloc;
#open(LOG, "/tmp/pylon.log");
open(LOG, "/tmp/placeholder.log");
seek(LOG,-50000000, 2);
while(<LOG>) {
    my $line = $_;
    chomp($line);
    my @f = split(/ /,  $line);
    if ($f[0] eq 'malloc') {
        $malloc{$f[4]} = $line;
    } elsif ($f[0] eq 'free') {
        delete($malloc{$f[4]});
    }
}

foreach my $key (keys %malloc) {
    print $malloc{$key} . "\n";
}

