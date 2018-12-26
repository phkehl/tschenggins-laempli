#!/usr/bin/perl
################################################################################
#
# Jenkins Lämpli -- Jenkins status watcher
#
# This watches a given Jenkins jobs directory for changes and publishes them
# to the jenkins-status.pl on a webserver.
#
# Copyright (c) 2017-2018 Philippe Kehl <flipflip at oinkzwurgl dot org>
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

use feature 'state';

use FindBin;
use lib "$FindBin::Bin";

use Time::HiRes qw(time sleep usleep);
use LWP::UserAgent;
use Linux::Inotify2;
use POSIX;
use XML::LibXML;
use JSON::PP;
use Sys::Hostname;
use Path::Tiny;
use BSD::Resource;

use Ffi::Debug ':all';

# limit our resource usage
setrlimit( RLIMIT_VMEM, 0.35 * ( 1 << 30 ), 1 * ( 1 << 30 ) );


################################################################################
# configuration

my $CFG =
{
    verbosity      => 0,
    backend        => '',
    agent          => 'tschenggins-watcher/2.0',
    checkperiod    => 60,
    server         => Sys::Hostname::hostname(),
    daemonise      => 0,
    backendtimeout => 5,
};

do
{
    my @jobdirs = ();

    # parse command line
    my %jobDirsSeen = ();
    while (my $arg = shift(@ARGV))
    {
        if    ($arg eq '-v') { $Ffi::Debug::VERBOSITY++; $CFG->{verbosity}++; }
        elsif ($arg eq '-q') { $Ffi::Debug::VERBOSITY--; $CFG->{verbosity}--; }
        elsif ($arg eq '-b') { $CFG->{backend} = shift(@ARGV); }
        elsif ($arg eq '-s') { $CFG->{server} = shift(@ARGV); }
        elsif ($arg eq '-d') { $CFG->{daemonise} = 1; }
        elsif ($arg eq '-h') { help(); }
        elsif ($arg !~ m{^-})
        {
            my $dir = path($arg);
            if ($dir->exists() && $dir->is_dir())
            {
                if (!$jobDirsSeen{$dir})
                {
                    push(@jobdirs, $dir);
                }
                else
                {
                    WARNING("Ignoring duplicate dir $dir!");
                }
                $jobDirsSeen{$dir}++;
            }
            else
            {
                WARNING("Ignoring nonexistent dir $dir!");
            }
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
          "$CFG->{agent}",
          "Copyright (c) 2017-2018 Philippe Kehl <flipflip at oinkzwurgl dot org>",
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
          "  -s <server>  the Jenkins server name (default on this machine: $CFG->{server})",
          "  -d  run in background (daemonise)",
          "  <jobdir> one or more Jenkins job directories to monitor",
          "",
          "Note that this uses the Linux 'inotify' interface to detect changes in the",
          "Jenkins job output directories. As such it only works on local filesystems",
          "and not on remote filesystems (NFS, Samba/SMB, ...).",
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

    my $state = {};

    foreach my $jobDir (@jobdirs)
    {
        my $jobName = $jobDir->basename();
        my $buildsDir = path("$jobDir/builds");

        # keep a state per job
        $state->{$jobName} =
        {
            jobName => $jobName, jobDir => $jobDir, buildsDir => $buildsDir,
            jState => 'dontknow', jStateDirty => 0,
            jResult => 'dontknow', jResultDirty => 0,
        };

        # get initial state
        my $jState = 'unknown';
        my $jResult = 'unknown';
        my ($latestBuildDir, $previousBuildDir) =
          reverse sort { $a->basename() <=> $b->basename() } $buildsDir->children(qr{^[0-9]+$});
        if ($latestBuildDir)
        {
            ($jState, $jResult) = getJenkinsJob($jobDir, $latestBuildDir);
        }
        if ($jState)
        {
            if ( ($jState eq 'running') && $previousBuildDir )
            {
                (undef, $jResult) =  getJenkinsJob($jobDir, $previousBuildDir);
            }
        }

        if ($jState && $jResult)
        {
            setState($state->{$jobName}, $jState, $jResult);

            # watch for new job output directories being created
            $in->watch($buildsDir, IN_CREATE, sub { jobCreatedCb($state, $jobName, $in, @_); });
        }

        DEBUG("Watching '%s' in '%s' (current state %s and result %s).", $jobName, "$buildsDir", $jState, $jResult);
    }

    # keep watching...
    my $lastCheck = 0;
    $in->blocking(0);
    while (1)
    {
        $in->poll();

        if ( (time() - $lastCheck) > $CFG->{checkperiod} )
        {
            PRINT("Watching %i jobs for '%s'.", $#jobdirs + 1, $CFG->{server});
            $lastCheck = time();
            # retry previously failed updates
            updateBackend($state);
        }

        usleep(250e3);
    }
}

sub getJenkinsJob
{
    my ($jobDir, $buildDir) = @_;

    # load job config file
    my $configFile = "$jobDir/config.xml";
    if (!-f $configFile)
    {
        WARNING("Missing $configFile!");
        return;
    }
    my $config = loadXml($configFile);
    if (!$config || ($config->nodeName() ne 'project'))
    {
        WARNING("Invalid $configFile (%s, expected <project/>)!", $config ? '<' . $config->nodeName() . '/>' : undef);
        return;
    }

    # check if job is disabled
    my ($disabled) = $config->findnodes('./disabled');
    $disabled = ($disabled && ($disabled->textContent() =~ m{true}i)) ? 1 : 0;
    #DEBUG("disabled=%s", $disabled);

    # check result and duration
    my $buildFile = "$buildDir/build.xml";
    my $build = loadXml($buildFile);
    if ($build && ($build->nodeName() ne 'build'))
    {
        WARNING("Invalid build (project) type (%s)!", $build->nodeName());
        return;
    }
    my ($result, $duration);
    if ($build)
    {
        ($result)   = $build->findnodes('./result');
        ($duration) = $build->findnodes('./duration');
        $result   = $result   ? lc($result->textContent())                 : undef;
        $duration = $duration ? int($duration->textContent() * 1e-3 + 0.5) : undef;
    }

    # determine answer
    DEBUG("build=%s disabled=%s result=%s duration=%s",
          $build ? "present" : "missing", $disabled, $result, $duration);

    # job explicitly disabled --> state=off
    my $state;
    if ($disabled)
    {
        $state = 'off';
    }
    # job not disabled: we have a result: state=idle, we don't have a result yet: state=running
    else
    {
        $state = $result ? 'idle' : 'running';
    }

    # we have a duration, i.e. the job has completed and should have a result
    if (defined $duration && defined $result)
    {
        if ($result =~ m{(success|failure|unstable)})
        {
            $result = $result;
        }
        elsif ($result =~ m{(aborted|not_built)})
        {
            $result = 'unknown';
        }
        else
        {
            $result = 'unknown';
        }
    }
    else
    {
        $result = 'unknown';
    }

    return ($state, $result);
}

sub loadXml
{
    my ($xmlFile) = @_;

    my $parser = XML::LibXML->new( line_numbers => 1, no_basefix => 0 );
    my $doc;
    eval
    {
        local $SIG{__DIE__} = 'IGNORE';
        $doc = $parser->parse_file($xmlFile);
    };
    if ($@)
    {
        my $e = $@;
        my $str = ref($e) ? $e->as_string() : $e;
        my @lines = grep { $_ } split(/\n/, $str);
        foreach my $line (@lines)
        {
            $line =~ s{^[^:]+/ProtocolSpec/}{};
            DEBUG("Warning: %s", $line);
        }
    }

    my $root = $doc ? $doc->documentElement() : undef;
    DEBUG("loadXml(%s): %s", $xmlFile, $root ? $root->nodeName() : undef);
    return $root;
}

sub setState
{
    my ($st, $jState, $jResult) = @_;

    $jState  //= $st->{jState};
    $jResult //= $st->{jResult};

    my $jStateDirty  = ($st->{jState}  ne $jState ) ? 1 : 0;
    my $jResultDirty = ($st->{jResult} ne $jResult) ? 1 : 0;

    PRINT("Job: %-40s state: %-20s result: %-20s", $st->{jobName},
          $jStateDirty  ? "$st->{jState} -> $jState"   : $jState,
          $jResultDirty ? "$st->{jResult} -> $jResult" : $jResult);

    $st->{jState}       = $jState;
    $st->{jResult}      = $jResult;
    $st->{jStateDirty}  = $jStateDirty;
    $st->{jResultDirty} = $jResultDirty;
}



################################################################################
# inotify callbacks

# called when something was created in the builds directory
sub jobCreatedCb
{
    my ($state, $jobName, $in, $e) = @_;
    my $buildDir = $e->fullname();

    # does it look like a build directory?
    if ( -d $buildDir && ($buildDir =~ m{/\d+$}) )
    {
        # watch for created and moved files
        $in->watch($buildDir, IN_CREATE | IN_MOVED_TO, sub { jobDoneCb($state, $jobName, @_); });

        DEBUG("Job '%s' has started, watching '%s'.", $jobName, $buildDir);

        # set status
        setState($state->{$jobName}, 'running');

        # update backend
        updateBackend($state);
    }
}

# called when things change in the build directory, checks if job is done
sub jobDoneCb
{
    my ($state, $jobName, $e) = @_;
    my $file = $e->fullname();
    #DEBUG("Job '%s' has new file '%s'.", $jobName, $file);

    # the job is done once the build.xml file appears
    if ($file =~ m{/build.xml$})
    {
        # get result
        my ($jState, $jResult) = getJenkinsJob($state->{$jobName}->{jobDir}, path($file)->parent());
        DEBUG("Job '%s' has stopped, state is %s and result is '%s'.", $jobName, $jState, $jResult);

        # set status
        setState($state->{$jobName}, $jState, $jResult);

        # update backend
        updateBackend($state);

        # remove the watch on the build directory
        $e->w()->cancel();
    }
}


################################################################################
# update jenkins-status.pl

sub updateBackend
{
    my ($state) = @_;

    # check what changed
    my @newUpdates = ();
    foreach my $jobName (sort keys %{$state})
    {
        my $st = $state->{$jobName};
        if ($st->{jStateDirty} || $st->{jResultdirty})
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
            push(@newUpdates, $_st);
        }
    }

    # send to backend if configured
    if ($CFG->{backend})
    {
        # sometimes sending the data to the backend may fail, so we try re-sending those newUpdates on
        # the next update (latest after $CFG->{checkperiod} seconds)
        state @pendingUpdates;
        #DEBUG("pendingUpdates=%s", \@pendingUpdates);

        my $maxPendingStates = 50;
        my $nRemovedStates = 0;
        while ($#pendingUpdates > ($maxPendingStates - 1))
        {
            splice(@pendingUpdates, 0, 1);
            $nRemovedStates++;
        }
        if ($nRemovedStates > 0)
        {
            WARNING("Dropped %i pending updates.", $nRemovedStates);
        }

        my @updates = (@pendingUpdates, @newUpdates);
        if ($#updates > -1)
        {
            my $t0 = time();
            DEBUG("updates=%s", \@updates);
            my $userAgent = LWP::UserAgent->new( timeout => $CFG->{backendtimeout}, agent => $CFG->{agent} );
            my $json = JSON::PP->new()->utf8(1)->canonical(1)->pretty(0)->encode(
                { debug => ($CFG->{verbosity} > 0 ? 1 : 0), cmd => 'update', states => \@updates } );
            my $resp = $userAgent->post($CFG->{backend}, 'Content-Type' => 'application/json', Content => $json);
            DEBUG("%s: %s", $resp->status_line(), $resp->decoded_content());
            my $dt = time() - $t0;
            if ($resp->is_success())
            {
                PRINT("Successfully updated backend with %i new and %i pending states (%.3fs).",
                      $#newUpdates + 1, $#pendingUpdates + 1, $dt);
                @pendingUpdates = ();
            }
            else
            {
                ERROR("Failed updating backend with %i new and %i pending states (%.3fs): %s",
                      $#newUpdates + 1, , $#pendingUpdates + 1, $dt, $resp->status_line());

                # try again next time
                push(@pendingUpdates, @newUpdates);
            }
        }
        else
        {
            DEBUG("Nothing to update.");
        }
    }
}

__END__
