#!/usr/bin/perl
################################################################################
#
# Jenkins Lämpli -- Jenkins status watcher
#
# This watches a given Jenkins jobs directory for changes and publishes them
# to the jenkins-status.pl on a webserver.
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################
#
# TODO:
# - handle jobs that can run multiple times in parallel
# - logging (when using -d)
#
################################################################################

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin";

use Time::HiRes qw(time sleep usleep);
use LWP::UserAgent;
use Linux::Inotify2;
use POSIX;
use XML::Simple;
use JSON::PP;
use Clone;
use Sys::Hostname;
#use BSD::Resource;

use Ffi::Debug ':all';

# limit our resource usage
#setrlimit( RLIMIT_VMEM, 0.35 * ( 1 << 30 ), 1 * ( 1 << 30 ) );


################################################################################
# configuration

my $CFG =
{
    verbosity   => 0,
    backend     => '',
    agent       => 'tschenggins-watcher/1.0',
    checkperiod => 300,
    server      => Sys::Hostname::hostname(),
    daemonise   => 0,
};

my @pendingUpdates = ();

do
{
    my @jobdirs = ();

    # parse command line
    while (my $arg = shift(@ARGV))
    {
        if    ($arg eq '-v') { $Ffi::Debug::VERBOSITY++; $CFG->{verbosity}++; }
        elsif ($arg eq '-q') { $Ffi::Debug::VERBOSITY--; $CFG->{verbosity}--; }
        elsif ($arg eq '-b') { $CFG->{backend} = shift(@ARGV); }
        elsif ($arg eq '-s') { $CFG->{server} = shift(@ARGV); }
        elsif ($arg eq '-d') { $CFG->{daemonise} = 1; }
        elsif ($arg eq '-h') { help(); }
        elsif (-d $arg && -f "$arg/config.xml" && -d "$arg/builds")
        {
            push(@jobdirs, $arg);
        }
        else
        {
            ERROR("Illegal argument '%s'!", $arg);
            exit(1);
        }
    }
    DEBUG("jobdirs=%s", \@jobdirs);

    if ($#jobdirs < 0)
    {
        ERROR("Try '$0 -h'.");
        exit(1);
    }

    # configure debugging output
    $Ffi::Debug::TIMESTAMP = 3;
    $Ffi::Debug::PRINTTYPE = 1;
    $Ffi::Debug::PRINTPID  = 1;

    # check server connection
    if ($CFG->{backend})
    {
        PRINT("Using backend at '%s'.", $CFG->{backend});
        my $userAgent = LWP::UserAgent->new( timeout => 5, agent => $CFG->{agent} );
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
    }

    # daemonise?
    if ($CFG->{daemonise})
    {
        my $pid = fork();
        if ($pid)
        {
            PRINT("tschenggins-watcher.pl running, pid=%i", $pid);
            exit(0);
        }
        chdir('/');
        open(STDIN, '<', '/dev/null');
        open(STDOUT, '>', '/dev/null');
        open(STDERR, '>', '/dev/null');
        setsid();
    }

    # run..
    run(@jobdirs);
};

sub help
{
    PRINT(
          "jenkins-watcher.pl",
          "Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>",
          "https://oinkzwurgl.org/projaeggd/tschenggins-laempli",
          "",
          "Usage:",
          "",
          "  $0 [-q] [-v] [-h] [-d] [-n <name>] -b <statusurl> <jobdir> ...",
          "",
          "Where:",
          "",
          "  -h  prints this screen and exits",
          "  -q  decreases verbosity",
          "  -v  increases verbosity",
          "  -b <statusurl>  URL for the jenkins-status.pl backend",
          "  -s <server>  the Jenkins server name (default on this machine: $CFG->{hostname})",
          "  -d  run in background",
          "  <jobdir> one or more Jenkins job directories to monitor",
          "",
          "Examples:",
          "",
          "  $0 -s gugus -b https://user:pass\@foo.bar/path/to/jenkins-status.pl /var/lib/jenkins/jobs/CI_*",
          "",
         );
    exit(0);

}

################################################################################
# watch Jenkins job dirs

sub run
{
    my @jobdirs = @_;

    PRINT("Initialising...");
    my $in = Linux::Inotify2->new() || die($!);
    $in->blocking(0);

    my $state = {};

    foreach my $jobDir (@jobdirs)
    {
        my $jobName = $jobDir; $jobName =~ s{.*/}{};
        my $buildsDir = "$jobDir/builds";

        # keep a state per job
        $state->{$jobName} =
        {
            jobName => $jobName, jobDir => $jobDir, buildsDir => $buildsDir,
            jState => 'dontknow', jResult => 'dontknow',
        };

        # get initial state
        my $lastBuildNo = (reverse sort { $a <=> $b }
                           grep { $_ =~ m{^\d+$} }
                           map { $_ =~ s{.*/}{}; $_ } glob("$buildsDir/[0-9]*"))[0];
        my ($jState, $jResult) = getJenkinsJob("$jobDir/config.xml", $lastBuildNo ? "$buildsDir/$lastBuildNo/build.xml" : undef);
        setState($state->{$jobName}, $jState, $jResult);

        # watch for new job output directories being created
        $in->watch($buildsDir, IN_CREATE, sub { jobcreated($state->{$jobName}, $in, @_); });

        DEBUG("Watching '%s' in '%s'.", $jobName, $buildsDir);
    }

    # update all
    update(values %{$state});


    # keep watching...
    my $lastCheck = 0;
    while (1)
    {
        $in->poll();

        if ( (time() - $lastCheck) > $CFG->{checkperiod} )
        {
            PRINT("Watching %i jobs for '%s'.", $#jobdirs + 1, $CFG->{server});
            $lastCheck = time();
            if ($#pendingUpdates > -1)
            {
                update();
            }
        }

        usleep(250e3);
    }
}

sub getJenkinsJob
{
    my ($configXmlFile, $buildXmlFile) = @_;
    my $config;
    eval
    {
        local $SIG{__DIE__} = 'IGNORE';
        if ($configXmlFile)
        {
            $config = XML::Simple::XMLin($configXmlFile);
        }
    };

    my $build;
    eval
    {
        local $SIG{__DIE__} = 'IGNORE';
        $build = XML::Simple::XMLin($buildXmlFile);
    };
    my $jState = $buildXmlFile ? (-f $buildXmlFile ? 'idle' : 'running') : 'unknown';
    my $jResult = 'unknown';
    my $jDuration = 0;
    if ( $config && $config->{disabled} && ($config->{disabled} =~ m{true}i) )
    {
        $jState = 'unknown';
    }
    elsif ($build && $build->{result} && ($build->{result} =~ m{^\s*(success|failure|unstable)\s*$}i) )
    {
        $jResult = lc($1);
        $jDuration = $build->{duration} ? $build->{duration} * 1e-3 : 0;
    }
    return ($jState, $jResult, $jDuration);
}

sub setState
{
    my ($st, $jState, $jResult) = @_;

    $jState  //= $st->{jState};
    $jResult //= $st->{jResult};

    my $jStateDirty  = ($st->{jState}  ne $jState ) ? 1 : 0;
    my $jResultDirty = ($st->{jResult} ne $jResult) ? 1 : 0;

    PRINT("%-40s state: %-20s result: %-20s", $st->{jobName},
          $jStateDirty  ? "$st->{jState} -> $jState"   : $jState,
          $jResultDirty ? "$st->{jResult} -> $jResult" : $jResult);

    $st->{jState}       = $jState;
    $st->{jResult}      = $jResult;
    $st->{jStateDirty}  = $jStateDirty;
    $st->{jResultDirty} = $jResultDirty;
}



################################################################################
# callbacks

# called when something was created in the builds directory
sub jobcreated
{
    my ($st, $in, $e) = @_;
    my $buildDir = $e->fullname();

    # does it look like a build directory?
    if ( -d $buildDir && ($buildDir =~ m{/\d+$}) )
    {
        # watch created and moved files
        $in->watch($buildDir, IN_CREATE | IN_MOVED_TO, sub { jobdone($st, @_); });

        DEBUG("Job '%s' has started, watching '%s'.", $st->{jobName}, $buildDir);

        # set status
        setState($st, 'running');

        # update backend
        update($st);
    }
}

# called when things change in the build directory, checks if job is done
sub jobdone
{
    my ($st, $e) = @_;
    my $file = $e->fullname();
    #DEBUG("Job '%s' has new file '%s'.", $st->{jobName}, $createdFile);

    # the job is done once the build.xml file appears
    if ($file =~ m{/build.xml$})
    {
        # get result
        my ($jState, $jResult, $jDuration) = getJenkinsJob(undef, $file);
        DEBUG("Job '%s' has stopped, duration=%.1fm, result is '%s'.", $st->{jobName}, $jDuration / 60, $jResult);

        # set status
        setState($st, $jState, $jResult);

        # update backend
        update($st);

        # remove the watch on the build directory
        $e->w()->cancel();
    }
}

################################################################################
# update jenkins-status.pl

sub update
{
    my @states;
    foreach my $st (grep { $_->{jStateDirty} || $_->{jResultdirty} } @_)
    {
        my $_st = { name => $st->{jobName}, server => $CFG->{server} };
        if ($st->{jStateDirty})
        {
            $_st->{state} = $st->{jState};
            $st->{jStateDirty} = 0;
        }
        if ($st->{jResultDirty})
        {
            $_st->{result} = $st->{jResult};
            $st->{jResultDirty} = 0;
        }
        push(@states, $_st);
    }
    DEBUG("update() %s", \@states);

    # send to backend if configured
    if ($CFG->{backend})
    {
        my @removedUpdates = splice(@pendingUpdates, 0, 50);
        if ($#removedUpdates > -1)
        {
            WARNING("Dropped %i pending updates.", $#removedUpdates + 1);
        }

        my @updates = (@pendingUpdates, @states);
        if ($#updates > -1)
        {
            my $userAgent = LWP::UserAgent->new( timeout => 5, agent => $CFG->{agent} );
            my $json = JSON::PP->new()->utf8(1)->canonical(1)->pretty(0)->encode(
                { debug => ($CFG->{verbosity} > 0 ? 1 : 0), cmd => 'update', states => \@updates } );
            my $resp = $userAgent->post($CFG->{backend}, 'Content-Type' => 'text/json', Content => $json);
            DEBUG("%s: %s", $resp->status_line(), $resp->decoded_content());
            if ($resp->is_success())
            {
                PRINT("Successfully updated backend with %i states (%i pending).", $#states + 1, $#pendingUpdates + 1);
            }
            else
            {
                ERROR("Failed updating backend with %i states: %s", $#states + 1, $resp->status_line());

                # try again next time
                push(@pendingUpdates, map { Clone::clone($_) } @states);
            }
        }
        else
        {
            WARNING("Nothing to update?!");
        }
    }
}

__END__
