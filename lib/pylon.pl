use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);

sub pylon {
    my $command = shift;

    my ($remote,$port, $iaddr, $paddr, $proto);

    #$remote  = "localhost";
    $remote  = `/sbin/ifconfig eth1 | grep "inet addr" | cut -d':' -f 2 | cut -d ' ' -f 1`;
    $port    = 5555;
    $iaddr   = inet_aton($remote) || die "no host: $remote";
    $paddr   = sockaddr_in($port, $iaddr);

    $proto   = getprotobyname('tcp');
    socket(PSOCK, PF_INET, SOCK_STREAM, $proto) || die "socket: $!";
    connect(PSOCK, $paddr) || die "connect: $!";

    select(PSOCK);
    $| = 1;
    select(STDOUT);
    my $strlen = length($command);
    print PSOCK "$command|EOF\n";

    my $response = '';
    while (my $line = <PSOCK>) {
        if ($line eq "\n") {
            last;
        }
        $response .= $line;
    }
    close(PSOCK);

    return $response;
}

sub waitForIt {
    my $args = shift || die;
    die unless (ref($args) eq 'HASH');

    my $step = $args->{step} || return;
    my $verbose = $args->{verbose} || 0;

    my $last = time();
    my $first = $last;
    print "waiting for the right time\n" if ($verbose);
    while ((time() % $step) > 0 || time() == $first) {
        if (time() != $last) {
            print localtime(time())  . "\r" if ($verbose);
            $last = time();
        }
        usleep(10000);
    }
    print "\n";
}

sub start {
    print "starting pylon\n";
    my $command = "$PYLON_HOME/init start";
    print "$command\n" if ($debug);
    print `$command`;
}

sub stop {
    print "stopping pylon\n";
    my $command = "$PYLON_HOME/init stop";
    print "$command\n" if ($debug);
    print `$command`;
}

1;
