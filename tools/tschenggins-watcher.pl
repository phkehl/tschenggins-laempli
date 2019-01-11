#!/usr/bin/perl
####################################################################################################
#
# Jenkins Lämpli -- Jenkins status watcher
#
# This watches a given Jenkins jobs directory for changes and publishes them to the
# jenkins-status.pl on a webserver.
#
# Copyright (c) 2017-2019 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
####################################################################################################
#
# TODO:
# - handle jobs that can run multiple times in parallel
# - (maybe) handle job rename
#
####################################################################################################

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

# configure debugging output
$Ffi::Debug::TIMESTAMP = 3;
$Ffi::Debug::PRINTTYPE = 1;
$Ffi::Debug::PRINTPID  = 1;

####################################################################################################
# configuration

my $CFG =
{
    verbosity      => 0,
    backend        => '',
    agent          => 'tschenggins-watcher/3.0',
    checkperiod    => 60,
    server         => Sys::Hostname::hostname(),
    daemonise      => 0,
    backendtimeout => 5,
    statefile      => '',
    logfile        => '',
    pidfile        => '',
};

do
{
    my @jobdirs = ();

    # parse command line
    my $errors = 0;
    my %jobDirsSeen = ();
    my %jobNamesSeen = ();
    while (my $arg = shift(@ARGV))
    {
        if    ($arg eq '-v') { $Ffi::Debug::VERBOSITY++; $CFG->{verbosity}++; }
        elsif ($arg eq '-q') { $Ffi::Debug::VERBOSITY--; $CFG->{verbosity}--; }
        elsif ($arg eq '-b') { $CFG->{backend} = shift(@ARGV); }
        elsif ($arg eq '-s') { $CFG->{server} = shift(@ARGV); }
        elsif ($arg eq '-j') { $CFG->{statefile} = shift(@ARGV); }
        elsif ($arg eq '-h') { help(); }
        elsif ($arg !~ m{^-})
        {
            my $dir = path($arg);
            if ($dir->exists() && $dir->is_dir())
            {
                my $buildsDir = path("$arg/builds");
                if  ($buildsDir->exists() && $buildsDir->is_dir())
                {
                    if (!$jobDirsSeen{$dir})
                    {
                        push(@jobdirs, $dir);
                        my $jobName = $dir->basename();
                        $jobNamesSeen{$jobName} = $dir;
                    }
                    else
                    {
                        WARNING("Ignoring duplicate dir '$dir'!");
                    }
                    $jobDirsSeen{$dir}++;
                }
                else
                {
                    WARNING("Ignoring funny dir '$dir'!");
                }
            }
            else
            {
                WARNING("Ignoring nonexistent dir $dir!");
            }
        }
        else
        {
            ERROR("Illegal argument '%s'!", $arg);
            $errors++;
        }
    }

    if ($#jobdirs < 0)
    {
        WARNING("No job dir(s)!");
        $errors++;
    }

    if ($errors)
    {
        ERROR("Try '$0 -h'.");
        exit(1);
    }

    # check server connection
    if ($CFG->{backend})
    {
        my $userAgent = _userAgent();
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

    # run..
    run(@jobdirs);
};

sub help
{
    PRINT(
          "$CFG->{agent}",
          "Copyright (c) 2017-2019 Philippe Kehl <flipflip at oinkzwurgl dot org>",
          "https://oinkzwurgl.org/projaeggd/tschenggins-laempli",
          "",
          "Watch Jenkins job directories for changes and update the backend server accordingly.",
          "",
          "Usage:",
          "",
          "  $0 [-q] [-v] [-h] [-s <servername>] [-b <statusurl>] [-j <statusfile>] <jobdir> ...",
          "",
          "Where:",
          "",
          "  -h  prints this screen and exits",
          "  -q  decreases verbosity",
          "  -v  increases verbosity",
          "  -b <statusurl>  URL for the jenkins-status.pl backend",
          "  -j <statusfile>  write state to this file (in JSON)",
          "  -s <server>  the Jenkins server name (default on this machine: $CFG->{server})",
          "  -m <multijobname> consider all following <jobdir>s in this virtual multi-job",
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
          "  $0 -j status.json -m CI_ALL /var/lib/jenkins/jobs/CI_* -m Nightly_ALL /var/lib/jenkins/jobs/Nightly_*",
          "",
         );
    exit(0);

}

####################################################################################################
# watch Jenkins job dirs

sub run
{
    my (@jobdirs) = @_;

    unless ($CFG->{backend} || $CFG->{statefile})
    {
        WARNING("No state file or backend URL given.");
    }
    if ($CFG->{backend})
    {
        PRINT("Using backend '%s'.", $CFG->{backend});
    }
    if ($CFG->{statefile})
    {
        PRINT("Using state file '%s'.", $CFG->{statefile});
    }

    PRINT("Initialising...");
    my $in = Linux::Inotify2->new() || die($!);

    my $state = {};

    # populate initial state and add watcher for each job
    foreach my $jobDir (@jobdirs)
    {
        my $jobName = $jobDir->basename();
        my $buildsDir = path("$jobDir/builds");

        # get initial state
        my $jState = 'unknown';
        my $jResult = 'unknown';
        my $timestamp = 0;
        my ($latestBuildDir, $previousBuildDir) = # Note: there may be no build dir(s) (yet, anymore)
          reverse sort { $a->basename() <=> $b->basename() } $buildsDir->children(qr{^[0-9]+$});

        # get latest build state and result
        ($jState, $jResult, $timestamp) = getJenkinsJob($jobDir, $latestBuildDir);

        # skip foul things
        if (!defined $jState && !defined $jResult)
        {
            next;
        }

        # keep a state per job
        $state->{$jobName} =
        {
            jobName => $jobName, jobDir => $jobDir, buildsDir => $buildsDir, timestamp => 0,
            jState => 'dontknow', jStateDirty => 0,
            jResult => 'dontknow', jResultDirty => 0,
        };

        # get last result if the job is currently running
        if ($jState)
        {
            if ( ($jState eq 'running') && $previousBuildDir )
            {
                (undef, $jResult, undef) =  getJenkinsJob($jobDir, $previousBuildDir);
                $jResult //= 'unknown';
            }
        }

        # add watches
        if ($jState && $jResult)
        {
            setState($state->{$jobName}, $jState, $jResult, $timestamp);

            # watch job config for changes
            $in->watch($jobDir, IN_CREATE | IN_MOVED_TO, sub { jobChangedCb($state, $jobName, $in, @_); });

            # watch for new job output directories being created
            $in->watch($buildsDir, IN_CREATE, sub { jobCreatedCb($state, $jobName, $in, @_); });
            DEBUG("Watching '%s' in '%s' (current state %s and result %s).", $jobName, "$buildsDir", $jState, $jResult);
        }
        else
        {
            DEBUG("Not watching '%s' in '%s'", $jobName, "$buildsDir");
        }
    }

    # keep watching...
    my $nJobs = $#jobdirs + 1;
    my $lastCheck = 0;
    $in->blocking(0);
    while (1)
    {
        $in->poll();

        if ( (time() - $lastCheck) > $CFG->{checkperiod} )
        {
            PRINT("Watching %i jobs for '%s'.", $nJobs, $CFG->{server});
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
    #DEBUG("getJenkinsJob(%s, %s)", $jobDir, $buildDir);

    # FIXME: Maybe we could check for ./disabled, ./result and ./duration instead of insisting on certain root nodes.

    # load job config file
    my $configFile = "$jobDir/config.xml";
    if (!-f $configFile)
    {
        WARNING("Missing $configFile!");
        return;
    }
    my $config = loadXml($configFile);
    if ( !$config || ($config->nodeName() !~ m{^(project|flow-definition)$}) )
    {
        WARNING("Ignoring invalid $configFile (have %s)!", $config ? '<' . $config->nodeName() . '/>' : undef);
        return;
    }

    # check if job is disabled
    my ($disabled) = $config->findnodes('./disabled');
    $disabled = ($disabled && ($disabled->textContent() =~ m{true}i)) ? 1 : 0;
    #DEBUG("disabled=%s", $disabled);

    # check result and duration
    my $haveBuild = 0;
    my ($result, $duration, $timestamp);
    if ($buildDir)
    {
        my $buildFile = "$buildDir/build.xml";
        my $build = loadXml($buildFile);
        if ( $build && ($build->nodeName() !~ m{^(build|flow-build)$}) )
        {
            WARNING("Invalid build (project) type (have <%s/>, expected <build/>)!", $build->nodeName());
            return;
        }
        if ($build)
        {
            $haveBuild = 1;
            ($result)    = $build->findnodes('./result');
            ($duration)  = $build->findnodes('./duration');
            ($timestamp) = $build->findnodes('./startTime');
            $result    = $result    ? lc($result->textContent())                  : undef;
            $duration  = $duration  ? int($duration->textContent() * 1e-3 + 0.5)  : undef;
            $timestamp = $timestamp ? int($timestamp->textContent() * 1e-3 + 0.5) : undef;
        }
    }

    # determine answer
    DEBUG("build=%s disabled=%s result=%s duration=%s timestamp=%s (%s)",
          $haveBuild ? "present" : "missing", $disabled, $result, $duration, $timestamp, _age_str($timestamp));

    # job explicitly disabled --> state=off, result=unknown
    my $state;
    if ($disabled)
    {
        $state = 'off';
        $result = 'unknown';
    }
    # job not disabled and have build dir: we have a result: state=idle, we don't have a result yet: state=running
    elsif ($haveBuild)
    {
        $state = $result ? 'idle' : 'running';
    }
    # job disabled, don't know result, don't know state
    else
    {
        $state = 'idle';
        $result = 'unknown';
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

    # determine timestamp
    $timestamp //= time();
    $timestamp += $duration if ($duration);
    $timestamp = int($timestamp);

    return ($state, $result, $timestamp);
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
    my ($st, $jState, $jResult, $timestamp) = @_;

    $jState    //= $st->{jState};
    $jResult   //= $st->{jResult};
    $timestamp //= $st->{timestamp} || int(time());

    my $jStateDirty  = ($st->{jState}  ne $jState ) ? 1 : 0;
    my $jResultDirty = ($st->{jResult} ne $jResult) ? 1 : 0;

    PRINT("Job: %-45s state: %-20s result: %-20s age: %s", $st->{jobName},
          $jStateDirty  ? "$st->{jState} -> $jState"   : $jState,
          $jResultDirty ? "$st->{jResult} -> $jResult" : $jResult,
          _age_str($timestamp));

    $st->{jState}       = $jState;
    $st->{jResult}      = $jResult;
    $st->{jStateDirty}  = $jStateDirty;
    $st->{jResultDirty} = $jResultDirty;
    $st->{timestamp}    = $timestamp;
}

sub _age_str
{
    my ($ts) = @_;
    my $dt = time() - ($ts || 0);
    if ($dt > (86400*365.25))
    {
        return sprintf('%.1fa', $dt / 86400 / 365.25);
    }
    elsif ($dt > (86400*31))
    {
        return sprintf('%.1fm', $dt / 86400 / (365.25 / 12));
    }
    elsif ($dt > (86400*7))
    {
        return sprintf('%.1fw', $dt / 86400 / 7);
    }
    elsif ($dt > 86400)
    {
        return sprintf('%.1fd', $dt / 86400);
    }
    else
    {
        return sprintf('%.1fh', $dt / 3600);
    }
}

####################################################################################################
# inotify callbacks

# called when something was created in the builds directory
sub jobCreatedCb
{
    my ($state, $jobName, $in, $e) = @_;
    my $buildDir = $e->fullname();
    DEBUG("jobCreatedCb(%s) %s %s", $jobName, $buildDir, _eventStr($e));

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

# called when job config changes (e.g. job disabled/enabled)
sub jobChangedCb
{
    my ($state, $jobName, $in, $e) = @_;
    my $file = $e->fullname();
    DEBUG("jobChangedCb(%s) %s %s", $jobName, $file, _eventStr($e));

    if ($file =~ m{/config.xml$})
    {
        my ($jState, $jResult, $timestamp) = getJenkinsJob(path($file)->parent());

        if ( ($jState && $jResult) &&
             # only enable -> disable and disable -> enable transitions, do not consider other job edits
             ( ($jState eq 'off') || ($state->{$jobName}->{jState} eq 'off') )
           )
        {
            # set status
            setState($state->{$jobName}, $jState, $jResult, $timestamp);

            # update backend
            updateBackend($state);
        }
    }
}

# called when things change in the build directory, checks if job is done
sub jobDoneCb
{
    my ($state, $jobName, $e) = @_;
    my $file = $e->fullname();
    DEBUG("jobDoneCb(%s) %s %s", $jobName, $file, _eventStr($e));

    # the job is done once the build.xml file appears
    if ($file =~ m{/build.xml$})
    {
        # get result
        my ($jState, $jResult, $timestamp) = getJenkinsJob($state->{$jobName}->{jobDir}, path($file)->parent());
        DEBUG("Job '%s' has stopped, state is %s and result is '%s'.", $jobName, $jState, $jResult);

        # set status
        setState($state->{$jobName}, $jState, $jResult, $timestamp);

        # update backend
        updateBackend($state);

        # remove the watch on the build directory
        $e->w()->cancel();
    }
}

sub _eventStr
{
    my ($e) = @_;
    my $str = sprintf('%08x', $e->mask());
    $str .= ' IN_ACCESS'        if ($e->IN_ACCESS());
    $str .= ' IN_MODIFY'        if ($e->IN_MODIFY());
    $str .= ' IN_ATTRIB'        if ($e->IN_ATTRIB());
    $str .= ' IN_CLOSE_WRITE'   if ($e->IN_CLOSE_WRITE());
    $str .= ' IN_CLOSE_NOWRITE' if ($e->IN_CLOSE_NOWRITE());
    $str .= ' IN_OPEN'          if ($e->IN_OPEN());
    $str .= ' IN_MOVED_FROM'    if ($e->IN_MOVED_FROM());
    $str .= ' IN_MOVED_TO'      if ($e->IN_MOVED_TO());
    $str .= ' IN_CREATE'        if ($e->IN_CREATE());
    $str .= ' IN_DELETE'        if ($e->IN_DELETE());
    $str .= ' IN_DELETE_SELF'   if ($e->IN_DELETE_SELF());
    $str .= ' IN_MOVE_SELF'     if ($e->IN_MOVE_SELF());
    return $str;
}

####################################################################################################
# update jenkins-status.pl

sub updateBackend
{
    my ($state) = @_;

    # check what changed
    my @newUpdates = ();
    foreach my $jobName (sort keys %{$state})
    {
        my $st = $state->{$jobName};
        if ($st->{jStateDirty} || $st->{jResultDirty})
        {
            my $_st = { name => $st->{jobName}, server => $CFG->{server}, ts => $st->{timestamp} };
            # always send full info, users may have modified it on the server manually
            if ($st->{jStateDirty} || $st->{jResultDirty})
            {
                $_st->{state} = $st->{jState};
                $_st->{result} = $st->{jResult};
                $st->{jStateDirty} = 0;
                $st->{jResultDirty} = 0;
            }
            push(@newUpdates, $_st);
        }
    }
    #DEBUG("newUpdates=%s", \@newUpdates);

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
        DEBUG("updates=%s", \@updates);
        if ($#updates > -1)
        {
            my $t0 = time();
            my $userAgent = _userAgent();
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

    # update state file
    if (($#newUpdates > -1) && $CFG->{statefile})
    {
        my %json = ();
        foreach my $jobName (sort keys %{$state})
        {
            my $st = $state->{$jobName};
            $json{$jobName}->{$_} = $st->{$_} for (qw(jobName jState jResult timestamp));
        }
        PRINT("Updating '%s' (%i/%i changed states).", $CFG->{statefile}, $#newUpdates + 1, scalar keys %json);
        eval
        {
            local $SIG{__DIE__} = 'default';
            my $jsonStr = JSON::PP->new()->utf8(1)->canonical(1)->pretty(1)->encode(\%json);
            path($CFG->{statefile})->append_utf8({ truncate => 1 }, $jsonStr);
        };
        if ($@)
        {
            WARNING("Could not write '%s': %s", $CFG->{statefile}, $! || "$@");
        }
    }
}

sub _userAgent
{
    my ($trace) = @_;
    my $ua = LWP::UserAgent->new( timeout => $CFG->{backendtimeout}, agent => $CFG->{agent} );
    if ($trace)
    {
        $ua->add_handler('request_send', sub
        {
            my ($req, $ua, $h) = @_;
            WARNING("trace 'request_send' follows:");
            $req->dump();
            return;
        });
        $ua->add_handler('response_done', sub
        {
            my ($resp, $ua, $h) = @_;
            WARNING("trace 'response_done' follows:");
            $resp->dump();
            return;
        });
    }
    return $ua;
}

####################################################################################################
__END__
