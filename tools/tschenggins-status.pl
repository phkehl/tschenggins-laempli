#!/usr/bin/perl

=pod

=encoding utf8

=head1 tschenggins-status.pl -- Tschenggins Lämpli Backend and User Inferface

Copyright (c) 2017-2018 Philippe Kehl <flipflip at oinkzwurgl dot org>,
L<https://oinkzwurgl.org/projaeggd/tschenggins-laempli>

=head2 Usage

C<< http://..../tschenggins-status.pl?param=value;param=value;... >>

C<< ./tschenggins-status.pl param=value param=value ... >>

=cut

use strict;
use warnings;

use CGI qw(-nosticky); # -debug
use CGI::Carp qw(fatalsToBrowser);
use JSON::PP;
use Fcntl qw(:flock :seek);
use FindBin;
use Digest::MD5;
use Data::Dumper;
$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Terse = 1;
use lib "/mnt/fry/d1/flip/tschenggins-laempli/ng/tools";
use Pod::Usage;
use IO::Handle;
use Time::HiRes qw(time);

my $q = CGI->new();

my @DEBUGSTRS     = ();
my $DATADIR       = $FindBin::Bin;
my $VALIDRESULT   = { unknown => 1, success => 2, unstable => 3, failure => 4 };
my $VALIDSTATE    = { unknown => 1, off => 2, running => 3, idle => 4 };
my $UNKSTATE      = { name => 'unknown', server => 'unknown', result => 'unknown', state => 'unknown', ts => int(time()) };
my $JOBNAMERE     = qr{^[-_a-zA-Z0-9]{5,50}$};
my $SERVERNAMERE  = qr{^[-_a-zA-Z0-9.]{5,50}$};
my $JOBIDRE       = qr{^[0-9a-z]{8,8}$};
my $DBFILE        = $ENV{'REMOTE_USER'} ? "$DATADIR/tschenggins-status-$ENV{'REMOTE_USER'}.json" : "$DATADIR/tschenggins-status.json";

#DEBUG("DATADIR=%s, VALIDRESULT=%s, VALIDSTATE=%s", $DATADIR, $VALIDRESULT, $VALIDSTATE);

