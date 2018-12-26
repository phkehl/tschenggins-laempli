#!/usr/bin/perl

=pod

=encoding utf8

=head1 tschenggins-status.pl -- Tschenggins Lämpli Backend and User Inferface

Copyright (c) 2017-2018 Philippe Kehl <flipflip at oinkzwurgl dot org>,
L<https://oinkzwurgl.org/projaeggd/tschenggins-laempli>

=head2 Usage

As a CGI script running on a web server: C<< https://..../tschenggins-status.pl?param=value;param=value;... >>

On the command line: C<< ./tschenggins-status.pl param=value param=value ... >>

=cut

use strict;
use warnings;

use feature 'state';

use CGI qw(-newstyle_urls -nosticky -tabindex -no_undef_params); # -debug
use CGI::Carp qw(fatalsToBrowser);
use JSON::PP; # run-time prefer JSON::XS, see _jsonObj()
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

The following parameters are used in the commands described below.

=over

=item * C<ascii> -- US-ASCII output (1) or UTF-8 (0, default)

=item * C<chunked> -- use "Transfer-Encoding: chunked" with given chunk size
        (default 0, i.e. not chunked), only for JSON output (e.g. C<cmd=list>)

=item * C<client> -- client ID

=item * C<cmd> -- the command

=item * C<debug> -- debugging on (1) or off (0, default), enabling will pretty-print (JSON) responses

=item * C<job> -- job ID

=item * C<limit> -- limit of list of results (default 0, i.e. all)

=item * C<maxch> -- maximum number of channels the client can handle (default 10)

=item * C<name> -- client or job name

=item * C<offset> -- offset into list of results (default 0)

=item * C<redirct> -- where to redirect to

=item * C<result> -- job result ('unknown', 'success', 'unstable', 'failure')

=item * C<server> -- server name

=item * C<state> -- job state ('unknown', 'off', 'running', 'idle')

=item * C<staip> -- client station IP address

=item * C<stassid> -- client station SSID

=item * C<strlen> -- chop long strings at length (default 256)

=item * C<version> -- client software version

=back

=cut

    # query string, application/x-www-form-urlencoded or multipart/form-data
    my $cmd      = $q->param('cmd')      || '';
    my $debug    = $q->param('debug')    || 0;
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
    }

    # application/json POST
    my $contentType = $q->content_type();
    if ( $contentType && ($contentType eq 'application/json') )
    {
        my $jsonStr = $q->param('POSTDATA');
        my $jsonObj = _jsonDecode($jsonStr);
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
    my $style = '';
    my $script = do { local $/; <DATA> };
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

This expects a application/json POST request. The C<states> array consists of objects with the
following fields: C<server>, C<name> and optionally C<state> and/or C<result>.

Use the C<tschenggins-watcher.pl> script to watch multiple Jenkins jobs on the Jenkins server, or
the C<tschenggins-update.pl> script to update single states manually or from scripts, or create your
own script, or use C<curl> (something like C<curl -H "Content-Type: application/json" --request POST  --data '{"cmd":"update","states":[{"name":"foo","result":"success","server":"ciserver","state":"idle"}]}' https://..../tschenggins-status.pl>).

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

=item B<<  C<< cmd=jobs client=<clientid> [name=<client name>] [staip=<client station IP>] [stassid=<client station SSID>] [version=<client sw version>] [strlen=<number>] [maxch=<number>] >> >>

Returns info for a client and updates client info (C<name>, C<staip>, C<stassid>, C<version>, C<maxch>).
This can be used to implement a polling client (but see C<cmd=realtime> for something better below).

=cut

    elsif ($cmd eq 'jobs')
    {
        ($data, $error) = _jobs($db, $client, $strlen,
            { name => $name, staip => $staip, stassid => $stassid, version => $version, maxch => $maxch });
    }

=pod

=item B<<  C<< cmd=realtime client=<clientid> [name=<client name>] [staip=<client station IP>] [stassid=<client station SSID>] [version=<client sw version>] [strlen=<number>] [maxch=<number>] >> >>

Returns info for a client and updates client info. This is persistent connection with real-time
update as things happen (i.e. the web server will keep sending).

The response will be something like this:

    hello 861468 256 clientname\r\n
    \r\n
    heartbeat 1545832436 1\r\n
    \r\n
    config 1545832436 {"bright":"medium","driver":"none","model":"gitta","name":"gitta_dev","noise":"some","order":"BRG"}\r\n
    \r\n
    status 1545832436 [[0,"jobname1","servername1","running","unstable",1545832418],[1,"jobname2","servername1","idle","success",1545832304],[2],[3],[4],[5],[6],[7],[8],[9]]\r\n
    \r\n
    heartbeat 1545832442 2\r\n
    \r\n
    heartbeat 1545832447 3\r\n
    \r\n
    status 1545832449 [[0,"jobname1","servername1","idle","success",1545832449]]\r\n
    \r\n
    heartbeat 1545832452 4\r\n
    .
    .
    .

The "hello" is is followed by the C<client> ID, C<strlen> and the client C<name>. The "hello", the
first "heartbeat", the "config" and the "status" are sent immediately. From then on heartbeats will
follow every 5 seconds. The status is sent as needed, i.e. as soon as something changes.

Note how the first "status" lists all configured channels (jobs) and how subsequent updates only
list the changed job(s). The C<strlen> corresponds to the maximum length of individual strings in
the JSON "config" data, not the whole response line.

To test use something like C<curl "https://..../tschenggins-status2.pl?cmd=realtime;client=...">.

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

=item B<<  C<< cmd=gui >> >>

The web interface.

=cut

    elsif ($cmd eq 'gui')
    {
        push(@html, _gui($db, $client, \$style, \$script));
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

=head2 Database

The database is a simple JSON file called C<tschenggins-status.json>. If HTTP authentication is used
(e.g. via C<.htaccess> file, or web server configuration), the file name will be C<<
tschenggins-laempli-I<user>.json >>. C<< I<user> >> will be the C<$REMOTE_USER> environment (CGI)
variable.

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
        $style .= "body { font-family: sans-serif; background-color: hsl(160, 100%, 95%); font-size: 100%; padding: 0.5em; }\n";
        $style .= "h1.title { color: #444; padding: 0; margin: 0 0 0.5em 0; }\n";
        $style .= "h1.title a { color: unset; text-decoration: none; }\n";
        $style .= "p.footer { font-size: 80%; text-align: center; color: #aaa; }\n";
        $style .= "p.footer a { color: #aaa; }\n";
        $style .= "pre.debug { color: #aaa; }\n";
        $style .= "body a.database { float: right; }\n";
        print(
              $q->header( -type => 'text/html', -expires => 'now', charset => 'UTF-8',
                          #'-Access-Control-Allow-Origin' => '*'
                        ),
              $q->start_html(-title => $TITLE,
                             -head => [ '<meta name="viewport" content="width=device-width, initial-scale=1.0"/>' ],
                             -style => { -code => $style }, -script => { -code => $script }),
              $q->a({ -class => 'database', -href => ($q->url() . '?cmd=rawdb;debug=1') }, 'DB: ' . ($ENV{'REMOTE_USER'} ? $ENV{'REMOTE_USER'} : '(default)')),
              $q->h1({ -class => 'title' }, $q->a({ -href => '?' }, $TITLE)),
              @html,
              $q->p({ -class => 'footer' }, 'Tschenggins Lämpli &mdash; Copyright &copy; 2017&ndash;2019 Philippe Kehl &amp; flipflip industries &mdash; '
                    . $q->a({ -href => 'https://oinkzwurgl.org/projeaggt/tschenggins-laempli' }, 'https://oinkzwurgl.org/projeaggt/tschenggins-laempli')
                    . ' &mdash; ' . $q->a({ -href => ($q->url() . '?cmd=help') }, 'help')
                   ),
              $q->pre({ -class => 'debug' }, $pre),
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
        my $json = _jsonEncode($data, $ascii, $debug);
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

sub _dbOpen
{
    # open database
    my $dbHandle;
    my $db;
    my $error = '';
    unless (open($dbHandle, '+>>', $DBFILE))
    {
        $error = "failed opening database file ($DBFILE): $!";
        return (undef, undef, $error);
    }
    unless (flock($dbHandle, LOCK_EX))
    {
        $error = "failed locking database file ($DBFILE): $!";
        return (undef, undef, $error);
    }

    if (!$error)
    {

        # read database
        seek($dbHandle, 0, SEEK_SET);
        my $dbJson = do { local $/; my $j = <$dbHandle>; $j };
        if ($dbJson)
        {
            $db = _jsonDecode($dbJson);
        }
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
        my $dbJson = _jsonEncode($db, 0, $debug);
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
    my $debugServer = ($q->param('debug') || 0) > 1 ? 1 : 0;
    my $doCheck = 0;
    $SIG{USR1} = sub { $doCheck = 1; };

    $0 = 'tschenggins-status.pl (' . ($info->{name} || $client) . ')';
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
            printf(STDERR "db dirty\n") if ($db->{_dirtiness} && $debugServer);
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
                    my $json = _jsonEncode(\%data, 1, 0);
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
                        my $json = _jsonEncode(\@changedJobs, 1, 0);
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
    my ($db, $client, $styleRef, $scriptRef) = @_;
    DEBUG("_gui() $client");

    my $css = '';
    #$css .= "* { margin: 0; padding: 0; }\n";
    $css .= "* { box-sizing: border-box; }\n";
    #$css .= "body, select, input { font-family: sans-serif; background-color: hsl(160, 100%, 95%); font-size: 100%; }\n";
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
    $css .= "div.led.state-unknown   { animation: flicker 1s ease-in-out 0s infinite; }\n";
    $css .= "div.led.state-off       { opacity: 0.5; }\n";
    $css .= "div.led.state-running   { animation: pulse 1s ease-in-out 0s infinite alternate; }\n";
    $css .= "div.led.state-idle      {}\n";
    $css .= "div.led.result-unknown  { background-color: hsl(  0,   0%, 50%); }\n";
    $css .= "div.led.result-success  { background-color: hsl(100, 100%, 50%); }\n";
    $css .= "div.led.result-unstable { background-color: hsl( 60, 100%, 50%); }\n";
    $css .= "div.led.result-failure  { background-color: hsl(  0, 100%, 50%); }\n";
    $css .= ".tabs { display: flex; flex-wrap: wrap; }\n";
    $css .= ".tab-input { position: absolute; opacity: 0; }\n";
    $css .= ".tab-label { with: auto; padding: 0.15em 0.25em; border: 0.1em solid #aaa; border-bottom: none; margin: 0 0.5em; color: #aaa; text-align: center; }\n";
    $css .= ".tab-label h2 { padding: 0; margin: 0; font-size: 120%; }\n";
    $css .= ".tab-label h3 { padding: 0; margin: 0; font-size: 100%; }\n";
    #$css .= ".tab-label:hover { /*background: #ff0000;*/ }\n";
    #$css .= ".tab-label:active { /*background: #ff00ff;*/ }\n";
    $css .= ".tab-input:checked + .tab-label { border-color: #000000; color: unset; font-weight: bold; }\n";
    $css .= ".tab-contents { order: 999; display: none; width: 100%; border-top: 0.15em solid #000; padding: 0.5em 1em; }\n";
    $css .= ".tab-input:checked + .tab-label + .tab-contents { display: block; } \n";
    $css .= "\n";
    $css .= "\n";
    $css .= "\n";
    $css .= "\n";

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

    $$styleRef .= $css;
    return $q->div({ -class => 'tabs' },

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-clients', -checked => 'checked' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-clients' }, $q->h2('Clients')),
                   $q->div({ -class => 'tab-contents' }, _gui_clients($db) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-config', -checked => 'checked' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-config' }, $q->h2('Config')),
                   $q->div({ -class => 'tab-contents' }, _gui_config($db) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-results' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-results' }, $q->h2('Results')),
                   $q->div({ -class => 'tab-contents' }, _gui_results($db) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-modify' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-modify' }, $q->h2('Modify')),
                   $q->div({ -class => 'tab-contents' }, _gui_modify($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-create' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-create' }, $q->h2('Create')),
                   $q->div({ -class => 'tab-contents' }, _gui_create($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-delete' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-delete' }, $q->h2('Delete')),
                   $q->div({ -class => 'tab-contents' }, _gui_delete($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) ),
                  );
}

