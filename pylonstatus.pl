#!/usr/bin/perl

my $PYLON_HOME = $ENV{PYLON_HOME};
die "\$PYLON_HOME is not set\n" unless ($PYLON_HOME);
die "PYLON_HOME='$PYLON_HOME': invalid directory\n" unless (-d $PYLON_HOME);

require "$PYLON_HOME/lib/pylon.pl";

print pylon("status");