do
{
    my $TITLE  = 'Tschenggins Lämpli';

    ##### get parameters #####

=pod

=head2 Parameters

=over

=item * C<cmd> -- the command

=item * C<debug> -- debugging on (1) or off (0, default)

=item * C<gui> -- which GUI to display ('jobs', 'clients')

=item * C<job> -- job ID

=item * C<result> -- job result ('unknown', 'success', 'unstable', 'failure')

=item * C<state> -- job state ('unknown', 'running', 'idle')

=item * C<redirct> -- where to redirect to

=item * C<ascii> -- US-ASCII output (1) or UTF-8 (0, default)

=item * C<client> -- client ID

=item * C<server> -- server name

=item * C<offset> -- offset into list of results (default 0)

=item * C<limit> -- limit of list of results (default 0, i.e. all)

=item * C<strlen> -- chop long strings at length (default 256)

=item * C<name> -- client or job name

=item * C<staip> -- client station IP address

=item * C<stassid> -- client station SSID

=item * C<version> -- client software version

=item * C<maxch> -- maximum number of channels the client can handle

=item * C<chunked> -- use "Transfer-Encoding: chunked" with given chunk size
        (default 0, i.e. not chunked), only for JSON output (e.g. cmd=list)

=back

=cut

    # query string, application/x-www-form-urlencoded or multipart/form-data
    my $cmd      = $q->param('cmd')      || '';
    my $debug    = $q->param('debug')    || 0;
    my $gui      = $q->param('gui')      || ''; # 'jobs', 'clients'
    my $job      = $q->param('job')      || '';
    my $result   = $q->param('result')   || ''; # 'unknown', 'success', 'unstable', 'failure'
    my $state    = $q->param('state')    || ''; # 'unknown', 'off', 'running', 'idle'
    my $redirect = $q->param('redirect') || '';
    my $ascii    = $q->param('ascii')    || 0;
    my $client   = $q->param('client')   || ''; # client id
    my $server   = $q->param('server')   || ''; # server name
    my $offset   = $q->param('offset')   || 0;
    my $limit    = $q->param('limit')    || 0;
    my $strlen   = $q->param('strlen')   || 256;
    my $name     = $q->param('name')     || '';
    my $staip    = $q->param('staip')    || '';
    my $stassid  = $q->param('stassid')  || '';
    my $version  = $q->param('version')  || '';
    my $maxch    = $q->param('maxch')    || 10;
    my $chunked  = $q->param('chunked')  || 0;
    my @states   = (); # $q->multi_param('states');
    my @jobs     = $q->multi_param('jobs');
    my $model    = $q->param('model')    || '';
    my $driver   = $q->param('driver')   || ''; # TODO: @key-@val
    my $order    = $q->param('order')    || '';
    my $bright   = $q->param('bright')   || '';
    my $noise    = $q->param('noise')    || '';
    my $cfgcmd   = $q->param('cfgcmd')   || '';

    # default: gui
    if (!$cmd)
    {
        $cmd = 'gui';
        #$gui = 'jobs';
    }

    # application/json POST
    my $contentType = $q->content_type();
    if ( $contentType && ($contentType eq 'application/json') )
    {
        my $jsonStr = $q->param('POSTDATA');
        my $jsonObj;
        eval
        {
            local $^W;
            local $SIG{__DIE__} = 'IGNORE';
            $jsonObj = JSON::PP->new()->utf8()->decode($jsonStr);
        };
        if ($jsonObj)
        {
            $cmd    =    $jsonObj->{cmd}      if ($jsonObj->{cmd});
            $ascii  =    $jsonObj->{ascii}    if ($jsonObj->{ascii});
            $debug  =    $jsonObj->{debug}    if ($jsonObj->{debug});
            @states = @{ $jsonObj->{states} } if ($jsonObj->{states});
        }
    }
    $q->delete_all();
    $q->param('debug', 1) if ($debug);
    DEBUG("cmd=%s user=%s", $cmd, $ENV{'REMOTE_USER'} || 'anonymous');
    $chunked = 0 if ($chunked !~ m{^\d+$}); # positive integers only


    ##### output is HTML, JSON or text #####

    my @html = ();
    my $data = undef;
    my $text = '';
    my $error = '';


    ##### load and lock "database" #####

    (my $dbHandle, my $db, $error) = _dbOpen();
    if ($error)
    {
        $cmd = '';
    }

    my $signalClient = 0;

    ##### handle requests #####

=pod

=head2 Commands

=head3 Status Commands

These commands are used to update the Jenkins jobs, e.g. by the C<tschenggins-watcher.pl> script.

=over

=item  B<< C<< cmd=hello >> or C<< cmd=delay >> >>

Connection check. Responds with "hi there" (immediately or after a delay).

=cut

    # connection check
    if ($cmd eq 'hello')
    {
        $text = "hi there, " . ($ENV{'REMOTE_USER'} || 'anonymous');
    }
    elsif ($cmd eq 'delay')
    {
        sleep(2);
        $text = "hi there, " . ($ENV{'REMOTE_USER'} || 'anonymous');
    }

=pod

=item B<< C<< cmd=update [debug=...] [ascii=...] <states=...> [...] >> >>

This expects a application/json POST request. The C<states> array consists of objects with the following
fields: C<server>, C<name> and optionally C<state> and/or C<result>.

=cut

    # update from tschenggins-watcher.pl
    elsif ($cmd eq 'update')
    {
        (my $res, $error) = _update($db, @states);
        if ($res)
        {
            $text = "db updated";
            #$data = { res => $res, text => 'db updated' };
        }
    }

=pod

=back

=head3 Client Commands

=over

=item B<<  C<< cmd=list offset=<number> limit=<number> >> >>

List available jobs with optional offset and/or limit.

=cut

    # list of jobs
    elsif ($cmd eq 'list')
    {
        ($data, $error) = _list($db, $offset, $limit);
    }

=pod

=item B<<  C<< cmd=get job=<jobid> >> >>

Get job state.

=cut

    # get job
    elsif ($cmd eq 'get')
    {
        ($data, $error) = _get($db, $job);
    }


=pod

=item B<<  C<< cmd=jobs client=<clientid> name=<client name> staip=<client station IP> stassid=<client station SSID> version=<client sw version> strlen=<number> maxch=<number> >> >>

Returns info for a client and updates client info.

=cut

    elsif ($cmd eq 'jobs')
    {
        ($data, $error) = _jobs($db, $client, $strlen,
            { name => $name, staip => $staip, stassid => $stassid, version => $version, maxch => $maxch });
    }

=pod

=item B<<  C<< cmd=realtime client=<clientid> name=<client name> staip=<client station IP> stassid=<client station SSID> version=<client sw version> strlen=<number> maxch=<number> >> >>

Returns info for a client and updates client info. This persistent connection with real-time update as things happen.

=cut

    elsif ($cmd eq 'realtime')
    {
        # dummy call like cmd=leds to check the parameters and update the client info in the DB
        ($data, $error) = _jobs($db, $client, $strlen,
            { name => $name, staip => $staip, stassid => $stassid, version => $version, maxch => $maxch });

        # clear pending commands
        $db->{cmd}->{$client} = '';
        $db->{_dirtiness}++;

        # save pid so that previous instances can terminate in case they're still running
        # and haven't noticed yet that the Lämpli is gone (Apache waiting "forever" for TCP timeout)
        unless ($error)
        {
            $db->{clients}->{$client}->{pid} = $$;
            $db->{_dirtiness}++;
        }

        # continues in call to _realtime() below... (unless $error)
    }

=pod

=back

=head3 Web Interface Commands

These commands are used by the web interface.

=over

=item B<<  C<< cmd=help >> >>

Print help.

=cut

    # help
    elsif ($cmd eq 'help')
    {
        print($q->header( -type => 'text/html', -expires => 'now', charset => 'UTF-8'));
        #pod2usage({ -verbose => 2,  -exitval => 0, -output => \*STDOUT, -perldocopt => '-ohtml', }); # only newer Perls :-(
        exec("perldoc -ohtml $0");
        exit(0);
    }

=pod

=item B<<  C<< cmd=gui gui=jobs >> / C<< cmd=gui gui=clients >> / C<< cmd=gui gui=client client=<clientid> >> >>

The web interface.

=cut

    elsif ($cmd eq 'gui')
    {
        push(@html, _gui($db, $gui, $client));
    }

=pod

=item B<<  C<< cmd=set job=<jobid> state=<state string> result=<result string> >> >>

Set job state and/or result.

=cut

    elsif ($cmd eq 'set')
    {
        (my $res, $error) = _set($db, $job, $state, $result);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=item B<<  C<< cmd=add job=<job name> server=<server name> state=<state string> result=<result string> >> >>

Add job.

=cut

    elsif ($cmd eq 'add')
    {
        (my $res, $error) = _add($db, $server, $job, $state, $result);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=item B<<  C<< cmd=del job=<jobid> >> >>

Delete job.

=cut

    elsif ($cmd eq 'del')
    {
        (my $res, $error) = _del($db, $job);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=item B<<  C<< cmd=rawdb >> >>

Returns the raw database (JSON).

=cut

    elsif ($cmd eq 'rawdb')
    {
        delete $db->{_dirtiness};
        $data = { db => $db, res => 1 };
    }

=pod

=item B<<  C<< cmd=rmclient client=<clientid> >> >>

Remove client info.

=cut

    # remove client data
    elsif ($cmd eq 'rmclient')
    {
        if ($client && $db->{clients}->{$client})
        {
            delete $db->{clients}->{$client};
            delete $db->{config}->{$client};
            $db->{_dirtiness}++;
            $text = "client $client removed";
        }
        else
        {
            $error = 'illegal parameter';
        }
    }

=item B<<  C<< cmd=cfgjobs client=<clientid> jobs=<jobID> ... >> >>

Set client jobs configuration.

=cut

    # set client jobs configuration
    elsif ($cmd eq 'cfgjobs')
    {
        DEBUG("jobs $client @jobs");
        if ($client && $db->{clients}->{$client} && $db->{config}->{$client} && ($#jobs > -1) && ($#jobs < 100))
        {
            # remove illegal and empty IDs
            @jobs = map { $_ && $db->{jobs}->{$_} ? $_ : '' } @jobs;
            $db->{config}->{$client}->{jobs} = \@jobs;
            $db->{_dirtiness}++;
            $text = "client $client set jobs @jobs";
            # signal server
            if ($db->{clients}->{$client}->{pid})
            {
                $signalClient = $db->{clients}->{$client}->{pid};
            }
        }
        else
        {
            $error = 'illegal parameter';
        }
    }

=item B<<  C<< cmd=cfgdevice client=<clientid> model=<...> driver=<...> order=<...> bright=<...> noise=<...> name=<...> >> >>

Set client device configuration.

=cut

    # set client device configuration
    elsif ($cmd eq 'cfgdevice')
    {
        DEBUG("cfg $client $model $driver $order $bright $noise $name");
        if ($client && $db->{config}->{$client}) # && $model && $driver && $order && $bright && $noise && $name)
        {
            $db->{config}->{$client}->{model}  = $model;
            $db->{config}->{$client}->{driver} = $driver;
            $db->{config}->{$client}->{order}  = $order;
            $db->{config}->{$client}->{bright} = $bright;
            $db->{config}->{$client}->{noise}  = $noise;
            $name =~ s{[^a-z0-9A-Z]}{_}g;
            $db->{config}->{$client}->{name}   = substr($name, 0, 20);
            $db->{_dirtiness}++;
            $text = "client $client set config $model $driver $order $bright $noise $name";
            # signal server
            if ($db->{clients}->{$client}->{pid})
            {
                $signalClient = $db->{clients}->{$client}->{pid};
            }
        }
        else
        {
            $error = 'illegal parameter';
        }
    }

=pod

=item B<<  C<< cmd=cfgcmd cfgcmd=<...> >> >>

Send command to client.

=cut

    # set client device configuration
    elsif ($cmd eq 'cfgcmd')
    {
        DEBUG("cmd $client $cfgcmd");
        if ($client && $db->{config}->{$client} && $cfgcmd)
        {
            $db->{cmd}->{$client} = $cfgcmd;
            $db->{_dirtiness}++;
            $text = "client $client send command $cfgcmd";
            # signal server
            if ($db->{clients}->{$client}->{pid})
            {
                $signalClient = $db->{clients}->{$client}->{pid};
            }
        }
        else
        {
            $error = 'illegal parameter';
        }
    }

    # illegal command
    else
    {
        if (!$error)
        {
            $error = 'illegal command';
        }
    }

=pod

=back

=cut

    ####################################################################################################

    ##### close, update database with changes, and unlock #####

    _dbClose($dbHandle, $db, $debug, $error ? 0 : 1);

    # signal client?
    if ($signalClient)
    {
        kill('USR1', $signalClient);
    }


    ##### real-time status monitoring #####

    if ( !$error && ($cmd eq 'realtime') )
    {
        _realtime($client, $strlen, { name => $name, staip => $staip, stassid => $stassid, version => $version }); # this doesn't return
        exit(0);
    }


    ##### render output #####

    # redirect?
    if (!$error && $redirect )
    {
        $q->delete_all();
        print($q->redirect($q->url() . "?$redirect" . ($debug ? ';debug=1' : '')));
        exit(0);
    }

    # output
    if (!$error && ($#html > -1))
    {
        my $pre = join('', map { "$_\n" } @DEBUGSTRS);
        $pre =~ s{<}{&lt;}gs;
        $pre =~ s{>}{&gt;}gs;
        my $css = '';
        #$css .= "* { margin: 0; padding: 0; }\n";
        $css .= "body, select, input { font-family: sans-serif; background-color: hsl(160, 100%, 95%); font-size: 100%; }\n";
        $css .= "table { padding: 0; border: 0.1em solid #000; border-collapse: collapse; }\n";
        $css .= "table td, table th { margin: 0; padding: 0.1em 0.25em 0.1em 0.25em;  border: 1px solid #000; }\n";
        $css .= "table th { font-weight: bold; background-color: #ddd; text-align: left; border-bottom: 1px solid #000; }\n";
        $css .= "table tr:hover td, table tr:hover td select, table tr:hover td input { background-color: hsl(160, 100%, 90%); }\n";
        $css .= "input, select { border: 0.1em solid #aaa; border-radius: 0.25em; }\n";
        $css .= "label { display: block; cursor: pointer; }\n";
        $css .= "label:hover, tr:hover td input:hover, option:hover, select:hover { background-color: hsl(160, 100%, 80%); border-color: #444; }\n";
        $css .= "td.online { color: hsl(125, 100%, 40%); } td.offline { color:hsl(0, 100%, 40%); }\n";
        $css .= "div.led { display: inline-block; margin: 0; padding: 0; width: 0.75em; height: 0.75em; max-width: 0.75em; max-height: 0.75em; border: 0.1em solid #aaa; border-radius: 0.75em; }\n";
        $css .= "\@keyframes pulse { 0% { opacity: 0.2; } 50% { opacity: 0.6; } 100% { opacity: 1.0; } }\n";
        $css .= "\@keyframes flicker { 0%, 50%, 70% { opacity: 0.2; } 5%, 66%, 100% { opacity: 0.5; } 35% { opacity: 0.8; } }\n";
        $css .= "div.led.state-unknown   { animation: flicker 1s ease-in-out 0s infinite; } \n";
        $css .= "div.led.state-off       { opacity: 0.5; } \n";
        $css .= "div.led.state-running   { animation: pulse 1s ease-in-out 0s infinite alternate; } \n";
        $css .= "div.led.state-idle      {} \n";
        $css .= "div.led.result-unknown  { background-color: hsl(  0,   0%, 50%); } \n";
        $css .= "div.led.result-success  { background-color: hsl(100, 100%, 50%); } \n";
        $css .= "div.led.result-unstable { background-color: hsl( 60, 100%, 50%); } \n";
        $css .= "div.led.result-failure  { background-color: hsl(  0, 100%, 50%); } \n";
        print(
              $q->header( -type => 'text/html', -expires => 'now', charset => 'UTF-8',
                          #'-Access-Control-Allow-Origin' => '*'
                        ),
              $q->start_html(-title => $TITLE,
                             -head => [ '<meta name="viewport" content="width=device-width, initial-scale=1.0"/>' ],
                             -style => { -code => $css }),
              $q->h1($TITLE),
              @html,
              $q->pre($pre),
              $q->end_html()
             );
    }
    elsif (!$error && $text)
    {
        my $content = "$text" . ($#DEBUGSTRS > -1 ? "\n\n" . join('', map { "$_\n" } @DEBUGSTRS) : '');
        print(
              $q->header( -type => 'text/plain', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                          '-Content-Length' => length($content),
                          #'-Access-Control-Allow-Origin' => '*'
                        ),
              $content
             );
    }
    elsif (!$error && $data)
    {
        $data->{debug} = \@DEBUGSTRS if ($debug );
        $data->{res} = 0 unless ($data->{res});
        my $json = JSON::PP->new()->ascii($ascii ? 1 : 0)->utf8($ascii ? 0 : 1)->canonical(1)->pretty($debug ? 1 : 0)->encode($data);
        if (!$chunked)
        {
            print(
                  $q->header( -type => 'application/json', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                              # avoid "Transfer-Encoding: chunked" by specifying the actual content length
                              # so that the raw output will be exactly and only the json string
                              # (i.e. no https://en.wikipedia.org/wiki/Chunked_transfer_encoding markers)
                              '-Content-Length' => length($json),
                              #'-Access-Control-Allow-Origin' => '*'
                            ),
                  $json
                 );
        }
        else
        {
            print(
                  $q->header( -type => 'application/json', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                              #'-Transfer-Encoding' => 'chunked',
                              #'-Access-Control-Allow-Origin' => '*'
                            )
                 );
            STDOUT->autoflush(1);
            for (my $offs = 0; $offs < length($json); $offs += $chunked)
            {
                print(substr($json, $offs, $chunked));
                #STDOUT->flush();
                Time::HiRes::usleep(1e3); # FIXME: is this the right way to force the chunk to be sent?
            }

        }
    }
    else
    {
        $error ||= "illegal request parameter(s)";
        my $content = "400 Bad Request: $error\n\n" . join('', map { "$_\n" } @DEBUGSTRS);
        print(
              $q->header(-type => 'text/plain', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                         -status => 400,
                         #'-Access-Control-Allow-Origin' => '*',
                         '-Content-Length' => length($content)),
              $content
             );
    }
};

sub DEBUG
{
    my $debug = $q->param('debug') || 0;
    return unless ($debug);
    my $strOrFmt = shift;
    push(@DEBUGSTRS, $strOrFmt =~ m/%/ ? sprintf($strOrFmt, @_) : $strOrFmt);
    return 1;
}

sub _dbOpen
{
    # open database
    my $dbHandle;
    my $db;
    my $error = '';
    unless (open($dbHandle, '+>>', $DBFILE))
    {
        $error = "failed opening database file: $!";
        return (undef, undef, $error);
    }
    unless (flock($dbHandle, LOCK_EX))
    {
        $error = "failed locking database file: $!";
        return (undef, undef, $error);
    }

    if (!$error)
    {

        # read database
        eval
        {
            seek($dbHandle, 0, SEEK_SET);
            local $/;
            my $dbJson = <$dbHandle>;
            $db = JSON::PP->new()->utf8()->decode($dbJson);
        };
        unless ($db)
        {
            $db = { };
        }
        $db->{_dirtiness} =  0  unless (exists $db->{_dirtiness});
        $db->{jobs}       = { } unless (exists $db->{jobs});
        $db->{config}     = { } unless (exists $db->{config});
        $db->{clients}    = { } unless (exists $db->{clients});
        $db->{cmd}        = { } unless (exists $db->{cmd});

        # add helper arrays
        my @jobIds = ();
        foreach my $jobId (sort
                         { $db->{jobs}->{$a}->{server} cmp $db->{jobs}->{$b}->{server} or
                             $db->{jobs}->{$a}->{name}   cmp $db->{jobs}->{$b}->{name}
                         } keys %{$db->{jobs}})
        {
            push(@jobIds, $jobId);
        }
        $db->{_jobIds} = \@jobIds;
        my @clientIds = ();
        my %clientIdSeen = ();
        foreach my $clientId (sort keys %{$db->{clients}}, sort keys %{$db->{config}})
        {
            if (!$clientIdSeen{$clientId})
            {
                push(@clientIds, $clientId);
            }
            $clientIdSeen{$clientId} = 1;
            if (!$db->{clients}->{$clientId})
            {
                $db->{clients}->{$clientId} = {};
            }
            if (!$db->{config}->{$clientId})
            {
                $db->{config}->{$clientId} = { jobs => [] };
            }
        }
        $db->{_clientIds} = \@clientIds;
        #DEBUG("db=%s", Dumper($db));
    }
    return ($dbHandle, $db, $error);
}

sub _dbClose
{
    my ($dbHandle, $db, $debug, $update) = @_;

    unless ($dbHandle && $db)
    {
        return;
    }
    if ($update && $db->{_dirtiness})
    {
        DEBUG("updating db, dirtiness $db->{_dirtiness}");
        delete $db->{_dirtiness};
        delete $db->{_clientIds};
        delete $db->{_jobIds};
        my $dbJson = JSON::PP->new()->utf8(1)->canonical(1)->pretty($debug ? 1 : 0)->encode($db);
        truncate($dbHandle, 0);
        seek($dbHandle, 0, SEEK_SET);
        print($dbHandle $dbJson);
    }
    close($dbHandle);
}


####################################################################################################
# status commands

sub _update
{
    my ($db, @states) = @_;
    DEBUG("_update() %i", $#states + 1);

    if ($#states < 0)
    {
        return 0, 'missing parameters';
    }

    my $ok = 1;
    my $error = '';
    foreach my $st (@states)
    {
        my $jobName  = $st->{name}   || '';
        my $server   = $st->{server} || '';
        my $jState   = $st->{state}  || '';
        my $jResult  = $st->{result} || '';
        if ($st->{server} !~ m{$SERVERNAMERE})
        {
            $error = "not a valid server name: $server";
            $ok = 0;
            last;
        }
        if ($jobName !~ m{$JOBNAMERE})
        {
            $error = "not a valid job name: $jobName";
            $ok = 0;
            last;
        }
        if ($jState && !$VALIDSTATE->{$jState})
        {
            $error = "not a valid state: $jState";
            $ok = 0;
            last;
        }
        if ($jResult && !$VALIDRESULT->{$jResult})
        {
            $error = "not a valid result: $jResult";
            $ok = 0;
            last;
        }

        my $id = substr(Digest::MD5::md5_hex("$server$jobName"), -8);
        DEBUG("_update() $server $jobName $jState $jResult $id");
        $db->{jobs}->{$id}->{ts}     = int(time() + 0.5);
        $db->{jobs}->{$id}->{name}   = $jobName;
        $db->{jobs}->{$id}->{server} = $server;
        $db->{jobs}->{$id}->{state}  //= 'unknown';
        $db->{jobs}->{$id}->{result} //= 'unknown';
        $db->{jobs}->{$id}->{state}  = $jState       if ($jState);
        $db->{jobs}->{$id}->{result} = $jResult      if ($jResult);
        $db->{_dirtiness}++;
    }
    return $ok, $error;
}


####################################################################################################
# client commands

sub _list
{
    my ($db, $offset, $limit) = @_;
    DEBUG("_list() %i %i", $offset, $limit);

    $limit = $#{$db->{_jobIds}} + 1 - $offset if (!$limit || ($limit + $offset > $#{$db->{_jobIds}}));

    my @list = ();
    for (my $ix = $offset; $ix < $offset + $limit; $ix++)
    {
        my $id = $db->{_jobIds}->[$ix];
        my $str = "$db->{jobs}->{$id}->{server}: $db->{jobs}->{$id}->{name}";
        push(@list, [ $id, $str ]);
    }

    return { res => 1, offset => $offset, limit => $limit, num => ($#list + 1), list => \@list };
}

sub _get
{
    my ($db, $job) = @_;
    DEBUG("_get() %s", $job || 'undef');

    if ($job && $db->{jobs}->{$job})
    {
        return
        {
            res    => 1,
            server => $db->{jobs}->{$job}->{server},
            name   => $db->{jobs}->{$job}->{name},
            state  => $db->{jobs}->{$job}->{state},
            result => $db->{jobs}->{$job}->{result}
        };
    }
    else
    {
        return undef, 'illegal job id';
    }
}

sub _jobs
{
    my ($db, $client, $strlen, $info) = @_;

    DEBUG("_jobs() $client $strlen");

    if ( !$client )
    {
        return undef, 'missing parameters';
    }
    my $data = { jobs => [], res => 1 };
    my $now = time();

    if (!$db->{config}->{$client}->{jobs})
    {
        $db->{config}->{$client}->{jobs} = [];
    }

    foreach my $jobId (@{$db->{config}->{$client}->{jobs}})
    {
        my $st = $db->{jobs}->{$jobId};
        if ($st)
        {
            my $state  = $st->{state};
            my $result = $st->{result};
            my $name   = $st->{name};
            my $server = $st->{server};
            my $ts     = $st->{ts};
            push(@{$data->{jobs}},
                 [ substr($name, 0, $strlen), substr($server, 0, $strlen), $state, $result, int($ts) ]);
        }
        else
        {
            push(@{$data->{jobs}}, []);
        }
    }

    $db->{clients}->{$client}->{ts} = int($now + 0.5);
    $db->{clients}->{$client}->{$_} = $info->{$_} for (keys %{$info});

    $db->{_dirtiness}++;

    return $data, '';
}

# to test:
# curl --raw -s -v -i "http://..../tschenggins-status.pl?cmd=realtime;client=...;debug=1"
sub _realtime
{
    my ($client, $strlen, $info) = @_;
    print($q->header(-type => 'text/plain', -expires => 'now', charset => 'US-ASCII'));
    my $n = 0;
    my $nHeartbeat = 0;
    my $lastTs = 0;
    my @lastStatus = ();
    my $lastConfig = 'not a possible config string';
    my $lastCheck = 0;
    my $startTs = time();
    my $debugServer = 0;
    my $doCheck = 0;
    $SIG{USR1} = sub { $doCheck = 1; };

    $0 = $info->{name} || "client$client";
    STDOUT->autoflush(1);
    print("hello $client $strlen $info->{name}\r\n");
    while (1)
    {
        sleep(1);
        my $now = time();
        my $nowInt = int($now + 0.5);
        if ( ($n % 5) == 0 )
        {
            $nHeartbeat++;
            printf("\r\nheartbeat $nowInt $nHeartbeat\r\n");
        }
        $n++;

        # don't run forever
        if ( ($now - $startTs) > (4 * 3600) )
        {
            exit(0);
        }

        # check database...
        # ...if it has changed
        my $ts = -f $DBFILE ? (stat($DBFILE))[9] : 0;
        if ($ts != $lastTs)
        {
            $doCheck = 1;
            $lastTs = $ts;
        }
        # ...and every once in a while
        elsif (($now - $lastCheck) > 10)
        {
            $doCheck = 1;
            $lastCheck = $now;
        }

        if ($doCheck)
        {
            $doCheck = 0;
            printf(STDERR "doCheck\n") if ($debugServer);
            my $sendCmd = '';

            # load database
            my ($dbHandle, $db, $error) = _dbOpen();;

            # command pending?
            if ($db->{cmd}->{$client})
            {
                $sendCmd = $db->{cmd}->{$client};
                $db->{cmd}->{$client} = '';
                $db->{_dirtiness}++;
            }

            # assume that while we're running the connection is still up
            if ($db->{clients}->{$client} &&
                (!$db->{clients}->{$client}->{check} || (($nowInt - $db->{clients}->{$client}->{check}) > 10)))
            {
                $db->{clients}->{$client}->{check} = $nowInt;
                $db->{_dirtiness}++;
            }

            # close database
            _dbClose($dbHandle, $db, 0, $error ? 0 : 1);

            # exit if we were deleted, or we're no longer in charge
            if (!$db->{clients}->{$client} ||
                ($db->{clients}->{$client}->{pid} && ($db->{clients}->{$client}->{pid} != $$)) )
            {
                printf(STDERR "client info gone\n") if ($debugServer);
                print("\r\nreconnect $nowInt\r\n");
                sleep(1);
                exit(0);
            }

            # send command?
            if ($sendCmd)
            {
                printf(STDERR "client command $sendCmd\n") if ($debugServer);
                print("\r\ncommand $nowInt $sendCmd\r\n");
            }

            # check if we're interested in any changes
            if ($db && $db->{config} && $db->{config}->{$client})
            {
                my @cfgKeys = grep { $_ ne 'jobs' } sort keys %{$db->{config}->{$client}};
                my $config = join(' ', map { "$_=$db->{config}->{$client}->{$_}" } @cfgKeys);
                if ($config ne $lastConfig)
                {
                    my %data = map { $_, $db->{config}->{$client}->{$_} } @cfgKeys;
                    my $json = JSON::PP->new()->ascii(1)->canonical(1)->pretty(0)->encode(\%data);
                    print("\r\nconfig $nowInt $json\r\n");
                    $lastConfig = $config;
                }
            }
            if ($db && $db->{clients} && $db->{clients}->{$client})
            {
                # same as cmd=jobs to get the data but we won't store the database (this updates $db->{clients}->{$client})
                my ($data, $error) = _jobs($db, $client, $strlen, $info);
                if ($error)
                {
                    print("\r\nerror $nowInt $error\r\n");
                }
                elsif ($data)
                {
                    # we send only changed jobs info
                    my @changedJobs = ();
                    # add index to results, find jobs that have changed
                    my @jobs = @{$data->{jobs}};
                    for (my $ix = 0; $ix <= $#jobs; $ix++)
                    {
                        my @job = (int($ix), @{$jobs[$ix]});
                        my $status = join(' ', map { $_ } @job); # join copy of array to avoid stringification of integers
                        if (!defined $lastStatus[$ix] || ($lastStatus[$ix] ne $status))
                        {
                            $lastStatus[$ix] = $status;
                            push(@changedJobs, \@job);
                        }
                    }
                    # send list of changed jobs
                    if ($#changedJobs > -1)
                    {
                        my $json = JSON::PP->new()->ascii(1)->canonical(1)->pretty(0)->encode(\@changedJobs);
                        print("\r\nstatus $nowInt $json\r\n");
                    }
                }
            }
        }

        # FIXME: if we just could read some data from the client here to determine if it is still alive..
        # reading from STDIN doesn't show any data... :-(
    }
}

####################################################################################################
# web interface commands

sub _set
{
    my ($db, $id, $state, $result) = @_;
    DEBUG("_set() %s %s %s", $id || 'undef', $state || 'undef', $result || 'undef');
    if (!$id || !$db->{jobs}->{$id})
    {
        return 0, 'illegal job id';
    }
    my $st = $db->{jobs}->{$id};
    $db->{jobs}->{$id}->{state}  = $state  if ($state);
    $db->{jobs}->{$id}->{result} = $result if ($result);
    $db->{jobs}->{$id}->{ts}     = int(time() + 0.5);
    $db->{_dirtiness}++;
    return 1, '';
}

sub _add
{
    my ($db, $server, $job, $state, $result) = @_;
    DEBUG("_add() %s %s %s %s", $server || 'undef', $job || 'undef', $state || 'undef', $result || 'undef');
    if (!$server || !$job || !$state || !$result)
    {
        return 0, 'missing parameters';
    }
    return _update($db, { server => $server, name => $job, state => $state, result => $result });
}

sub _del
{
    my ($db, $job) = @_;
    DEBUG("_del() %s", $job || 'undef');
    if ($db->{jobs}->{$job})
    {
        delete $db->{jobs}->{$job};
        $db->{_dirtiness}++;
        return 1, '';
    }
    else
    {
        return 0, 'illegal job id';
    }
}

sub _gui
{
    my ($db, $gui, $client) = @_;

    DEBUG("_gui() $gui $client");
    my @html = ();

    push(@html,
         $q->p({},
               'Menu: ',
               $q->a({ -href => ($q->url() . '?cmd=gui;gui=jobs') }, 'jobs'),
               ', ',
               $q->a({ -href => ($q->url() . '?cmd=gui;gui=clients') }, 'clients'),
               ', ',
               $q->a({ -href => ($q->url() . '?cmd=help') }, 'help'),
               ', ',
               $q->a({ -href => ($q->url() . '?cmd=rawdb;debug=1') }, 'raw db'),
              ),
         $q->p({},
               'Database: ' . ($ENV{'REMOTE_USER'} ? $ENV{'REMOTE_USER'} : '(default)')),
         #$q->hr(),
        );

    if ($gui eq 'jobs')
    {
        push(@html, _gui_jobs($db));
    }
    elsif ($gui eq 'clients')
    {
        push(@html, _gui_clients($db));
    }
    elsif ($client)
    {
        push(@html, _gui_client($db, $client));
    }

    return @html;
}

sub _gui_jobs
{
    my ($db) = @_;
    my @html = ();
    my $debug = $q->param('debug') || 0;

    # results
    push(@html, $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' }, $q->h2('Results'),
                        __gui_results($db, @{$db->{_jobIds}})));

    # helpers
    #my $jobSelectArgs =
    #{
    #    -name         => 'job',
    #    -values       => [ '', @{$db->{_jobIds}} ],
    #    -labels       => { '' => '<empty>', map { $_, "$db->{jobs}->{$_}->{server}: $db->{jobs}->{$_}->{name} ($db->{jobs}->{$_}->{state}, $db->{jobs}->{$_}->{result})" } @{$db->{_jobIds}} },
    #    -autocomplete => 'off',
    #    -default      => '',
    #    -size         => 10,
    #};
    my $stateSelectArgs =
    {
        -name         => 'state',
        -values       => [ '', sort { $VALIDSTATE->{$a} <=> $VALIDSTATE->{$b} } keys %{$VALIDSTATE} ],
        -autocomplete => 'off',
        -default      => '',
        -labels       => { '' => '<empty>' },
        -linebreak    => 0,
    };
    my $resultSelectArgs =
    {
        -name         => 'result',
        -values       => [ '', sort { $VALIDRESULT->{$a} <=> $VALIDRESULT->{$b} } keys %{$VALIDRESULT} ],
        -autocomplete => 'off',
        -default      => '',
        -labels       => { '' => '<empty>' },
        -linebreak    => 0,
    };
    my $jobSelectRadios = '';
    my $now = time();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        $jobSelectRadios .= '<label><input name="job" value="' . $jobId . '" autocomplete="off" type="radio"/>';
        my $age = sprintf('%.1fh', ($now - $st->{ts}) / 3600);
        $jobSelectRadios .= __gui_led($st) . " $st->{server}: $st->{name} ($age)";
        $jobSelectRadios .= '</label>';
    }

    # override
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h2('Override'),
                 $q->start_form(-method => 'GET', -action => $q->url() ),
                 $q->table(
                           $q->Tr(
                                  $q->td('job: '),
                                  $q->td({ -style => 'height: 10em; display: inline-block; overflow-y: scroll;' }, $jobSelectRadios),
                                  #$q->td($q->popup_menu($jobSelectArgs))
                                 ),
                           $q->Tr(
                                  $q->td('state: '),
                                  $q->td($q->radio_group($stateSelectArgs)),
                                 ),
                           $q->Tr(
                                  $q->td('result: '),
                                  $q->td($q->radio_group($resultSelectArgs)),
                                 ),
                           $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                         $q->submit(-value => 'override'))),

                         ),

                 ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                 $q->hidden(-name => 'cmd', -default => 'set'),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;gui=jobs'),
         $q->end_form())
   );

    # create
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h2('Create'),
                 $q->start_form(-method => 'GET', -action => $q->url() ),
                 $q->table(
                           $q->Tr(
                                  $q->td('server: '),
                                  $q->td($q->input({
                                                    -type => 'text',
                                                    -name => 'server',
                                                    -size => 20,
                                                    -autocomplete => 'off',
                                                    -default => '',
                                                   })),
                                 ),
                           $q->Tr(
                                  $q->td('job: '),
                                  $q->td($q->input({
                                                    -type => 'text',
                                                    -name => 'job',
                                                    -size => 20,
                                                    -autocomplete => 'off',
                                                    -default => '',
                                                   })),
                                 ),
                           $q->Tr(
                                  $q->td('state: '),
                                  $q->td($q->radio_group($stateSelectArgs)),
                                 ),
                           $q->Tr(
                                  $q->td('result: '),
                                  $q->td($q->radio_group($resultSelectArgs)),
                                 ),
                           $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                         $q->submit(-value => 'create')))
                          ),
                 ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                 $q->hidden(-name => 'cmd', -default => 'add'),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;gui=jobs'),
                 $q->end_form())
         );

    # delete
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h2('Delete'),
                 $q->start_form(-method => 'GET', -action => $q->url() ),
                 $q->table(
                           $q->Tr(
                                  $q->td('job: '),
                                  $q->td({ -style => 'height: 10em; display: inline-block; overflow-y: scroll;' }, $jobSelectRadios),
                                  #$q->td($q->popup_menu($jobSelectArgs)),
                                 ),
                           $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                         $q->submit(-value => 'delete info')))
                          ),
                 ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                 $q->hidden(-name => 'cmd', -default => 'del'),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;gui=jobs'),
                 $q->end_form())
        );

    return $q->div({}, @html), $q->div({ -style => 'clear: both;' });
}

