use Socket;

sub pylon {
    my $command = shift;

    my ($remote,$port, $iaddr, $paddr, $proto);

    $remote  = "ii100-39.friendfinderinc.com";
    $port    = 5555;
    $iaddr   = inet_aton($remote) || die "no host: $remote";
    $paddr   = sockaddr_in($port, $iaddr);

    $proto   = getprotobyname('tcp');
    socket(SOCK, PF_INET, SOCK_STREAM, $proto) || die "socket: $!";
    connect(SOCK, $paddr) || die "connect: $!";

    select(SOCK);
    $| = 1;
    select(STDOUT);
    my $strlen = length($command);
    print SOCK "$command|EOF\n";

    my $response = '';
    while (my $line = <SOCK>) {
        if ($line eq "\n") {
            last;
        }
        $response .= $line;
    }
    #$response = <SOCK>;
    close(SOCK);

    return $response;
}

sub printstatus {
    print pylon("status");
}


1;
