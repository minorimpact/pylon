package Pylon;

use FindBin;
use Socket;
use Time::HiRes qw(gettimeofday tv_interval usleep);

sub new {
    my $package     = shift || return;
    my $class       = ref($package) || $package;
    my $params      = shift;
    my $self        = {};

    $self->{host} = $params->{host} || `/sbin/ifconfig eth1 | grep "inet addr" | cut -d':' -f 2 | cut -d ' ' -f 1` || 'localhost';
    $self->{port} = $params->{port} || 5555;
    $self->{debug} = $params->{debug} || 0;
    $self->{verbose} = $params->{verbose} || 0;

    bless($self, $class);
    return $self;
}

sub command {
    my $self = shift || return;
    my $command = shift || return;

    my ($iaddr, $paddr, $proto);

    $iaddr   = inet_aton($self->{host}) || die "no host: $self->{host}";
    $paddr   = sockaddr_in($self->{port}, $iaddr);

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
    my $self = shift || return;
    my $args = shift || die;
    die unless (ref($args) eq 'HASH');

    my $step = $args->{step} || return;

    my $last = time();
    my $first = $last;
    print "waiting for the right time\n" if ($self->{verbose});
    while ((time() % $step) > 0 || $first == $last) {
        if (time() != $last) {
            print localtime(time())  . "\r" if ($self->{verbose});
            $last = time();
        }
        usleep(10000);
    }
    print localtime(time()) . "\n" if ($self->{verbose});
}

sub start {
    my $self = shift || return;
    print "starting pylon\n" if ($self->{verbose});
    my $command = "$FindBin::Bin/../init start";
    print "$command\n" if ($self->{debug});
    print `$command`;
    sleep 1;
}

sub stop {
    my $self = shift || return;
    print "stopping pylon\n" if ($self->{verbose});
    my $command = "$FindBin::Bin/../init stop";
    print "$command\n" if ($self->{debug});
    print `$command`;
}

sub options {
    my $self = shift || return;
    my $options;

    my $option_str = $self->command("options");
    foreach my $o (split(/ /, $option_str)) {
        my ($key, $value) = split(/=/, $o);
        $options->{$key} = $value;
    }
    return $options;
}


1;