sub _gui_clients
{
    my ($db) = @_;
    my @html = ();

    my @trs = ();
    foreach my $clientId (sort  @{$db->{_clientIds}})
    {
        my $now = time();
        my $client = $db->{clients}->{$clientId};
        my $config = $db->{config}->{$clientId};
        my $name     = $client->{name} || 'unknown';
        my $cfgName  = $config->{name} || 'unknown';
        my $last     = $client->{ts} ? sprintf('%.1f hours ago', ($now - $client->{ts}) / 3600.0) : 'unknown';
        my $online   = $client->{check} && (($now - $client->{check}) < 15) ? 'online' : 'offline';
        my $check    = $online eq 'online' ? int($now - $client->{check} + 0.5) : 'n/a';
        my $pid      = $client->{pid} || 'n/a';
        my $staIp    = $client->{staip} || 'unknown';
        my $staSsid  = $client->{stassid} || 'unknown';
        my $version  = $client->{version} || 'unknown';
        my $debugr = ($q->param('debug') ? '%3Bdebug%3D1' : '');
        my $debug  = ($q->param('debug') ? ';debug=1' : '');
        my $edit   = $q->a({ -href => $q->url() . '?cmd=gui;client=' . $clientId . $debug }, 'configure');

        my @leds = ();
        foreach my $jobId (@{$config->{jobs}})
        {
            if ($jobId)
            {
                push(@leds, __gui_led( $db->{jobs}->{$jobId} ));
            }
        }

        push(@trs, $q->Tr({}, $q->td({}, $clientId), $q->td({}, $name), $q->td({}, $cfgName),
                          $q->td({}, @leds),
                          $q->td({ -align => 'center' }, $last),
                          $q->td({ -class => $online, -align => 'center' }, "$pid ($check)"),
                          $q->td({ -align => 'center' }, $staIp),
               $q->td({ -align => 'center' }, $staSsid), $q->td({ -align => 'center' }, $version), $q->td({}, $edit)));
    }
    push(@html,
         $q->h2({}, 'Clients'),
         $q->table({},
                   $q->Tr({},
                          $q->th({}, 'ID'),
                          $q->th({ -colspan => 2 }, 'name'),
                          $q->th({}, 'status'),
                          $q->th({}, 'connected'),
                          $q->th({}, 'PID'),
                          $q->th({}, 'station IP'),
                          $q->th({}, 'station SSID'),
                          $q->th({}, 'version'),
                          $q->th({}, 'actions'),
                         ),
                   @trs));

    return @html;
}

