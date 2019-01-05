#!/usr/bin/perl
################################################################################
#
# Jenkins Lämpli -- Jenkins status updated
#
# Script to send Jenkins job info to the jenkins-status.pl on a webserver.
#
# Copyright (c) 2018 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;

use feature 'state';

use FindBin;
use lib "$FindBin::Bin";

use Time::HiRes qw(time sleep usleep);
use LWP::UserAgent;
use POSIX;
use XML::LibXML;
use JSON::PP;
use Sys::Hostname;
use Path::Tiny;

use Ffi::Debug ':all';

################################################################################
# configuration

my $CFG =
{
    verbosity      => 0,
    backend        => '',
    agent          => 'tschenggins-update/1.0',
    server         => Sys::Hostname::hostname(),
    backendtimeout => 5,
    jobname        => '',
    jobstate       => '',
    jobresult      => '',
};

do
{
    # parse command line
    while (my $arg = shift(@ARGV))
    {
        if    ($arg eq '-v') { $Ffi::Debug::VERBOSITY++; $CFG->{verbosity}++; }
        elsif ($arg eq '-q') { $Ffi::Debug::VERBOSITY--; $CFG->{verbosity}--; }
        elsif ($arg eq '-b') { $CFG->{backend} = shift(@ARGV); }
        elsif ($arg eq '-s') { $CFG->{server} = shift(@ARGV); }
        elsif ($arg eq '-J') { $CFG->{jobname} = shift(@ARGV); }
        elsif ($arg eq '-S') { $CFG->{jobstate} = shift(@ARGV); }
        elsif ($arg eq '-R') { $CFG->{jobresult} = shift(@ARGV); }
        elsif ($arg eq '-h') { help(); }
        else
        {
            ERROR("Illegal argument '%s'!", $arg);
            exit(1);
        }
    }
    DEBUG("CFG=%s", $CFG);

    my %states  = ( dontknow => -1, unknown => 0, off => 1, idle => 2, running => 3 );
    my %results = ( dontknow => -1, unknown => 0, success => 1, unstable => 2, failure => 3 );

    # determine most active state
    if ($CFG->{jobstate} && (index($CFG->{jobstate}, ',') > -1))
    {
        my $worstState = 'dontknow';
        foreach my $state (split(',', $CFG->{jobstate}))
        {
            if ( $state && ($states{$state} >= 0) && ($states{$state} > $states{$worstState}) )
            {
                $worstState = $state;
            }
            $CFG->{jobstate} = $worstState;
        }
    }
    # determine worst result
    if ($CFG->{jobresult} && (index($CFG->{jobresult}, ',') > -1))
    {
        my $worstResult = 'dontknow';
        foreach my $result (split(',', $CFG->{jobresult}))
        {
            if ( $result && ($results{$result} >= 0) && ($results{$result} > $results{$worstResult}) )
            {
                $worstResult = $result;
            }
            $CFG->{jobresult} = $worstResult;
        }
    }

    if ( !$CFG->{server} || !$CFG->{backend} || !$CFG->{jobname} || (!$CFG->{jobstate} && !$CFG->{jobresult}) )
    {
        ERROR("Try '$0 -h'.");
        exit(1);
    }

    # configure debugging output
    $Ffi::Debug::TIMESTAMP = 3;
    $Ffi::Debug::PRINTTYPE = 1;
    $Ffi::Debug::PRINTPID  = 1;

    # check server connection
    my $t0 = time();
    PRINT("Using backend at '%s'.", $CFG->{backend});
    my $userAgent = LWP::UserAgent->new( timeout => $CFG->{backendtimeout}, agent => $CFG->{agent} );
    my $resp = $userAgent->get("$CFG->{backend}?cmd=hello");
    if ($resp->is_success())
    {
        DEBUG("%s: %s", $resp->status_line(), $resp->decoded_content());
    }
    else
    {
        ERROR("Failed connecting to backend: %s", $resp->status_line());
        exit(1);
    }

    # send update
    my $update = { name => $CFG->{jobname}, server => $CFG->{server} };
    $update->{state} = $CFG->{jobstate} if ($CFG->{jobstate});
    $update->{result} = $CFG->{jobresult} if ($CFG->{jobresult});
    PRINT("Sending: server=%s name=%s state=%s result=%s", $update->{server}, $update->{name}, $update->{state}, $update->{result});
    my $json = JSON::PP->new()->utf8(1)->canonical(1)->pretty(0)->encode(
        { debug => ($CFG->{verbosity} > 0 ? 1 : 0), cmd => 'update', states => [ $update ] } );
    $resp = $userAgent->post($CFG->{backend}, 'Content-Type' => 'application/json', Content => $json);
    DEBUG($resp->status_line(), split(/\n/, $resp->decoded_content()));
    my $dt = time() - $t0;
    if ($resp->is_success())
    {
        PRINT("Successfully updated backend (%.3fs).", $dt);
        exit(0);
    }
    else
    {
        ERROR("Failed updating backend (%.3fs): %s: %s", $dt, $resp->status_line(), $resp->decoded_content());
        exit(1);
    }

};

sub help
{
    PRINT(
          "$CFG->{agent}",
          "Copyright (c) 2018 Philippe Kehl <flipflip at oinkzwurgl dot org>",
          "https://oinkzwurgl.org/projaeggd/tschenggins-laempli",
          "",
          "Update job info on the backend server.",
          "",
          "Usage:",
          "",
          "  $0 [-q] [-v] [-h] [-d] [-n <name>] -b <statusurl> -J <jobname> [-S <jobstate>] [-R <jobresult>]",
          "",
          "Where:",
          "",
          "  -h  prints this screen and exits",
          "  -q  decreases verbosity",
          "  -v  increases verbosity",
          "  -b <statusurl>  URL for the jenkins-status.pl backend",
          "  -s <server>     the Jenkins server name (default on this machine: $CFG->{server})",
          "  -J <name>       job name",
          "  -S <state>      job state (unknown, off, idle, running)",
          "  -R <result>     job result (unknown, success, unstable, failure)",
          "",
          "Note that you need to provide the job state and/or the job result.",
          "Multiple states or results can be given as a comma-separated list. In this case the most",
          "active state respectively the worst result in the list will be used to update the job.",
          "",
          "Examples:",
          "",
          "  $0 -b https://user:pass\@foo.bar/path/to/jenkins-status.pl \\",
          "      -J CI_make_world -S idle -R success",
          "",
         );
    exit(0);
}

__END__