sub _gui_results
{
    my ($db) = @_;
    my @html = ();

    my $now = time();
    my @trs = ();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        push(@trs, $q->Tr($q->td($jobId), $q->td($st->{server}), $q->td(__gui_led($st), $st->{name}), $q->td($st->{state}), $q->td($st->{result}),
                          $q->td({ -align => 'right' }, sprintf('%.1fh', ($now - $st->{ts}) / 3600))));
    }
    return $q->table($q->Tr($q->th('ID'), $q->th('server'), $q->th('job'), $q->th('state'), $q->th('result'), $q->th('age')), @trs);
}

sub _gui_modify
{
    my ($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) = @_;
    my $debug = $q->param('debug') || 0;

    return (
            $q->start_form(-method => 'GET', -action => $q->url() ),
            $q->table(
                      $q->Tr(
                             $q->td('job: '),
                             $q->td({ -style => 'height: 20em; display: inline-block; overflow-y: scroll;' }, $jobSelectRadios),
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
            $q->end_form()
           );
}

sub _gui_create
{
    my ($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) = @_;
    my $debug = $q->param('debug') || 0;

    return (
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
            $q->end_form()
           );
}

sub _gui_delete
{
    my ($db, $stateSelectArgs, $resultSelectArgs, $jobSelectRadios) = @_;
    my $debug = $q->param('debug') || 0;

    return (
            $q->start_form(-method => 'GET', -action => $q->url() ),
            $q->table(
                      $q->Tr(
                             $q->td('job: '),
                             $q->td({ -style => 'height: 20em; display: inline-block; overflow-y: scroll;' }, $jobSelectRadios),
                             #$q->td($q->popup_menu($jobSelectArgs)),
                            ),
                      $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                    $q->submit(-value => 'delete info')))
                     ),
            ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
            $q->hidden(-name => 'cmd', -default => 'del'),
            $q->hidden(-name => 'redirect', -default => 'cmd=gui;gui=jobs'),
            $q->end_form()
           );
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

sub _gui_config
{
    my ($db) = @_;

    my @tabs = ();

    foreach my $clientId (sort  @{$db->{_clientIds}})
    {
        my $name = $db->{config}->{$clientId}->{name} || $clientId;

        push(@tabs,
                   $q->input({ -class => 'tab-input', -name => 'tabs-config', type => 'radio', id => "tab-config-$clientId" }),
                   $q->label({ -class => 'tab-label', -for => "tab-config-$clientId" }, $q->h3($name)),
                   $q->div({ -class => 'tab-contents' }, __gui_config_client($db, $clientId) ),
            );
    }

    # common job select popup menu
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
        -style        => 'display: none;',
        -id           => 'jobSelectPopup'
    };

    return $q->div({ -class => 'tabs' }, @tabs, $q->popup_menu($jobSelectArgs));
}