sub _gui_client
{
    my ($db, $clientId) = @_;
    my $client = $db->{clients}->{$clientId};
    my $config = $db->{config}->{$clientId};
    my @html = ();
    if (!$client || !$config)
    {
        return;
    }
    my $debug = ($q->param('debug') ? ';debug=1' : '');

    push(@html, $q->h2('Client ' . $clientId . ' (' . ($config->{name} || '') . ')' ));
    push(@html, $q->p({},
        $q->a({ -href => $q->url() . '?cmd=rmclient;client=' . $clientId . ';redirect=cmd%3Dgui%3Bgui%3Dclients' . $debug },
              'delete info & config')));

    # info (and raw config)
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h3({}, 'Info'),
                 $q->table({},
                           (map { $q->Tr({}, $q->th({}, $_), $q->td({}, $client->{$_})) } sort keys %{$client}),
                          ),
                )
        );

    #$q->Tr({}, $q->th({}, 'config'), $q->td({}, $q->pre({}, $rawconfig)))

    # config: jobs
    my %labels = ();
    my $now = time();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        $labels{$jobId} =  "$st->{server}: $st->{name} ($st->{state}, $st->{result}, " . sprintf('%.1fh', ($now - $st->{ts}) / 3600) . ")";
    }
    my $jobSelectArgs =
    {
        -name         => 'jobs',
        -values       => [ '', @{$db->{_jobIds}} ],
        -labels       => \%labels,
        -autocomplete => 'off',
        -default      => '',
    };
    my @jobsTrs = ();
    my $maxch = $client->{maxch} || 0;
    for (my $ix = 0; $ix < $maxch; $ix++)
    {
        my $jobId = $config->{jobs}->[$ix] || '';
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        $jobSelectArgs->{default} = $jobId;
        push(@jobsTrs,
             $q->Tr({}, $q->td({ -align => 'right' }, $ix), $q->td({}, $jobId),
                    $q->td({}, $jobId ? __gui_led($st) : ''), $q->td({}, $q->popup_menu($jobSelectArgs)))
            );
    }
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h3({}, 'Jobs'),
                 $q->start_form(-method => 'GET', -action => $q->url() ),
                 $q->table({},
                           $q->Tr({}, $q->th({}, 'ix'), $q->th({}, 'ID'), $q->th({ -colspan => 2 }, 'job')),
                           @jobsTrs,
                           ($maxch ? $q->Tr({ }, $q->td({ -colspan => 3, -align => 'center' }, $q->submit(-value => 'apply config'))) : ''),
                          ),
                 ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                 $q->hidden(-name => 'cmd', -default => 'cfgjobs'),
                 $q->hidden(-name => 'client', -default => $clientId),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;client=' . $clientId),
                 $q->end_form()
                )
         );

    # config: LEDs
    my $modelSelectArgs =
    {
        -name         => 'model',
        -values       => [ '', 'standard', 'hello', 'gitta' ],
        -autocomplete => 'off',
        -default      => ($config->{model} || ''),
    };
    my $driverSelectArgs =
    {
        -name         => 'driver',
        -values       => [ '', 'WS2801', 'SK9822' ],
        -autocomplete => 'off',
        -default      => ($config->{driver} || ''),
    };
    my $orderSelectArgs =
    {
        -name         => 'order',
        -values       => [ '', qw(RGB RBG GRB GBR BRG BGR) ],
        -labels       => { RGB => 'RGB (red-green-blue)', RBG => 'RBG (red-blue-green)', GRB => 'GRB (green-red-blue)',
                           GBR => 'GBR (green-blue-red)', BRG => 'BRG (blue-red-green)', BGR => 'BGR (blue-green-red)' },
        -autocomplete => 'off',
        -default      => ($config->{order} || ''),
    };
    my $brightSelectArgs =
    {
        -name         => 'bright',
        -values       => [ '', qw(low medium high full) ],
        -autocomplete => 'off',
        -default      => ($config->{bright} || ''),
    };
    my $noiseSelectArgs =
    {
        -name         => 'noise',
        -values       => [ '', qw(none some more most) ],
        -autocomplete => 'off',
        -default      => ($config->{noise} || ''),
    };
    my $nameInputArgs =
    {
        -type         => 'text',
        -name         => 'name',
        -size         => 20,
        -value        => ($config->{name} || ''),
        -autocomplete => 'off',
    };
    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h3({}, 'Device'),
                 $q->start_form(-method => 'POST', -action => $q->url() ),
                 $q->table({},
                           $q->Tr({}, $q->td({}, 'Lämpli model:'), $q->td({}, $q->popup_menu($modelSelectArgs))),
                           $q->Tr({}, $q->td({}, 'LED driver:'), $q->td({}, $q->popup_menu($driverSelectArgs))),
                           $q->Tr({}, $q->td({}, 'LED colour order:'), $q->td({}, $q->popup_menu($orderSelectArgs))),
                           $q->Tr({}, $q->td({}, 'LED brightness:'), $q->td({}, $q->popup_menu($brightSelectArgs))),
                           $q->Tr({}, $q->td({}, 'noise:'), $q->td({}, $q->popup_menu($noiseSelectArgs))),
                           $q->Tr({}, $q->td({}, 'name:'), $q->td({}, $q->input($nameInputArgs))),
                           $q->Tr({ }, $q->td({ -colspan => 3, -align => 'center' }, $q->submit(-value => 'apply config'))),
                          ),
                 $q->hidden(-name => 'cmd', -default => 'cfgdevice'),
                 $q->hidden(-name => 'client', -default => $clientId),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;client=' . $clientId),
                 $q->end_form()
                )
        );


    # commands
    my $cmdSelectArgs =
    {
        -name         => 'cfgcmd',
        -values       => [ '', qw(reset reconnect identify random chewie hello dummy) ],
        -autocomplete => 'off',
        -default      => '',
    };

    push(@html,
         $q->div({ -style => 'float: left; margin: 0 0 1em 1em;' },
                 $q->h3({}, 'Command'),
                 $q->start_form(-method => 'POST', -action => $q->url() ),
                 $q->table({},
                           $q->Tr({}, $q->td({}, 'command:'), $q->td({}, $q->popup_menu($cmdSelectArgs))),
                           $q->Tr({ }, $q->td({ -colspan => 3, -align => 'center' }, $q->submit(-value => 'send command'))),
                          ),
                 $q->hidden(-name => 'cmd', -default => 'cfgcmd'),
                 $q->hidden(-name => 'client', -default => $clientId),
                 $q->hidden(-name => 'redirect', -default => 'cmd=gui;client=' . $clientId),
                 $q->end_form()
                )
        );

    return $q->div({}, @html), $q->div({ -style => 'clear: both;' });
}

sub __gui_results
{
    my ($db, @jobIds) = @_;
    my $now = time();
    my @trs = ();
    foreach my $jobId (@jobIds)
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        push(@trs, $q->Tr($q->td($jobId), $q->td($st->{server}), $q->td(__gui_led($st), $st->{name}), $q->td($st->{state}), $q->td($st->{result}),
                          $q->td({ -align => 'right' }, sprintf('%.1fh', ($now - $st->{ts}) / 3600))));
    }
    return $q->table($q->Tr($q->th('ID'), $q->th('server'), $q->th('job'), $q->th('state'), $q->th('result'), $q->th('age')),
                     @trs);
}


sub __gui_led
{
    my ($state, $html) = @_;
    #return "<div class=\"led state-$state->{state} result-$state->{result}\"></div>";
    if ($state)
    {
        return $q->div({ -title => "$state->{server}: $state->{name} ($state->{state}, $state->{result})",
                         -class => ('led state-' . $state->{state} . ' result-' . $state->{result} ) }, '');
    }
    else
    {
        return $q->div({ -class => 'led' }, '');
    }
}


__END__
