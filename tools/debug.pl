#!/usr/bin/perl
################################################################################
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use utf8;
use strict;
use warnings;
use Time::HiRes qw(time sleep usleep);
use Device::SerialPort;
#use Win32::SerialPort;
#use Win32::Console::ANSI;
use IO::Socket::INET;
use IO::Handle;


################################################################################
# config

my $debug = 0;
my $n = 0;

STDOUT->autoflush(1);
#sed 's/\x1b[^m]*m//g'

################################################################################
# parse command line and create handle and input func

my $inputFunc = undef;

if ($ARGV[0] && ($ARGV[0] =~ m@^(/dev/[^:]+|COM\d)(?::(\d+)|)$@))
{
    my $port = $1;
    my $br = $2 || 115200;
    printf("serial port: [%s] [%s]\n", $port, $br);# if ($debug);
    unless ($inputFunc = createHandleSerial($port, $br))
    {
        printf(STDERR "ERROR: Failed opening serial port '%s', baudrate %i!\n", $port, $br);
    }
}
elsif ($ARGV[0] && ($ARGV[0] =~ m@^([^:]+)(?::(\d+)|)@))
{
    my $host = $1;
    my $port = $2 || 6454;
    printf("Art-Net: [%s] [%s]\n", $host, $port);# if ($debug);
    unless ($inputFunc = createHandleNet($host, $port))
    {
        printf(STDERR "ERROR: Failed binding host '%s', port %i!\n", $host, $port);
    }
}

unless ($inputFunc)
{
    print(STDERR "\n\n");
    print(STDERR "Usage: $0 <device>:<baudrate> | <host>:<port>\n");
    print(STDERR "\n");
    print(STDERR "E.g. $0 /dev/ttyUSB0:115200   or   $0 localhost:6454\n");
    print(STDERR "\n\n");
    exit(1);
}


################################################################################
# handle user abort (C-c) nicely

my $ABORT = 0;
$SIG{INT} = sub { printf(STDERR "Got C-c. Aborting ASAP!\n"); $ABORT = 1; };


################################################################################
# colours for the ffm debug messages

my %colours =
(
    ERROR => "\e[31m", WARNING => "\e[33m", NOTICE => "\e[1m", PRINT => "\e[m",
    DEBUG => "\e[36m", ts => "\e[35m", _OFF_ => "\e[m", ASCII => "\e[34m"
);


################################################################################
# wait for input data, parse it and display it

my $t0 = time();
my $rxbuf = '';
while (!$ABORT)
{
    # get more input (rx)
    my $data = $inputFunc->();
    if ($data)
    {
        $rxbuf .= $data;
        if ($debug)
        {
            printf(STDERR "%05i: got %i bytes: %s\n", $n++, length($data),
                   join(' | ', map { my $b = $_;
                                     sprintf('0x%02x [%c]', $b,
                                             chr($b) =~ m/^[[:print:]]$/ ? $b : ord('?')) }
                   unpack('C*', $data)));
        }
    }
    else
    {
        usleep(10e3);
    }

    # send anything?
    #if (defined (my $tx = <STDIN>))
    #{
    #    $tx =~ s/\r?\n$//;
    #    if (defined $tx)
    #    {
    #        printf(STDERR "Sending %u [%s]\n", length($tx), $tx) if ($debug);
    #        #$sp->write("$tx\r\n");
    #        $sp->write("$tx");
    #    }
    #}

    # parse rx data and display
    while (my $msg = parse(\$rxbuf))
    {
        if ($msg->{_name} eq 'ERROR')
        {
            #`bell`;
            print("\a");
        }
        printf("$colours{ts}%07.3f$colours{_OFF_} ", time() - $t0);
        if (defined $colours{$msg->{_name}})
        {
            print($colours{$msg->{_name}} . $msg->{_str} . $colours{_OFF_} . "\n");
        }
        elsif ($msg->{_str})
        {
            printf("%s[%i]: %s\n", $msg->{_name}, $msg->{_size}, $msg->{_str});
        }
        else
        {
            printf("%s[%i]\n", $msg->{_name}, $msg->{_size});
        }
    }
}


################################################################################
# input data parser, will return undef or a structure like this:
# { _name => 'some message identifier', _raw => 'raw message data',
#   _size => size of raw message data, _str => 'stringified message data' }

my $garbage = '';


