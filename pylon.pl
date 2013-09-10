use Socket;


sub pylon {
    my $command = shift;

    my ($remote,$port, $iaddr, $paddr, $proto);

    $remote  = "ii83-22.friendfinderinc.com";
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


1;