sub __gui_config_client
{
    my ($db, $clientId) = @_;
    my $client = $db->{clients}->{$clientId};
    my $config = $db->{config}->{$clientId};
    if (!$client || !$config)
    {
        return $q->p('nothing here :-(');
    }
    my $debug = ($q->param('debug') ? ';debug=1' : '');

    # info (and raw config)
    my $htmlInfo =
      $q->div({ },
              $q->table({},
                        (map { $q->Tr({}, $q->th({}, $_), $q->td({}, $client->{$_})) } sort keys %{$client}),
                       ),
             );

    #$q->Tr({}, $q->th({}, 'config'), $q->td({}, $q->pre({}, $rawconfig)))

    # config: jobs
    my @jobsTrs = ();
    my $maxch = $client->{maxch} || 0;
    for (my $ix = 0; $ix < $maxch; $ix++)
    {
        my $jobId = $config->{jobs}->[$ix] || '';
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        push(@jobsTrs, # Note: the job selection popup menu is populated run-time (JS), since $q->popup_menu() is very expensive
             $q->Tr({}, $q->td({ -align => 'right' }, $ix), $q->td({}, $jobId),
                    $q->td({}, $jobId ? __gui_led($st) : ''), $q->td({ -class => 'jobSelectPopup'}, $jobId))
            );
    }
    my $htmlJobs =
      $q->div({ },
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
        -values       => [ '', 'none', 'WS2801', 'SK9822' ],
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
    my $htmlDevice =
      $q->div({  },
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
             );

    # commands
    my $cmdSelectArgs =
    {
        -name         => 'cfgcmd',
        -values       => [ '', qw(reset reconnect identify random chewie hello dummy) ],
        -autocomplete => 'off',
        -default      => '',
    };
    my $htmlCommands =
      $q->div({  },
              $q->start_form(-method => 'POST', -action => $q->url() ),
              $q->table({},
                        $q->Tr({}, $q->td({}, 'command:'), $q->td({}, $q->popup_menu($cmdSelectArgs))),
                        $q->Tr({ }, $q->td({ -colspan => 2, -align => 'center' }, $q->submit(-value => 'send command'))),
                       ),
              $q->hidden(-name => 'cmd', -default => 'cfgcmd'),
              $q->hidden(-name => 'client', -default => $clientId),
              $q->hidden(-name => 'redirect', -default => 'cmd=gui;client=' . $clientId),
              $q->end_form()
             );


    # delete info
    my $htmlDelete =
      $q->div({  },
              $q->start_form(-method => 'POST', -action => $q->url() ),
              $q->table({},
                        $q->Tr({ }, $q->td({ -align => 'center' }, $q->submit(-value => 'delete info & config'))),
                       ),
              $q->hidden(-name => 'cmd', -default => 'rmclient'),
              $q->hidden(-name => 'client', -default => $clientId),
              $q->hidden(-name => 'redirect', -default => 'cmd=gui;client=' . $clientId . $debug),
              $q->end_form()
             );

    return (
            $q->div({ -style => 'display: inline-block; vertical-align: top;' }, $htmlInfo, $q->br(), $htmlDevice, $q->br(), $htmlCommands, $q->br(), $htmlDelete, $q->br() ),
            $q->div({ -style => 'display: inline-block; vertical-align: top;' }, $htmlJobs),
           );
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


####################################################################################################
# funky functions

sub DEBUG
{
    my $debug = $q->param('debug') || 0;
    return unless ($debug);
    my $strOrFmt = shift;
    push(@DEBUGSTRS, $strOrFmt =~ m/%/ ? sprintf($strOrFmt, @_) : $strOrFmt);
    return 1;
}

sub _jsonObj
{
    state $jsonObj;
    if (!$jsonObj)
    {
        # try JSON::XS
        eval
        {
            local $^W = 0;
            local $SIG{__DIE__} = 'IGNORE';
            local $SIG{__WARN__} = 'IGNORE';
            require JSON::XS;
            $jsonObj = JSON::XS->new();
        };
        # fall back to JSON:PP
        if (!$jsonObj)
        {
            print(STDERR "Falling back to JSON::PP\n");
            $jsonObj = JSON::PP->new();
        }
    }
    return $jsonObj;
}

sub _jsonEncode
{
    my ($data, $ascii, $pretty) = @_;
    $ascii //= 0;
    $pretty //= 0;
    my $json = _jsonObj()->ascii($ascii ? 1 : 0)->utf8($ascii ? 0 : 1)->canonical(1)->pretty($pretty ? 1 : 0)->encode($data);
    return $json;
}


sub _jsonDecode
{
    my ($json) = @_;
    my $jsonObj = JSON::PP->new();
    my $data;
    eval
    {
        local $^W;
        local $SIG{__DIE__} = 'IGNORE';
        $data = _jsonObj()->utf8()->decode($json);
    };
    return $data;
}

__DATA__
/* Zepto v1.2.0 - zepto event ajax form ie - zeptojs.com/license */
(function(global, factory) {
  if (typeof define === 'function' && define.amd)
    define(function() { return factory(global) })
  else
    factory(global)
}(this, function(window) {
  var Zepto = (function() {
  var undefined, key, $, classList, emptyArray = [], concat = emptyArray.concat, filter = emptyArray.filter, slice = emptyArray.slice,
    document = window.document,
    elementDisplay = {}, classCache = {},
    cssNumber = { 'column-count': 1, 'columns': 1, 'font-weight': 1, 'line-height': 1,'opacity': 1, 'z-index': 1, 'zoom': 1 },
    fragmentRE = /^\s*<(\w+|!)[^>]*>/,
    singleTagRE = /^<(\w+)\s*\/?>(?:<\/\1>|)$/,
    tagExpanderRE = /<(?!area|br|col|embed|hr|img|input|link|meta|param)(([\w:]+)[^>]*)\/>/ig,
    rootNodeRE = /^(?:body|html)$/i,
    capitalRE = /([A-Z])/g,

    // special attributes that should be get/set via method calls
    methodAttributes = ['val', 'css', 'html', 'text', 'data', 'width', 'height', 'offset'],

    adjacencyOperators = [ 'after', 'prepend', 'before', 'append' ],
    table = document.createElement('table'),
    tableRow = document.createElement('tr'),
    containers = {
      'tr': document.createElement('tbody'),
      'tbody': table, 'thead': table, 'tfoot': table,
      'td': tableRow, 'th': tableRow,
      '*': document.createElement('div')
    },
    readyRE = /complete|loaded|interactive/,
    simpleSelectorRE = /^[\w-]*$/,
    class2type = {},
    toString = class2type.toString,
    zepto = {},
    camelize, uniq,
    tempParent = document.createElement('div'),
    propMap = {
      'tabindex': 'tabIndex',
      'readonly': 'readOnly',
      'for': 'htmlFor',
      'class': 'className',
      'maxlength': 'maxLength',
      'cellspacing': 'cellSpacing',
      'cellpadding': 'cellPadding',
      'rowspan': 'rowSpan',
      'colspan': 'colSpan',
      'usemap': 'useMap',
      'frameborder': 'frameBorder',
      'contenteditable': 'contentEditable'
    },
    isArray = Array.isArray ||
      function(object){ return object instanceof Array }

  zepto.matches = function(element, selector) {
    if (!selector || !element || element.nodeType !== 1) return false
    var matchesSelector = element.matches || element.webkitMatchesSelector ||
                          element.mozMatchesSelector || element.oMatchesSelector ||
                          element.matchesSelector
    if (matchesSelector) return matchesSelector.call(element, selector)
    // fall back to performing a selector:
    var match, parent = element.parentNode, temp = !parent
    if (temp) (parent = tempParent).appendChild(element)
    match = ~zepto.qsa(parent, selector).indexOf(element)
    temp && tempParent.removeChild(element)
    return match
  }

  function type(obj) {
    return obj == null ? String(obj) :
      class2type[toString.call(obj)] || "object"
  }

  function isFunction(value) { return type(value) == "function" }
  function isWindow(obj)     { return obj != null && obj == obj.window }
  function isDocument(obj)   { return obj != null && obj.nodeType == obj.DOCUMENT_NODE }
  function isObject(obj)     { return type(obj) == "object" }
  function isPlainObject(obj) {
    return isObject(obj) && !isWindow(obj) && Object.getPrototypeOf(obj) == Object.prototype
  }

  function likeArray(obj) {
    var length = !!obj && 'length' in obj && obj.length,
      type = $.type(obj)

    return 'function' != type && !isWindow(obj) && (
      'array' == type || length === 0 ||
        (typeof length == 'number' && length > 0 && (length - 1) in obj)
    )
  }

  function compact(array) { return filter.call(array, function(item){ return item != null }) }
  function flatten(array) { return array.length > 0 ? $.fn.concat.apply([], array) : array }
  camelize = function(str){ return str.replace(/-+(.)?/g, function(match, chr){ return chr ? chr.toUpperCase() : '' }) }
  function dasherize(str) {
    return str.replace(/::/g, '/')
           .replace(/([A-Z]+)([A-Z][a-z])/g, '$1_$2')
           .replace(/([a-z\d])([A-Z])/g, '$1_$2')
           .replace(/_/g, '-')
           .toLowerCase()
  }
  uniq = function(array){ return filter.call(array, function(item, idx){ return array.indexOf(item) == idx }) }

  function classRE(name) {
    return name in classCache ?
      classCache[name] : (classCache[name] = new RegExp('(^|\\s)' + name + '(\\s|$)'))
  }

  function maybeAddPx(name, value) {
    return (typeof value == "number" && !cssNumber[dasherize(name)]) ? value + "px" : value
  }

  function defaultDisplay(nodeName) {
    var element, display
    if (!elementDisplay[nodeName]) {
      element = document.createElement(nodeName)
      document.body.appendChild(element)
      display = getComputedStyle(element, '').getPropertyValue("display")
      element.parentNode.removeChild(element)
      display == "none" && (display = "block")
      elementDisplay[nodeName] = display
    }
    return elementDisplay[nodeName]
  }

  function children(element) {
    return 'children' in element ?
      slice.call(element.children) :
      $.map(element.childNodes, function(node){ if (node.nodeType == 1) return node })
  }

  function Z(dom, selector) {
    var i, len = dom ? dom.length : 0
    for (i = 0; i < len; i++) this[i] = dom[i]
    this.length = len
    this.selector = selector || ''
  }

  // `$.zepto.fragment` takes a html string and an optional tag name
  // to generate DOM nodes from the given html string.
  // The generated DOM nodes are returned as an array.
  // This function can be overridden in plugins for example to make
  // it compatible with browsers that don't support the DOM fully.
  zepto.fragment = function(html, name, properties) {
    var dom, nodes, container

    // A special case optimization for a single tag
    if (singleTagRE.test(html)) dom = $(document.createElement(RegExp.$1))

    if (!dom) {
      if (html.replace) html = html.replace(tagExpanderRE, "<$1></$2>")
      if (name === undefined) name = fragmentRE.test(html) && RegExp.$1
      if (!(name in containers)) name = '*'

      container = containers[name]
      container.innerHTML = '' + html
      dom = $.each(slice.call(container.childNodes), function(){
        container.removeChild(this)
      })
    }

    if (isPlainObject(properties)) {
      nodes = $(dom)
      $.each(properties, function(key, value) {
        if (methodAttributes.indexOf(key) > -1) nodes[key](value)
        else nodes.attr(key, value)
      })
    }

    return dom
  }

  // `$.zepto.Z` swaps out the prototype of the given `dom` array
  // of nodes with `$.fn` and thus supplying all the Zepto functions
  // to the array. This method can be overridden in plugins.
  zepto.Z = function(dom, selector) {
    return new Z(dom, selector)
  }

  // `$.zepto.isZ` should return `true` if the given object is a Zepto
  // collection. This method can be overridden in plugins.
  zepto.isZ = function(object) {
    return object instanceof zepto.Z
  }

  // `$.zepto.init` is Zepto's counterpart to jQuery's `$.fn.init` and
  // takes a CSS selector and an optional context (and handles various
  // special cases).
  // This method can be overridden in plugins.
  zepto.init = function(selector, context) {
    var dom
    // If nothing given, return an empty Zepto collection
    if (!selector) return zepto.Z()
    // Optimize for string selectors
    else if (typeof selector == 'string') {
      selector = selector.trim()
      // If it's a html fragment, create nodes from it
      // Note: In both Chrome 21 and Firefox 15, DOM error 12
      // is thrown if the fragment doesn't begin with <
      if (selector[0] == '<' && fragmentRE.test(selector))
        dom = zepto.fragment(selector, RegExp.$1, context), selector = null
      // If there's a context, create a collection on that context first, and select
      // nodes from there
      else if (context !== undefined) return $(context).find(selector)
      // If it's a CSS selector, use it to select nodes.
      else dom = zepto.qsa(document, selector)
    }
    // If a function is given, call it when the DOM is ready
    else if (isFunction(selector)) return $(document).ready(selector)
    // If a Zepto collection is given, just return it
    else if (zepto.isZ(selector)) return selector
    else {
      // normalize array if an array of nodes is given
      if (isArray(selector)) dom = compact(selector)
      // Wrap DOM nodes.
      else if (isObject(selector))
        dom = [selector], selector = null
      // If it's a html fragment, create nodes from it
      else if (fragmentRE.test(selector))
        dom = zepto.fragment(selector.trim(), RegExp.$1, context), selector = null
      // If there's a context, create a collection on that context first, and select
      // nodes from there
      else if (context !== undefined) return $(context).find(selector)
      // And last but no least, if it's a CSS selector, use it to select nodes.
      else dom = zepto.qsa(document, selector)
    }
    // create a new Zepto collection from the nodes found
    return zepto.Z(dom, selector)
  }

  // `$` will be the base `Zepto` object. When calling this
  // function just call `$.zepto.init, which makes the implementation
  // details of selecting nodes and creating Zepto collections
  // patchable in plugins.
  $ = function(selector, context){
    return zepto.init(selector, context)
  }

  function extend(target, source, deep) {
    for (key in source)
      if (deep && (isPlainObject(source[key]) || isArray(source[key]))) {
        if (isPlainObject(source[key]) && !isPlainObject(target[key]))
          target[key] = {}
        if (isArray(source[key]) && !isArray(target[key]))
          target[key] = []
        extend(target[key], source[key], deep)
      }
      else if (source[key] !== undefined) target[key] = source[key]
  }

  // Copy all but undefined properties from one or more
  // objects to the `target` object.
  $.extend = function(target){
    var deep, args = slice.call(arguments, 1)
    if (typeof target == 'boolean') {
      deep = target
      target = args.shift()
    }
    args.forEach(function(arg){ extend(target, arg, deep) })
    return target
  }

  // `$.zepto.qsa` is Zepto's CSS selector implementation which
  // uses `document.querySelectorAll` and optimizes for some special cases, like `#id`.
  // This method can be overridden in plugins.
  zepto.qsa = function(element, selector){
    var found,
        maybeID = selector[0] == '#',
        maybeClass = !maybeID && selector[0] == '.',
        nameOnly = maybeID || maybeClass ? selector.slice(1) : selector, // Ensure that a 1 char tag name still gets checked
        isSimple = simpleSelectorRE.test(nameOnly)
    return (element.getElementById && isSimple && maybeID) ? // Safari DocumentFragment doesn't have getElementById
      ( (found = element.getElementById(nameOnly)) ? [found] : [] ) :
      (element.nodeType !== 1 && element.nodeType !== 9 && element.nodeType !== 11) ? [] :
      slice.call(
        isSimple && !maybeID && element.getElementsByClassName ? // DocumentFragment doesn't have getElementsByClassName/TagName
          maybeClass ? element.getElementsByClassName(nameOnly) : // If it's simple, it could be a class
          element.getElementsByTagName(selector) : // Or a tag
          element.querySelectorAll(selector) // Or it's not simple, and we need to query all
      )
  }

  function filtered(nodes, selector) {
    return selector == null ? $(nodes) : $(nodes).filter(selector)
  }

  $.contains = document.documentElement.contains ?
    function(parent, node) {
      return parent !== node && parent.contains(node)
    } :
    function(parent, node) {
      while (node && (node = node.parentNode))
        if (node === parent) return true
      return false
    }

  function funcArg(context, arg, idx, payload) {
    return isFunction(arg) ? arg.call(context, idx, payload) : arg
  }

  function setAttribute(node, name, value) {
    value == null ? node.removeAttribute(name) : node.setAttribute(name, value)
  }

  // access className property while respecting SVGAnimatedString
  function className(node, value){
    var klass = node.className || '',
        svg   = klass && klass.baseVal !== undefined

    if (value === undefined) return svg ? klass.baseVal : klass
    svg ? (klass.baseVal = value) : (node.className = value)
  }

  // "true"  => true
  // "false" => false
  // "null"  => null
  // "42"    => 42
  // "42.5"  => 42.5
  // "08"    => "08"
  // JSON    => parse if valid
  // String  => self
  function deserializeValue(value) {
    try {
      return value ?
        value == "true" ||
        ( value == "false" ? false :
          value == "null" ? null :
          +value + "" == value ? +value :
          /^[\[\{]/.test(value) ? $.parseJSON(value) :
          value )
        : value
    } catch(e) {
      return value
    }
  }

  $.type = type
  $.isFunction = isFunction
  $.isWindow = isWindow
  $.isArray = isArray
  $.isPlainObject = isPlainObject

  $.isEmptyObject = function(obj) {
    var name
    for (name in obj) return false
    return true
  }

  $.isNumeric = function(val) {
    var num = Number(val), type = typeof val
    return val != null && type != 'boolean' &&
      (type != 'string' || val.length) &&
      !isNaN(num) && isFinite(num) || false
  }

  $.inArray = function(elem, array, i){
    return emptyArray.indexOf.call(array, elem, i)
  }

  $.camelCase = camelize
  $.trim = function(str) {
    return str == null ? "" : String.prototype.trim.call(str)
  }

  // plugin compatibility
  $.uuid = 0
  $.support = { }
  $.expr = { }
  $.noop = function() {}

  $.map = function(elements, callback){
    var value, values = [], i, key
    if (likeArray(elements))
      for (i = 0; i < elements.length; i++) {
        value = callback(elements[i], i)
        if (value != null) values.push(value)
      }
    else
      for (key in elements) {
        value = callback(elements[key], key)
        if (value != null) values.push(value)
      }
    return flatten(values)
  }

  $.each = function(elements, callback){
    var i, key
    if (likeArray(elements)) {
      for (i = 0; i < elements.length; i++)
        if (callback.call(elements[i], i, elements[i]) === false) return elements
    } else {
      for (key in elements)
        if (callback.call(elements[key], key, elements[key]) === false) return elements
    }

    return elements
  }

  $.grep = function(elements, callback){
    return filter.call(elements, callback)
  }

  if (window.JSON) $.parseJSON = JSON.parse

  // Populate the class2type map
  $.each("Boolean Number String Function Array Date RegExp Object Error".split(" "), function(i, name) {
    class2type[ "[object " + name + "]" ] = name.toLowerCase()
  })

  // Define methods that will be available on all
  // Zepto collections
  $.fn = {
    constructor: zepto.Z,
    length: 0,

    // Because a collection acts like an array
    // copy over these useful array functions.
    forEach: emptyArray.forEach,
    reduce: emptyArray.reduce,
    push: emptyArray.push,
    sort: emptyArray.sort,
    splice: emptyArray.splice,
    indexOf: emptyArray.indexOf,
    concat: function(){
      var i, value, args = []
      for (i = 0; i < arguments.length; i++) {
        value = arguments[i]
        args[i] = zepto.isZ(value) ? value.toArray() : value
      }
      return concat.apply(zepto.isZ(this) ? this.toArray() : this, args)
    },

    // `map` and `slice` in the jQuery API work differently
    // from their array counterparts
    map: function(fn){
      return $($.map(this, function(el, i){ return fn.call(el, i, el) }))
    },
    slice: function(){
      return $(slice.apply(this, arguments))
    },

    ready: function(callback){
      // need to check if document.body exists for IE as that browser reports
      // document ready when it hasn't yet created the body element
      if (readyRE.test(document.readyState) && document.body) callback($)
      else document.addEventListener('DOMContentLoaded', function(){ callback($) }, false)
      return this
    },
    get: function(idx){
      return idx === undefined ? slice.call(this) : this[idx >= 0 ? idx : idx + this.length]
    },
    toArray: function(){ return this.get() },
    size: function(){
      return this.length
    },
    remove: function(){
      return this.each(function(){
        if (this.parentNode != null)
          this.parentNode.removeChild(this)
      })
    },
    each: function(callback){
      emptyArray.every.call(this, function(el, idx){
        return callback.call(el, idx, el) !== false
      })
      return this
    },
    filter: function(selector){
      if (isFunction(selector)) return this.not(this.not(selector))
      return $(filter.call(this, function(element){
        return zepto.matches(element, selector)
      }))
    },
    add: function(selector,context){
      return $(uniq(this.concat($(selector,context))))
    },
    is: function(selector){
      return this.length > 0 && zepto.matches(this[0], selector)
    },
    not: function(selector){
      var nodes=[]
      if (isFunction(selector) && selector.call !== undefined)
        this.each(function(idx){
          if (!selector.call(this,idx)) nodes.push(this)
        })
      else {
        var excludes = typeof selector == 'string' ? this.filter(selector) :
          (likeArray(selector) && isFunction(selector.item)) ? slice.call(selector) : $(selector)
        this.forEach(function(el){
          if (excludes.indexOf(el) < 0) nodes.push(el)
        })
      }
      return $(nodes)
    },
    has: function(selector){
      return this.filter(function(){
        return isObject(selector) ?
          $.contains(this, selector) :
          $(this).find(selector).size()
      })
    },
    eq: function(idx){
      return idx === -1 ? this.slice(idx) : this.slice(idx, + idx + 1)
    },
    first: function(){
      var el = this[0]
      return el && !isObject(el) ? el : $(el)
    },
    last: function(){
      var el = this[this.length - 1]
      return el && !isObject(el) ? el : $(el)
    },
    find: function(selector){
      var result, $this = this
      if (!selector) result = $()
      else if (typeof selector == 'object')
        result = $(selector).filter(function(){
          var node = this
          return emptyArray.some.call($this, function(parent){
            return $.contains(parent, node)
          })
        })
      else if (this.length == 1) result = $(zepto.qsa(this[0], selector))
      else result = this.map(function(){ return zepto.qsa(this, selector) })
      return result
    },
    closest: function(selector, context){
      var nodes = [], collection = typeof selector == 'object' && $(selector)
      this.each(function(_, node){
        while (node && !(collection ? collection.indexOf(node) >= 0 : zepto.matches(node, selector)))
          node = node !== context && !isDocument(node) && node.parentNode
        if (node && nodes.indexOf(node) < 0) nodes.push(node)
      })
      return $(nodes)
    },
    parents: function(selector){
      var ancestors = [], nodes = this
      while (nodes.length > 0)
        nodes = $.map(nodes, function(node){
          if ((node = node.parentNode) && !isDocument(node) && ancestors.indexOf(node) < 0) {
            ancestors.push(node)
            return node
          }
        })
      return filtered(ancestors, selector)
    },
    parent: function(selector){
      return filtered(uniq(this.pluck('parentNode')), selector)
    },
    children: function(selector){
      return filtered(this.map(function(){ return children(this) }), selector)
    },
    contents: function() {
      return this.map(function() { return this.contentDocument || slice.call(this.childNodes) })
    },
    siblings: function(selector){
      return filtered(this.map(function(i, el){
        return filter.call(children(el.parentNode), function(child){ return child!==el })
      }), selector)
    },
    empty: function(){
      return this.each(function(){ this.innerHTML = '' })
    },
    // `pluck` is borrowed from Prototype.js
    pluck: function(property){
      return $.map(this, function(el){ return el[property] })
    },
    show: function(){
      return this.each(function(){
        this.style.display == "none" && (this.style.display = '')
        if (getComputedStyle(this, '').getPropertyValue("display") == "none")
          this.style.display = defaultDisplay(this.nodeName)
      })
    },
    replaceWith: function(newContent){
      return this.before(newContent).remove()
    },
    wrap: function(structure){
      var func = isFunction(structure)
      if (this[0] && !func)
        var dom   = $(structure).get(0),
            clone = dom.parentNode || this.length > 1

      return this.each(function(index){
        $(this).wrapAll(
          func ? structure.call(this, index) :
            clone ? dom.cloneNode(true) : dom
        )
      })
    },
    wrapAll: function(structure){
      if (this[0]) {
        $(this[0]).before(structure = $(structure))
        var children
        // drill down to the inmost element
        while ((children = structure.children()).length) structure = children.first()
        $(structure).append(this)
      }
      return this
    },
    wrapInner: function(structure){
      var func = isFunction(structure)
      return this.each(function(index){
        var self = $(this), contents = self.contents(),
            dom  = func ? structure.call(this, index) : structure
        contents.length ? contents.wrapAll(dom) : self.append(dom)
      })
    },
    unwrap: function(){
      this.parent().each(function(){
        $(this).replaceWith($(this).children())
      })
      return this
    },
    clone: function(){
      return this.map(function(){ return this.cloneNode(true) })
    },
    hide: function(){
      return this.css("display", "none")
    },
    toggle: function(setting){
      return this.each(function(){
        var el = $(this)
        ;(setting === undefined ? el.css("display") == "none" : setting) ? el.show() : el.hide()
      })
    },
    prev: function(selector){ return $(this.pluck('previousElementSibling')).filter(selector || '*') },
    next: function(selector){ return $(this.pluck('nextElementSibling')).filter(selector || '*') },
    html: function(html){
      return 0 in arguments ?
        this.each(function(idx){
          var originHtml = this.innerHTML
          $(this).empty().append( funcArg(this, html, idx, originHtml) )
        }) :
        (0 in this ? this[0].innerHTML : null)
    },
    text: function(text){
      return 0 in arguments ?
        this.each(function(idx){
          var newText = funcArg(this, text, idx, this.textContent)
          this.textContent = newText == null ? '' : ''+newText
        }) :
        (0 in this ? this.pluck('textContent').join("") : null)
    },
    attr: function(name, value){
      var result
      return (typeof name == 'string' && !(1 in arguments)) ?
        (0 in this && this[0].nodeType == 1 && (result = this[0].getAttribute(name)) != null ? result : undefined) :
        this.each(function(idx){
          if (this.nodeType !== 1) return
          if (isObject(name)) for (key in name) setAttribute(this, key, name[key])
          else setAttribute(this, name, funcArg(this, value, idx, this.getAttribute(name)))
        })
    },
    removeAttr: function(name){
      return this.each(function(){ this.nodeType === 1 && name.split(' ').forEach(function(attribute){
        setAttribute(this, attribute)
      }, this)})
    },
    prop: function(name, value){
      name = propMap[name] || name
      return (1 in arguments) ?
        this.each(function(idx){
          this[name] = funcArg(this, value, idx, this[name])
        }) :
        (this[0] && this[0][name])
    },
    removeProp: function(name){
      name = propMap[name] || name
      return this.each(function(){ delete this[name] })
    },
    data: function(name, value){
      var attrName = 'data-' + name.replace(capitalRE, '-$1').toLowerCase()

      var data = (1 in arguments) ?
        this.attr(attrName, value) :
        this.attr(attrName)

      return data !== null ? deserializeValue(data) : undefined
    },
    val: function(value){
      if (0 in arguments) {
        if (value == null) value = ""
        return this.each(function(idx){
          this.value = funcArg(this, value, idx, this.value)
        })
      } else {
        return this[0] && (this[0].multiple ?
           $(this[0]).find('option').filter(function(){ return this.selected }).pluck('value') :
           this[0].value)
      }
    },
    offset: function(coordinates){
      if (coordinates) return this.each(function(index){
        var $this = $(this),
            coords = funcArg(this, coordinates, index, $this.offset()),
            parentOffset = $this.offsetParent().offset(),
            props = {
              top:  coords.top  - parentOffset.top,
              left: coords.left - parentOffset.left
            }

        if ($this.css('position') == 'static') props['position'] = 'relative'
        $this.css(props)
      })
      if (!this.length) return null
      if (document.documentElement !== this[0] && !$.contains(document.documentElement, this[0]))
        return {top: 0, left: 0}
      var obj = this[0].getBoundingClientRect()
      return {
        left: obj.left + window.pageXOffset,
        top: obj.top + window.pageYOffset,
        width: Math.round(obj.width),
        height: Math.round(obj.height)
      }
    },
    css: function(property, value){
      if (arguments.length < 2) {
        var element = this[0]
        if (typeof property == 'string') {
          if (!element) return
          return element.style[camelize(property)] || getComputedStyle(element, '').getPropertyValue(property)
        } else if (isArray(property)) {
          if (!element) return
          var props = {}
          var computedStyle = getComputedStyle(element, '')
          $.each(property, function(_, prop){
            props[prop] = (element.style[camelize(prop)] || computedStyle.getPropertyValue(prop))
          })
          return props
        }
      }

      var css = ''
      if (type(property) == 'string') {
        if (!value && value !== 0)
          this.each(function(){ this.style.removeProperty(dasherize(property)) })
        else
          css = dasherize(property) + ":" + maybeAddPx(property, value)
      } else {
        for (key in property)
          if (!property[key] && property[key] !== 0)
            this.each(function(){ this.style.removeProperty(dasherize(key)) })
          else
            css += dasherize(key) + ':' + maybeAddPx(key, property[key]) + ';'
      }

      return this.each(function(){ this.style.cssText += ';' + css })
    },
    index: function(element){
      return element ? this.indexOf($(element)[0]) : this.parent().children().indexOf(this[0])
    },
    hasClass: function(name){
      if (!name) return false
      return emptyArray.some.call(this, function(el){
        return this.test(className(el))
      }, classRE(name))
    },
    addClass: function(name){
      if (!name) return this
      return this.each(function(idx){
        if (!('className' in this)) return
        classList = []
        var cls = className(this), newName = funcArg(this, name, idx, cls)
        newName.split(/\s+/g).forEach(function(klass){
          if (!$(this).hasClass(klass)) classList.push(klass)
        }, this)
        classList.length && className(this, cls + (cls ? " " : "") + classList.join(" "))
      })
    },
    removeClass: function(name){
      return this.each(function(idx){
        if (!('className' in this)) return
        if (name === undefined) return className(this, '')
        classList = className(this)
        funcArg(this, name, idx, classList).split(/\s+/g).forEach(function(klass){
          classList = classList.replace(classRE(klass), " ")
        })
        className(this, classList.trim())
      })
    },
    toggleClass: function(name, when){
      if (!name) return this
      return this.each(function(idx){
        var $this = $(this), names = funcArg(this, name, idx, className(this))
        names.split(/\s+/g).forEach(function(klass){
          (when === undefined ? !$this.hasClass(klass) : when) ?
            $this.addClass(klass) : $this.removeClass(klass)
        })
      })
    },
    scrollTop: function(value){
      if (!this.length) return
      var hasScrollTop = 'scrollTop' in this[0]
      if (value === undefined) return hasScrollTop ? this[0].scrollTop : this[0].pageYOffset
      return this.each(hasScrollTop ?
        function(){ this.scrollTop = value } :
        function(){ this.scrollTo(this.scrollX, value) })
    },
    scrollLeft: function(value){
      if (!this.length) return
      var hasScrollLeft = 'scrollLeft' in this[0]
      if (value === undefined) return hasScrollLeft ? this[0].scrollLeft : this[0].pageXOffset
      return this.each(hasScrollLeft ?
        function(){ this.scrollLeft = value } :
        function(){ this.scrollTo(value, this.scrollY) })
    },
    position: function() {
      if (!this.length) return

      var elem = this[0],
        // Get *real* offsetParent
        offsetParent = this.offsetParent(),
        // Get correct offsets
        offset       = this.offset(),
        parentOffset = rootNodeRE.test(offsetParent[0].nodeName) ? { top: 0, left: 0 } : offsetParent.offset()

      // Subtract element margins
      // note: when an element has margin: auto the offsetLeft and marginLeft
      // are the same in Safari causing offset.left to incorrectly be 0
      offset.top  -= parseFloat( $(elem).css('margin-top') ) || 0
      offset.left -= parseFloat( $(elem).css('margin-left') ) || 0

      // Add offsetParent borders
      parentOffset.top  += parseFloat( $(offsetParent[0]).css('border-top-width') ) || 0
      parentOffset.left += parseFloat( $(offsetParent[0]).css('border-left-width') ) || 0

      // Subtract the two offsets
      return {
        top:  offset.top  - parentOffset.top,
        left: offset.left - parentOffset.left
      }
    },
    offsetParent: function() {
      return this.map(function(){
        var parent = this.offsetParent || document.body
        while (parent && !rootNodeRE.test(parent.nodeName) && $(parent).css("position") == "static")
          parent = parent.offsetParent
        return parent
      })
    }
  }

  // for now
  $.fn.detach = $.fn.remove

  // Generate the `width` and `height` functions
  ;['width', 'height'].forEach(function(dimension){
    var dimensionProperty =
      dimension.replace(/./, function(m){ return m[0].toUpperCase() })

    $.fn[dimension] = function(value){
      var offset, el = this[0]
      if (value === undefined) return isWindow(el) ? el['inner' + dimensionProperty] :
        isDocument(el) ? el.documentElement['scroll' + dimensionProperty] :
        (offset = this.offset()) && offset[dimension]
      else return this.each(function(idx){
        el = $(this)
        el.css(dimension, funcArg(this, value, idx, el[dimension]()))
      })
    }
  })

  function traverseNode(node, fun) {
    fun(node)
    for (var i = 0, len = node.childNodes.length; i < len; i++)
      traverseNode(node.childNodes[i], fun)
  }

  // Generate the `after`, `prepend`, `before`, `append`,
  // `insertAfter`, `insertBefore`, `appendTo`, and `prependTo` methods.
  adjacencyOperators.forEach(function(operator, operatorIndex) {
    var inside = operatorIndex % 2 //=> prepend, append

    $.fn[operator] = function(){
      // arguments can be nodes, arrays of nodes, Zepto objects and HTML strings
      var argType, nodes = $.map(arguments, function(arg) {
            var arr = []
            argType = type(arg)
            if (argType == "array") {
              arg.forEach(function(el) {
                if (el.nodeType !== undefined) return arr.push(el)
                else if ($.zepto.isZ(el)) return arr = arr.concat(el.get())
                arr = arr.concat(zepto.fragment(el))
              })
              return arr
            }
            return argType == "object" || arg == null ?
              arg : zepto.fragment(arg)
          }),
          parent, copyByClone = this.length > 1
      if (nodes.length < 1) return this

      return this.each(function(_, target){
        parent = inside ? target : target.parentNode

        // convert all methods to a "before" operation
        target = operatorIndex == 0 ? target.nextSibling :
                 operatorIndex == 1 ? target.firstChild :
                 operatorIndex == 2 ? target :
                 null

        var parentInDocument = $.contains(document.documentElement, parent)

        nodes.forEach(function(node){
          if (copyByClone) node = node.cloneNode(true)
          else if (!parent) return $(node).remove()

          parent.insertBefore(node, target)
          if (parentInDocument) traverseNode(node, function(el){
            if (el.nodeName != null && el.nodeName.toUpperCase() === 'SCRIPT' &&
               (!el.type || el.type === 'text/javascript') && !el.src){
              var target = el.ownerDocument ? el.ownerDocument.defaultView : window
              target['eval'].call(target, el.innerHTML)
            }
          })
        })
      })
    }

    // after    => insertAfter
    // prepend  => prependTo
    // before   => insertBefore
    // append   => appendTo
    $.fn[inside ? operator+'To' : 'insert'+(operatorIndex ? 'Before' : 'After')] = function(html){
      $(html)[operator](this)
      return this
    }
  })

  zepto.Z.prototype = Z.prototype = $.fn

  // Export internal API functions in the `$.zepto` namespace
  zepto.uniq = uniq
  zepto.deserializeValue = deserializeValue
  $.zepto = zepto

  return $
})()

window.Zepto = Zepto
window.$ === undefined && (window.$ = Zepto)

;(function($){
  var _zid = 1, undefined,
      slice = Array.prototype.slice,
      isFunction = $.isFunction,
      isString = function(obj){ return typeof obj == 'string' },
      handlers = {},
      specialEvents={},
      focusinSupported = 'onfocusin' in window,
      focus = { focus: 'focusin', blur: 'focusout' },
      hover = { mouseenter: 'mouseover', mouseleave: 'mouseout' }

  specialEvents.click = specialEvents.mousedown = specialEvents.mouseup = specialEvents.mousemove = 'MouseEvents'

  function zid(element) {
    return element._zid || (element._zid = _zid++)
  }
  function findHandlers(element, event, fn, selector) {
    event = parse(event)
    if (event.ns) var matcher = matcherFor(event.ns)
    return (handlers[zid(element)] || []).filter(function(handler) {
      return handler
        && (!event.e  || handler.e == event.e)
        && (!event.ns || matcher.test(handler.ns))
        && (!fn       || zid(handler.fn) === zid(fn))
        && (!selector || handler.sel == selector)
    })
  }
  function parse(event) {
    var parts = ('' + event).split('.')
    return {e: parts[0], ns: parts.slice(1).sort().join(' ')}
  }
  function matcherFor(ns) {
    return new RegExp('(?:^| )' + ns.replace(' ', ' .* ?') + '(?: |$)')
  }

  function eventCapture(handler, captureSetting) {
    return handler.del &&
      (!focusinSupported && (handler.e in focus)) ||
      !!captureSetting
  }

  function realEvent(type) {
    return hover[type] || (focusinSupported && focus[type]) || type
  }

  function add(element, events, fn, data, selector, delegator, capture){
    var id = zid(element), set = (handlers[id] || (handlers[id] = []))
    events.split(/\s/).forEach(function(event){
      if (event == 'ready') return $(document).ready(fn)
      var handler   = parse(event)
      handler.fn    = fn
      handler.sel   = selector
      // emulate mouseenter, mouseleave
      if (handler.e in hover) fn = function(e){
        var related = e.relatedTarget
        if (!related || (related !== this && !$.contains(this, related)))
          return handler.fn.apply(this, arguments)
      }
      handler.del   = delegator
      var callback  = delegator || fn
      handler.proxy = function(e){
        e = compatible(e)
        if (e.isImmediatePropagationStopped()) return
        e.data = data
        var result = callback.apply(element, e._args == undefined ? [e] : [e].concat(e._args))
        if (result === false) e.preventDefault(), e.stopPropagation()
        return result
      }
      handler.i = set.length
      set.push(handler)
      if ('addEventListener' in element)
        element.addEventListener(realEvent(handler.e), handler.proxy, eventCapture(handler, capture))
    })
  }
  function remove(element, events, fn, selector, capture){
    var id = zid(element)
    ;(events || '').split(/\s/).forEach(function(event){
      findHandlers(element, event, fn, selector).forEach(function(handler){
        delete handlers[id][handler.i]
      if ('removeEventListener' in element)
        element.removeEventListener(realEvent(handler.e), handler.proxy, eventCapture(handler, capture))
      })
    })
  }

  $.event = { add: add, remove: remove }

  $.proxy = function(fn, context) {
    var args = (2 in arguments) && slice.call(arguments, 2)
    if (isFunction(fn)) {
      var proxyFn = function(){ return fn.apply(context, args ? args.concat(slice.call(arguments)) : arguments) }
      proxyFn._zid = zid(fn)
      return proxyFn
    } else if (isString(context)) {
      if (args) {
        args.unshift(fn[context], fn)
        return $.proxy.apply(null, args)
      } else {
        return $.proxy(fn[context], fn)
      }
    } else {
      throw new TypeError("expected function")
    }
  }

  $.fn.bind = function(event, data, callback){
    return this.on(event, data, callback)
  }
  $.fn.unbind = function(event, callback){
    return this.off(event, callback)
  }
  $.fn.one = function(event, selector, data, callback){
    return this.on(event, selector, data, callback, 1)
  }

  var returnTrue = function(){return true},
      returnFalse = function(){return false},
      ignoreProperties = /^([A-Z]|returnValue$|layer[XY]$|webkitMovement[XY]$)/,
      eventMethods = {
        preventDefault: 'isDefaultPrevented',
        stopImmediatePropagation: 'isImmediatePropagationStopped',
        stopPropagation: 'isPropagationStopped'
      }

  function compatible(event, source) {
    if (source || !event.isDefaultPrevented) {
      source || (source = event)

      $.each(eventMethods, function(name, predicate) {
        var sourceMethod = source[name]
        event[name] = function(){
          this[predicate] = returnTrue
          return sourceMethod && sourceMethod.apply(source, arguments)
        }
        event[predicate] = returnFalse
      })

      event.timeStamp || (event.timeStamp = Date.now())

      if (source.defaultPrevented !== undefined ? source.defaultPrevented :
          'returnValue' in source ? source.returnValue === false :
          source.getPreventDefault && source.getPreventDefault())
        event.isDefaultPrevented = returnTrue
    }
    return event
  }

  function createProxy(event) {
    var key, proxy = { originalEvent: event }
    for (key in event)
      if (!ignoreProperties.test(key) && event[key] !== undefined) proxy[key] = event[key]

    return compatible(proxy, event)
  }

  $.fn.delegate = function(selector, event, callback){
    return this.on(event, selector, callback)
  }
  $.fn.undelegate = function(selector, event, callback){
    return this.off(event, selector, callback)
  }

  $.fn.live = function(event, callback){
    $(document.body).delegate(this.selector, event, callback)
    return this
  }
  $.fn.die = function(event, callback){
    $(document.body).undelegate(this.selector, event, callback)
    return this
  }

  $.fn.on = function(event, selector, data, callback, one){
    var autoRemove, delegator, $this = this
    if (event && !isString(event)) {
      $.each(event, function(type, fn){
        $this.on(type, selector, data, fn, one)
      })
      return $this
    }

    if (!isString(selector) && !isFunction(callback) && callback !== false)
      callback = data, data = selector, selector = undefined
    if (callback === undefined || data === false)
      callback = data, data = undefined

    if (callback === false) callback = returnFalse

    return $this.each(function(_, element){
      if (one) autoRemove = function(e){
        remove(element, e.type, callback)
        return callback.apply(this, arguments)
      }

      if (selector) delegator = function(e){
        var evt, match = $(e.target).closest(selector, element).get(0)
        if (match && match !== element) {
          evt = $.extend(createProxy(e), {currentTarget: match, liveFired: element})
          return (autoRemove || callback).apply(match, [evt].concat(slice.call(arguments, 1)))
        }
      }

      add(element, event, callback, data, selector, delegator || autoRemove)
    })
  }
  $.fn.off = function(event, selector, callback){
    var $this = this
    if (event && !isString(event)) {
      $.each(event, function(type, fn){
        $this.off(type, selector, fn)
      })
      return $this
    }

    if (!isString(selector) && !isFunction(callback) && callback !== false)
      callback = selector, selector = undefined

    if (callback === false) callback = returnFalse

    return $this.each(function(){
      remove(this, event, callback, selector)
    })
  }

  $.fn.trigger = function(event, args){
    event = (isString(event) || $.isPlainObject(event)) ? $.Event(event) : compatible(event)
    event._args = args
    return this.each(function(){
      // handle focus(), blur() by calling them directly
      if (event.type in focus && typeof this[event.type] == "function") this[event.type]()
      // items in the collection might not be DOM elements
      else if ('dispatchEvent' in this) this.dispatchEvent(event)
      else $(this).triggerHandler(event, args)
    })
  }

  // triggers event handlers on current element just as if an event occurred,
  // doesn't trigger an actual event, doesn't bubble
  $.fn.triggerHandler = function(event, args){
    var e, result
    this.each(function(i, element){
      e = createProxy(isString(event) ? $.Event(event) : event)
      e._args = args
      e.target = element
      $.each(findHandlers(element, event.type || event), function(i, handler){
        result = handler.proxy(e)
        if (e.isImmediatePropagationStopped()) return false
      })
    })
    return result
  }

  // shortcut methods for `.bind(event, fn)` for each event type
  ;('focusin focusout focus blur load resize scroll unload click dblclick '+
  'mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave '+
  'change select keydown keypress keyup error').split(' ').forEach(function(event) {
    $.fn[event] = function(callback) {
      return (0 in arguments) ?
        this.bind(event, callback) :
        this.trigger(event)
    }
  })

  $.Event = function(type, props) {
    if (!isString(type)) props = type, type = props.type
    var event = document.createEvent(specialEvents[type] || 'Events'), bubbles = true
    if (props) for (var name in props) (name == 'bubbles') ? (bubbles = !!props[name]) : (event[name] = props[name])
    event.initEvent(type, bubbles, true)
    return compatible(event)
  }

})(Zepto)

;(function($){
  var jsonpID = +new Date(),
      document = window.document,
      key,
      name,
      rscript = /<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi,
      scriptTypeRE = /^(?:text|application)\/javascript/i,
      xmlTypeRE = /^(?:text|application)\/xml/i,
      jsonType = 'application/json',
      htmlType = 'text/html',
      blankRE = /^\s*$/,
      originAnchor = document.createElement('a')

  originAnchor.href = window.location.href

  // trigger a custom event and return false if it was cancelled
  function triggerAndReturn(context, eventName, data) {
    var event = $.Event(eventName)
    $(context).trigger(event, data)
    return !event.isDefaultPrevented()
  }

  // trigger an Ajax "global" event
  function triggerGlobal(settings, context, eventName, data) {
    if (settings.global) return triggerAndReturn(context || document, eventName, data)
  }

  // Number of active Ajax requests
  $.active = 0

  function ajaxStart(settings) {
    if (settings.global && $.active++ === 0) triggerGlobal(settings, null, 'ajaxStart')
  }
  function ajaxStop(settings) {
    if (settings.global && !(--$.active)) triggerGlobal(settings, null, 'ajaxStop')
  }

  // triggers an extra global event "ajaxBeforeSend" that's like "ajaxSend" but cancelable
  function ajaxBeforeSend(xhr, settings) {
    var context = settings.context
    if (settings.beforeSend.call(context, xhr, settings) === false ||
        triggerGlobal(settings, context, 'ajaxBeforeSend', [xhr, settings]) === false)
      return false

    triggerGlobal(settings, context, 'ajaxSend', [xhr, settings])
  }
  function ajaxSuccess(data, xhr, settings, deferred) {
    var context = settings.context, status = 'success'
    settings.success.call(context, data, status, xhr)
    if (deferred) deferred.resolveWith(context, [data, status, xhr])
    triggerGlobal(settings, context, 'ajaxSuccess', [xhr, settings, data])
    ajaxComplete(status, xhr, settings)
  }
  // type: "timeout", "error", "abort", "parsererror"
  function ajaxError(error, type, xhr, settings, deferred) {
    var context = settings.context
    settings.error.call(context, xhr, type, error)
    if (deferred) deferred.rejectWith(context, [xhr, type, error])
    triggerGlobal(settings, context, 'ajaxError', [xhr, settings, error || type])
    ajaxComplete(type, xhr, settings)
  }
  // status: "success", "notmodified", "error", "timeout", "abort", "parsererror"
  function ajaxComplete(status, xhr, settings) {
    var context = settings.context
    settings.complete.call(context, xhr, status)
    triggerGlobal(settings, context, 'ajaxComplete', [xhr, settings])
    ajaxStop(settings)
  }

  function ajaxDataFilter(data, type, settings) {
    if (settings.dataFilter == empty) return data
    var context = settings.context
    return settings.dataFilter.call(context, data, type)
  }

  // Empty function, used as default callback
  function empty() {}

  $.ajaxJSONP = function(options, deferred){
    if (!('type' in options)) return $.ajax(options)

    var _callbackName = options.jsonpCallback,
      callbackName = ($.isFunction(_callbackName) ?
        _callbackName() : _callbackName) || ('Zepto' + (jsonpID++)),
      script = document.createElement('script'),
      originalCallback = window[callbackName],
      responseData,
      abort = function(errorType) {
        $(script).triggerHandler('error', errorType || 'abort')
      },
      xhr = { abort: abort }, abortTimeout

    if (deferred) deferred.promise(xhr)

    $(script).on('load error', function(e, errorType){
      clearTimeout(abortTimeout)
      $(script).off().remove()

      if (e.type == 'error' || !responseData) {
        ajaxError(null, errorType || 'error', xhr, options, deferred)
      } else {
        ajaxSuccess(responseData[0], xhr, options, deferred)
      }

      window[callbackName] = originalCallback
      if (responseData && $.isFunction(originalCallback))
        originalCallback(responseData[0])

      originalCallback = responseData = undefined
    })

    if (ajaxBeforeSend(xhr, options) === false) {
      abort('abort')
      return xhr
    }

    window[callbackName] = function(){
      responseData = arguments
    }

    script.src = options.url.replace(/\?(.+)=\?/, '?$1=' + callbackName)
    document.head.appendChild(script)

    if (options.timeout > 0) abortTimeout = setTimeout(function(){
      abort('timeout')
    }, options.timeout)

    return xhr
  }

  $.ajaxSettings = {
    // Default type of request
    type: 'GET',
    // Callback that is executed before request
    beforeSend: empty,
    // Callback that is executed if the request succeeds
    success: empty,
    // Callback that is executed the the server drops error
    error: empty,
    // Callback that is executed on request complete (both: error and success)
    complete: empty,
    // The context for the callbacks
    context: null,
    // Whether to trigger "global" Ajax events
    global: true,
    // Transport
    xhr: function () {
      return new window.XMLHttpRequest()
    },
    // MIME types mapping
    // IIS returns Javascript as "application/x-javascript"
    accepts: {
      script: 'text/javascript, application/javascript, application/x-javascript',
      json:   jsonType,
      xml:    'application/xml, text/xml',
      html:   htmlType,
      text:   'text/plain'
    },
    // Whether the request is to another domain
    crossDomain: false,
    // Default timeout
    timeout: 0,
    // Whether data should be serialized to string
    processData: true,
    // Whether the browser should be allowed to cache GET responses
    cache: true,
    //Used to handle the raw response data of XMLHttpRequest.
    //This is a pre-filtering function to sanitize the response.
    //The sanitized response should be returned
    dataFilter: empty
  }

  function mimeToDataType(mime) {
    if (mime) mime = mime.split(';', 2)[0]
    return mime && ( mime == htmlType ? 'html' :
      mime == jsonType ? 'json' :
      scriptTypeRE.test(mime) ? 'script' :
      xmlTypeRE.test(mime) && 'xml' ) || 'text'
  }

  function appendQuery(url, query) {
    if (query == '') return url
    return (url + '&' + query).replace(/[&?]{1,2}/, '?')
  }

  // serialize payload and append it to the URL for GET requests
  function serializeData(options) {
    if (options.processData && options.data && $.type(options.data) != "string")
      options.data = $.param(options.data, options.traditional)
    if (options.data && (!options.type || options.type.toUpperCase() == 'GET' || 'jsonp' == options.dataType))
      options.url = appendQuery(options.url, options.data), options.data = undefined
  }

  $.ajax = function(options){
    var settings = $.extend({}, options || {}),
        deferred = $.Deferred && $.Deferred(),
        urlAnchor, hashIndex
    for (key in $.ajaxSettings) if (settings[key] === undefined) settings[key] = $.ajaxSettings[key]

    ajaxStart(settings)

    if (!settings.crossDomain) {
      urlAnchor = document.createElement('a')
      urlAnchor.href = settings.url
      // cleans up URL for .href (IE only), see https://github.com/madrobby/zepto/pull/1049
      urlAnchor.href = urlAnchor.href
      settings.crossDomain = (originAnchor.protocol + '//' + originAnchor.host) !== (urlAnchor.protocol + '//' + urlAnchor.host)
    }

    if (!settings.url) settings.url = window.location.toString()
    if ((hashIndex = settings.url.indexOf('#')) > -1) settings.url = settings.url.slice(0, hashIndex)
    serializeData(settings)

    var dataType = settings.dataType, hasPlaceholder = /\?.+=\?/.test(settings.url)
    if (hasPlaceholder) dataType = 'jsonp'

    if (settings.cache === false || (
         (!options || options.cache !== true) &&
         ('script' == dataType || 'jsonp' == dataType)
        ))
      settings.url = appendQuery(settings.url, '_=' + Date.now())

    if ('jsonp' == dataType) {
      if (!hasPlaceholder)
        settings.url = appendQuery(settings.url,
          settings.jsonp ? (settings.jsonp + '=?') : settings.jsonp === false ? '' : 'callback=?')
      return $.ajaxJSONP(settings, deferred)
    }

    var mime = settings.accepts[dataType],
        headers = { },
        setHeader = function(name, value) { headers[name.toLowerCase()] = [name, value] },
        protocol = /^([\w-]+:)\/\//.test(settings.url) ? RegExp.$1 : window.location.protocol,
        xhr = settings.xhr(),
        nativeSetHeader = xhr.setRequestHeader,
        abortTimeout

    if (deferred) deferred.promise(xhr)

    if (!settings.crossDomain) setHeader('X-Requested-With', 'XMLHttpRequest')
    setHeader('Accept', mime || '*/*')
    if (mime = settings.mimeType || mime) {
      if (mime.indexOf(',') > -1) mime = mime.split(',', 2)[0]
      xhr.overrideMimeType && xhr.overrideMimeType(mime)
    }
    if (settings.contentType || (settings.contentType !== false && settings.data && settings.type.toUpperCase() != 'GET'))
      setHeader('Content-Type', settings.contentType || 'application/x-www-form-urlencoded')

    if (settings.headers) for (name in settings.headers) setHeader(name, settings.headers[name])
    xhr.setRequestHeader = setHeader

    xhr.onreadystatechange = function(){
      if (xhr.readyState == 4) {
        xhr.onreadystatechange = empty
        clearTimeout(abortTimeout)
        var result, error = false
        if ((xhr.status >= 200 && xhr.status < 300) || xhr.status == 304 || (xhr.status == 0 && protocol == 'file:')) {
          dataType = dataType || mimeToDataType(settings.mimeType || xhr.getResponseHeader('content-type'))

          if (xhr.responseType == 'arraybuffer' || xhr.responseType == 'blob')
            result = xhr.response
          else {
            result = xhr.responseText

            try {
              // http://perfectionkills.com/global-eval-what-are-the-options/
              // sanitize response accordingly if data filter callback provided
              result = ajaxDataFilter(result, dataType, settings)
              if (dataType == 'script')    (1,eval)(result)
              else if (dataType == 'xml')  result = xhr.responseXML
              else if (dataType == 'json') result = blankRE.test(result) ? null : $.parseJSON(result)
            } catch (e) { error = e }

            if (error) return ajaxError(error, 'parsererror', xhr, settings, deferred)
          }

          ajaxSuccess(result, xhr, settings, deferred)
        } else {
          ajaxError(xhr.statusText || null, xhr.status ? 'error' : 'abort', xhr, settings, deferred)
        }
      }
    }

    if (ajaxBeforeSend(xhr, settings) === false) {
      xhr.abort()
      ajaxError(null, 'abort', xhr, settings, deferred)
      return xhr
    }

    var async = 'async' in settings ? settings.async : true
    xhr.open(settings.type, settings.url, async, settings.username, settings.password)

    if (settings.xhrFields) for (name in settings.xhrFields) xhr[name] = settings.xhrFields[name]

    for (name in headers) nativeSetHeader.apply(xhr, headers[name])

    if (settings.timeout > 0) abortTimeout = setTimeout(function(){
        xhr.onreadystatechange = empty
        xhr.abort()
        ajaxError(null, 'timeout', xhr, settings, deferred)
      }, settings.timeout)

    // avoid sending empty string (#319)
    xhr.send(settings.data ? settings.data : null)
    return xhr
  }

  // handle optional data/success arguments
  function parseArguments(url, data, success, dataType) {
    if ($.isFunction(data)) dataType = success, success = data, data = undefined
    if (!$.isFunction(success)) dataType = success, success = undefined
    return {
      url: url
    , data: data
    , success: success
    , dataType: dataType
    }
  }

  $.get = function(/* url, data, success, dataType */){
    return $.ajax(parseArguments.apply(null, arguments))
  }

  $.post = function(/* url, data, success, dataType */){
    var options = parseArguments.apply(null, arguments)
    options.type = 'POST'
    return $.ajax(options)
  }

  $.getJSON = function(/* url, data, success */){
    var options = parseArguments.apply(null, arguments)
    options.dataType = 'json'
    return $.ajax(options)
  }

  $.fn.load = function(url, data, success){
    if (!this.length) return this
    var self = this, parts = url.split(/\s/), selector,
        options = parseArguments(url, data, success),
        callback = options.success
    if (parts.length > 1) options.url = parts[0], selector = parts[1]
    options.success = function(response){
      self.html(selector ?
        $('<div>').html(response.replace(rscript, "")).find(selector)
        : response)
      callback && callback.apply(self, arguments)
    }
    $.ajax(options)
    return this
  }

  var escape = encodeURIComponent

  function serialize(params, obj, traditional, scope){
    var type, array = $.isArray(obj), hash = $.isPlainObject(obj)
    $.each(obj, function(key, value) {
      type = $.type(value)
      if (scope) key = traditional ? scope :
        scope + '[' + (hash || type == 'object' || type == 'array' ? key : '') + ']'
      // handle data in serializeArray() format
      if (!scope && array) params.add(value.name, value.value)
      // recurse into nested objects
      else if (type == "array" || (!traditional && type == "object"))
        serialize(params, value, traditional, key)
      else params.add(key, value)
    })
  }

  $.param = function(obj, traditional){
    var params = []
    params.add = function(key, value) {
      if ($.isFunction(value)) value = value()
      if (value == null) value = ""
      this.push(escape(key) + '=' + escape(value))
    }
    serialize(params, obj, traditional)
    return params.join('&').replace(/%20/g, '+')
  }
})(Zepto)

;(function($){
  $.fn.serializeArray = function() {
    var name, type, result = [],
      add = function(value) {
        if (value.forEach) return value.forEach(add)
        result.push({ name: name, value: value })
      }
    if (this[0]) $.each(this[0].elements, function(_, field){
      type = field.type, name = field.name
      if (name && field.nodeName.toLowerCase() != 'fieldset' &&
        !field.disabled && type != 'submit' && type != 'reset' && type != 'button' && type != 'file' &&
        ((type != 'radio' && type != 'checkbox') || field.checked))
          add($(field).val())
    })
    return result
  }

  $.fn.serialize = function(){
    var result = []
    this.serializeArray().forEach(function(elm){
      result.push(encodeURIComponent(elm.name) + '=' + encodeURIComponent(elm.value))
    })
    return result.join('&')
  }

  $.fn.submit = function(callback) {
    if (0 in arguments) this.bind('submit', callback)
    else if (this.length) {
      var event = $.Event('submit')
      this.eq(0).trigger(event)
      if (!event.isDefaultPrevented()) this.get(0).submit()
    }
    return this
  }

})(Zepto)

;(function(){
  // getComputedStyle shouldn't freak out when called
  // without a valid element as argument
  try {
    getComputedStyle(undefined)
  } catch(e) {
    var nativeGetComputedStyle = getComputedStyle
    window.getComputedStyle = function(element, pseudoElement){
      try {
        return nativeGetComputedStyle(element, pseudoElement)
      } catch(e) {
        return null
      }
    }
  }
})()
  return Zepto
}))

// '
$( document ).ready(function()
{
    var template = $('#jobSelectPopup');
    $('td.jobSelectPopup').each(function ()
    {
        var jobId = $(this).text();
        $(this).empty().append(template.clone().show().val(jobId));
    });
});