sub parse
{
    my $bufRef = shift;

    my $msg = undef;

    # try parsing..
    while (length($$bufRef))
    {
        # parser wants more data
        my $wait = 0;

        # try parse: ERROR/WARNING/NOTICE/PRINT/DEBUG message
        #if ($$bufRef =~ m/^(\n*([EWNPD]): ([[:print:]]+)\r?\n+)/)
        if ($$bufRef =~ m/^(\n*([EWNPD]): ([[:print:]\p{Alnum}]+)\r?\n+)/)
        {
            my %map = ( E => 'ERROR', W => 'WARNING', N => 'NOTICE', P => 'PRINT', D => 'DEBUG' );
            $msg = { _name => $map{$2}, _size => length($1), _raw => $1, _str => "$2: $3" };
        }
        # try parse: ASCII data followed by CRLF or LF
        elsif ($$bufRef =~ m/^([[:print:]]*\r*\n+)/)
        {
            my $raw = $1;
            my $str = $1; # stringify by..
            $str =~ s/\r/\\r/g; # ..removing some non-printable stuff
            $str =~ s/\n/\\n/g;
            $str =~ s/\t/\\t/g;
            $msg = { _name => 'ASCII', _size => length($raw), _raw => $raw, _str => $str };
        }
        # try parse: incomplete ASCII data
        #elsif ($$bufRef =~ m/^[[:print:]]+$/)
        elsif ($$bufRef =~ m/^[[:print:]\p{Alnum}]+$/)
        {
            $wait = 1;
        }

        # have something --> remove from buffer and return
        if (defined $msg)
        {
            # if we have collected garbage, return this first
            if ($garbage)
            {
                my $str = join(' ', map { sprintf('%02x', $_) } unpack('C*', $garbage));
                #my $str = $garbage;
                #$str =~ s/([^[:print:]\p{Alnum}])/sprintf("\\x{%x}",ord($1))/eg;

                $msg = { _name => 'GARBAGE', _size => length($garbage), _raw => $garbage,
                         _str => $str };
                $garbage = '';
            }
            else
            {
                if ($debug)
                {
                    printf(STDERR "[%s] [%i] [%s]\n", $msg->{_name}, $msg->{_size}, $msg->{_str});
                }
                substr($$bufRef, 0, $msg->{_size}, '');
            }
            return $msg;
        }

        # have nothing yet --> retry later
        if (!defined $msg && $wait)
        {
            return undef;
        }

        # have nothing now and more data available -> drop one byte from the input buffer and try again
        if (!defined $msg && length($$bufRef))
        {
            my $g = substr($$bufRef, 0, 1);
            #printf(STDERR "DROPPING GARBAGE BYTE 0x%02x [%s]\n", unpack('C', $g), $g);
            $garbage .= $g;
            substr($$bufRef, 0, 1, '');
        }

    }

    return $msg;
}


################################################################################
# input function for serial port

sub createHandleSerial
{
    my ($port, $br) = @_;

    my $sp = Device::SerialPort->new($port) || return undef;
    #my $sp = Win32::SerialPort->new($port) || return undef;
    $sp->baudrate($br);
    $sp->databits(8);
    $sp->parity('none');
    $sp->stopbits(1);
    $sp->handshake('none');
    $sp->buffers(4096, 4096);
    $sp->user_msg(0); # debugging
    $sp->error_msg(0); # STFU
    $sp->read_char_time(0);
    $sp->read_const_time(0);
    $sp->write_settings();
    $sp->reset_error();
    $sp->lookclear();
    $sp->reset_error();

    return sub
    {
        $sp->reset_error();
        return $sp->input() || '';
    };
}


################################################################################
# input function for Art-Net DiagData input

sub createHandleNet
{
    my ($host, $port) = @_;

    my $h = IO::Socket::INET->new( LocalHost => ($host ne '*') ? $host : undef,
                                   LocalPort => $port, Proto => 'udp', Blocking => 0 ) || return undef;
    return sub
    {
        my $msg;
        if ($h->recv($msg, 1024))
        {
            my ($port, $ipaddr) = sockaddr_in($h->peername());
            my $host = gethostbyaddr($ipaddr, AF_INET);
            my ($id, $opCode, $protVer) = unpack('Z8vn', $msg);
            if ( ($id eq 'Art-Net') && ($protVer == 14) && ($opCode == 0x2300) )
            {
                my (undef, $prio, undef, undef, $len, $msg) = unpack('CCCCnZ*', substr($msg, 12));
                printf("%s:%s $id %x %x %x %u [%s]\n", $host, $port, $opCode, $protVer, $prio, $len, substr($msg, 0, -1)) if ($debug);
                return $msg;
            }
        }
    };
}


################################################################################
1;
__END__
