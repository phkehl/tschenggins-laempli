#!/usr/bin/perl

=pod

=encoding utf8

=head1 tschenggins-status.pl -- Tschenggins Lämpli Backend and User Inferface

Copyright (c) 2017-2019 Philippe Kehl <flipflip at oinkzwurgl dot org>,
L<https://oinkzwurgl.org/projaeggd/tschenggins-laempli>

=head2 Usage

As a CGI script running on a web server: C<< https://..../tschenggins-status.pl?param=value;param=value;... >>

On the command line: C<< ./tschenggins-status.pl param=value param=value ... >>

=cut

use strict;
use warnings;

use feature 'state';

use Time::HiRes qw(time);
my $T0 = time();

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

my $q = CGI->new();

my @DEBUGSTRS     = ();
my $DATADIR       = $FindBin::Bin;
my $VALIDRESULT   = { unknown => 1, success => 2, unstable => 3, failure => 4 };
my $VALIDSTATE    = { unknown => 1, off => 2, idle => 3, running => 4 };
my $UNKSTATE      = { name => 'unknown', server => 'unknown', result => 'unknown', state => 'unknown', ts => int(time()) };
my $JOBNAMERE     = qr{^[-_a-zA-Z0-9]{5,100}$};
my $SERVERNAMERE  = qr{^[-_a-zA-Z0-9.]{5,50}$};
my $JOBIDRE       = qr{^[0-9a-z]{8,8}$};
my $DBFILE        = $ENV{'REMOTE_USER'} ? "$DATADIR/tschenggins-status-$ENV{'REMOTE_USER'}.json" : "$DATADIR/tschenggins-status.json";
my $DEFAULTCMD    = 'gui';

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

=item * C<jobs> -- one or more job ID (array)

=item * C<limit> -- limit of list of results (default 0, i.e. all)

=item * C<maxch> -- maximum number of channels the client can handle (default 10)

=item * C<name> -- client or job name

=item * C<offset> -- offset into list of results (default 0)

=item * C<redirct> -- where to redirect to (query string)

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
    my $cmd      = $q->param('cmd')      || $DEFAULTCMD;
    my $debug    = $q->param('debug')    || 0;
    my $job      = $q->param('job')      || '';
    my @jobs     = $q->multi_param('jobs');
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
    my $model    = $q->param('model')    || '';
    my $driver   = $q->param('driver')   || ''; # TODO: @key-@val
    my $order    = $q->param('order')    || '';
    my $bright   = $q->param('bright')   || '';
    my $noise    = $q->param('noise')    || '';
    my $cfgcmd   = $q->param('cfgcmd')   || '';

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

=item B<< C<< cmd=update <states=...> >> >>

This expects a application/json POST request. The C<states> array consists of objects with the
following fields: C<server>, C<name> and optionally C<state> and/or C<result>. A last-changed
timestamp can be given in C<ts>.

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

    \r\n
    hello 87e984 256 clientname\r\n
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
        push(@html, _gui($db, $client));
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

=item B<< C<< cmd=multi job=<jobid> [jobs=<jobid> ...] >> >>

Set multijob list of jobs.

=cut

    elsif ($cmd eq 'multi')
    {
        (my $res, $error) = _multi($db, $job, \@jobs);
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
    if (!$error && $redirect)
    {
        my $debug = $debug;
        $q->delete_all();
        my $hash = '';
        if ($redirect =~ m{(^[^#]*)(#.+)?})
        {
            my $query = $1;
            $hash = $2 if ($2);
            $query =~ s{cmd=$DEFAULTCMD}{};
            foreach my $kv (split(';', $query))
            {
                if ($kv =~ m{^([^=]+)=(.+)})
                {
                    #$q->param($1, $2);
                    DEBUG("set [$1] [$2]");
                }
            }
        }
        if ($debug)
        {
            $q->param('debug', $debug);
        }
        print($q->redirect($q->self_url() . $hash));
        exit(0);
    }

    # output
    if (!$error && ($#html > -1))
    {
        my $pre = join('', map { "$_\n" } @DEBUGSTRS);
        $pre =~ s{<}{&lt;}gs;
        $pre =~ s{>}{&gt;}gs;
        print(
              $q->header( -type => 'text/html', -expires => 'now', charset => 'UTF-8',
                          #'-Access-Control-Allow-Origin' => '*'
                        ),
              $q->start_html(-title => $TITLE,
                             -head => [ '<meta name="viewport" content="width=device-width, initial-scale=1.0"/>',
                                        CGI::Link({ -rel => 'shortcut icon', -href => 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAA0ElEQVQ4y5VRwQkCMRCcDfkdFiC+vDbOEoR7axkWcmXoW7CEsw19iQWIv0B8xJFNLjnMQmB2yczuzgoAjNZ6qNg4J0jigLcHgAGN6NyQ3DmHzjnkBGdjtNZ7IHo1Aubfjxw5rRkAuFr7K2o8J0IsJRNvZ0S1toekUwxoRHKdSF6vQn5/TEV4jeK8JBNThESGxdbH5lzyU1VfgR1TnEboxilU95yJy2Nce+4hUw++Ym0fr0LyYhfy1ynUTM2+JGtcJVD2ILNCepXcClUCWkSb+AEnDWJiA7f+pQAAAABJRU5ErkJggg==' }) ],
                             -style   => [ { src => 'tschenggins-status.css' } ],
                             -script  => [
                                             { -type => 'javascript', -src => 'jquery-3.3.1.min.js' },
                                             { -type => 'javascript', -src => 'tschenggins-status.js' },
                                         ]),
              $q->a({ -class => 'database', -href => ($q->url() . '?cmd=rawdb;debug=1') }, 'DB: ' . ($ENV{'REMOTE_USER'} ? $ENV{'REMOTE_USER'} : '(default)')),
              $q->h1({ -class => 'title' }, $q->a({ -href => $q->url() }, $TITLE)),
              @html,
              $q->p({ -class => 'footer' }, 'Tschenggins Lämpli &mdash; Copyright &copy; 2017&ndash;2019 Philippe Kehl &amp; flipflip industries &mdash; '
                    . $q->a({ -href => 'https://oinkzwurgl.org/projeaggt/tschenggins-laempli' }, 'https://oinkzwurgl.org/projeaggt/tschenggins-laempli')
                    . ' &mdash; ' . $q->a({ -href => ($q->url() . '?cmd=help') }, 'help')
                    . ' &mdash; ' . sprintf('%.3fs', time() - $T0),
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
    my $now = int(time());
    my $ok = 1;
    my $error = '';
    foreach my $st (@states)
    {
        my $jobName  = $st->{name}   || '';
        my $server   = $st->{server} || '';
        my $jState   = $st->{state}  || '';
        my $jResult  = $st->{result} || '';
        my $ts       = $st->{ts}     || $now;
        if ($st->{server} !~ m{$SERVERNAMERE})
        {
            $error = "not a valid server name: $server";
            $ok = 0;
            last;
        }
        if ($jobName !~ m{$JOBNAMERE})
        {
            $error = "not a valid job name: $jobName ($JOBNAMERE)";
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

        my $id = __serverAndNameToJobId($server, $jobName);
        DEBUG("_update() $server $jobName $jState $jResult $ts $id");
        $db->{jobs}->{$id}->{ts}     = $ts;
        $db->{jobs}->{$id}->{name}   = $jobName;
        $db->{jobs}->{$id}->{server} = $server;
        $db->{jobs}->{$id}->{state}  //= 'unknown';
        $db->{jobs}->{$id}->{result} //= 'unknown';
        $db->{jobs}->{$id}->{state}  = $jState       if ($jState);
        $db->{jobs}->{$id}->{result} = $jResult      if ($jResult);
        $db->{_dirtiness}++;
    }

    # update multijobs
    if ($ok)
    {
        __update_multijobs($db);
    }

    return $ok, $error;
}

sub __serverAndNameToJobId
{
    my ($server, $jobName) = @_;
    return substr(Digest::MD5::md5_hex("$server$jobName"), -8);
}

sub __update_multijobs
{
    my ($db) = @_;
    foreach my $multiSt (grep { ($_->{server} eq 'multijob') && $_->{multi} } map { $db->{jobs}->{$_} } @{$db->{_jobIds}})
    {
        my $state = 'unknown';
        my $result = 'unknown';
        my $ts = 0;
        foreach my $id (@{$multiSt->{multi}})
        {
            my $jobSt = $db->{jobs}->{$id};
            #DEBUG("$multiSt->{name}: consider $jobSt->{name} $jobSt->{state} $jobSt->{result}");
            if ($VALIDSTATE->{ $jobSt->{state} } > $VALIDSTATE->{$state})
            {
                $state = $jobSt->{state};
            }
            if ($VALIDRESULT->{ $jobSt->{result} } > $VALIDRESULT->{$result})
            {
                $result = $jobSt->{result};
            }
            if ($jobSt->{ts} > $ts)
            {
                $ts = $jobSt->{ts};
            }
        }
        $ts ||= int(time());
        if ( ($multiSt->{state} ne $state) || ($multiSt->{result} ne $result) || ($multiSt->{ts} != $ts) )
        {
            DEBUG("$multiSt->{name}: $multiSt->{state} -> $state, $multiSt->{result} -> $result, $multiSt->{ts} -> $ts");
            $multiSt->{state}     = $state;
            $multiSt->{result}    = $result;
            $multiSt->{ts} = $ts;
            $db->{_dirtiness}++;
        }
    }
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
    print("\r\nhello $client $strlen $info->{name}\r\n");

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
    $db->{jobs}->{$id}->{ts}     = int(time());
    $db->{_dirtiness}++;
    __update_multijobs($db);
    return 1, '';
}

sub _multi
{
    my ($db, $multiId, $jobIds) = @_;
    DEBUG("_multi() %s -- %s", $multiId, "@{$jobIds}");

    if (!$multiId)
    {
        return 0, 'missing parameter';
    }

    if ( !$db->{jobs}->{$multiId} || ($db->{jobs}->{$multiId}->{server} ne 'multijob') )
    {
        return 0, 'illegal job';
    }

    # empty @jobIds, delete multi job info
    if ($#{$jobIds} < 0)
    {
        if ($db->{jobs}->{$multiId}->{multi})
        {
            $db->{jobs}->{$multiId}->{multi} = [];
            $db->{_dirtiness}++;
        }
        __update_multijobs($db);
        return 1;
    }

    # verify that @jobIds are valid jobs
    foreach my $id (@{$jobIds})
    {
        if ( !$db->{jobs}->{$id} || ($db->{jobs}->{$id} eq 'multijob') )
        {
            return 0, 'illegal jobs';
        }
    }

    # store multi job info
    $db->{jobs}->{$multiId}->{multi} = $jobIds;
    $db->{_dirtiness}++;

    __update_multijobs($db);

    return 1;
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

    # helpers
    my %leds = ();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        $leds{$jobId} = __gui_led($db->{jobs}->{$jobId});
    }
    my $jobSelectRadios = '<div class="joblist">';
    my $jobSelectRadiosAll = '<div class="joblist">';
    my $jobSelectCheckboxes = '<div class="joblist">';
    my $multiSelectRadios = '<div class="joblist">';
    my $now = time();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        my $age = _age_str($now, $st->{ts});
        my $labelStart = '<label class="job" data-state="' . $st->{state} . '" data-result="' . $st->{result} . '"'
          . ' data-server="' . $st->{server} . '" data-name="' . $st->{name} . '"'
          . ' data-multi="' . join(',', @{ $st->{multi} || [] }) . '">';
        my $labelEnd = "$leds{$jobId} $st->{server}: $st->{name} ($st->{state}, $st->{result}, $age)</label>";
        my $radio = $labelStart . '<input type="radio" name="job" value="' . $jobId . '" autocomplete="off"/>' . $labelEnd;
        my $checkbox = $labelStart . '<input type="checkbox" name="jobs" value="' . $jobId . '" autocomplete="off"/>' . $labelEnd;
        $jobSelectRadiosAll .= $radio;
        if ($st->{server} ne 'multijob')
        {
            $jobSelectRadios .= $radio;
            $jobSelectCheckboxes .= $checkbox;
        }
        else
        {
            $multiSelectRadios .= $radio;
        }
    }
    $jobSelectRadios .= '</div>';
    $jobSelectRadiosAll .= '</div>';
    $jobSelectCheckboxes .= '</div>';
    $multiSelectRadios .= '</div>';

    return $q->div({ -class => 'tabs' },

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-laempli', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-laempli' }, $q->h2('Lämpli')),
                   $q->div({ -class => 'tab-contents' }, _gui_clients($db, \%leds) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-config', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-config' }, $q->h2('Config')),
                   $q->div({ -class => 'tab-contents' }, _gui_config($db, \%leds) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-jobs', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-jobs' }, $q->h2('Jobs')),
                   $q->div({ -class => 'tab-contents' }, _gui_jobs($db, \%leds) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-modify', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-modify' }, $q->h2('Modify')),
                   $q->div({ -class => 'tab-contents' }, _gui_modify($db, $jobSelectRadios) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-multi', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-multi' }, $q->h2('Multi')),
                   $q->div({ -class => 'tab-contents' }, _gui_multi($db, $multiSelectRadios, $jobSelectCheckboxes) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-create', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-create' }, $q->h2('Create')),
                   $q->div({ -class => 'tab-contents' }, _gui_create($db) ),

                   $q->input({ -class => 'tab-input', -name => 'tabs', type => 'radio', id => 'tab-delete', -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => 'tab-delete' }, $q->h2('Delete')),
                   $q->div({ -class => 'tab-contents' }, _gui_delete($db, $jobSelectRadiosAll) ),
                  );
}

sub _gui_jobs
{
    my ($db, $leds) = @_;
    my @html = ();

    my $now = time();
    my @trs = ();
    foreach my $jobId (@{$db->{_jobIds}})
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        my $modify = $q->span({ -class => 'action action-modify-job', -data_server => $st->{server}, -data_jobid => $jobId }, 'modify');
        my $delete = $q->span({ -class => 'action action-delete-job', -data_jobid => $jobId }, 'delete');
        my $multiInfo = '';
        if ($db->{jobs}->{$jobId}->{multi})
        {
            $multiInfo .= '<span class="multijob-info-toggle" data-id="multijob-info-' . $jobId . '">details&hellip;</span>';
            $multiInfo .= '<div id="multijob-info-' . $jobId . '" class="multijob-info joblist"><ul>';
            foreach my $id (@{$db->{jobs}->{$jobId}->{multi}})
            {
                my $s = $db->{jobs}->{$id};
                my $age = _age_str($now, $s->{ts});
                $multiInfo .= "<li>$leds->{$id} $s->{server}: $s->{name} ($s->{state}, $s->{result}, $age)</li>";
            }
            $multiInfo .= '</ul></div>';
        }

        push(@trs, $q->Tr(
                          $q->td({ -class => 'jobid' }, $jobId),
                          $q->td({ -class => 'nowrap' }, $st->{server} eq 'multijob' ? $q->i($st->{server}) : $st->{server}),
                          $q->td({}, $leds->{$jobId}, $st->{name}, $multiInfo),
                          $q->td({}, $st->{state}), $q->td({}, $st->{result}),
                          $q->td({ -align => 'right', -data_sort => $st->{ts} }, _age_str($now, $st->{ts})),
                          $q->td({ -class => 'nowrap' }, $modify, $delete),
                         )
            );
    }
    return (
            $q->p({}, 'The current state and result of all known jobs.'),
            $q->table(
                      $q->Tr(
                             $q->th('Filter:'),
                             $q->td($q->input({ -type => 'text', -id => 'jobs-filter', -autocomplete => 'off', -placeholder => 'filter (regex)...', -default => '' })),
                             $q->td({ -id => 'jobs-filter-status', -class => 'right nowrap max-width' }, 'showing all'),
                            ),
                      ),
            $q->table({ -id => 'jobs-table' },
                      $q->thead(
                                $q->Tr(
                                       $q->th({ -class => 'sort' }, $q->span({}, 'ID')),
                                       $q->th({ -class => 'sort' }, $q->span({}, 'Server')),
                                       $q->th({ -class => 'sort max-width' }, $q->span({}, 'Job')),
                                       $q->th({ -class => 'sort' }, $q->span({}, 'State')),
                                       $q->th({ -class => 'sort' }, $q->span({}, 'Result')),
                                       $q->th({ -class => 'sort' }, $q->span({}, 'Age')),
                                       $q->th({}, 'Actions'),
                                      ),
                               ),
                      $q->tbody(
                                @trs
                               )
                     )
           );
}

sub _gui_modify
{
    my ($db, $jobSelectRadios) = @_;
    my $debug = $q->param('debug') || 0;
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
    return (
            $q->p({}, 'Select a job and override its state and/or result.'),
            $q->start_form(-method => 'POST', -action => $q->url(), id => 'modify-job-form' ),
            $q->table(
                      $q->Tr({},
                             $q->th('Job:'),
                             $q->td({ -class => 'max-width' }, $jobSelectRadios),
                            ),
                      $q->Tr({},
                             $q->th('State:'),
                             $q->td($q->radio_group($stateSelectArgs)),
                             ),
                      $q->Tr({},
                             $q->th('Result:'),
                             $q->td($q->radio_group($resultSelectArgs)),
                            ),
                      $q->Tr($q->td({ -colspan => 4, -align => 'center' },
                                    $q->submit(-value => 'modify'))),
                     ),
            ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
            $q->hidden(-name => 'cmd', -default => 'set'),
            $q->hidden(-name => 'redirect', -default => 'cmd=gui#modify'),
            $q->end_form()
           );
}

sub _gui_multi
{
    my ($db, $multiSelectRadios, $jobSelectCheckboxes) = @_;
    my $debug = $q->param('debug') || 0;

    return (
            $q->p({}, 'Select a multi-job and adjust included jobs.'),
            $q->start_form(-method => 'POST', -action => $q->url(), id => 'modify-multi-form' ),
            $q->table(
                      $q->Tr({},
                             $q->th('Multijob:'),
                             $q->td({ -class => 'max-width' }, $multiSelectRadios),
                            ),
                      $q->Tr({},
                             $q->th('Jobs:'),
                             $q->td({}, $jobSelectCheckboxes),
                            ),
                      $q->Tr($q->td({ -colspan => 4, -align => 'center' },
                                    $q->submit(-value => 'modify'))),
                     ),
            ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
            $q->hidden(-name => 'cmd', -default => 'multi'),
            $q->hidden(-name => 'redirect', -default => 'cmd=gui#multi'),
            $q->end_form()
           );
}

sub _gui_create
{
    my ($db) = @_;
    my $debug = $q->param('debug') || 0;
    my $stateSelectArgs =
    {
        -name         => 'state',
        -values       => [ sort { $VALIDSTATE->{$a} <=> $VALIDSTATE->{$b} } keys %{$VALIDSTATE} ],
        -autocomplete => 'off',
        -default      => 'unknown',
        -linebreak    => 0,
    };
    my $resultSelectArgs =
    {
        -name         => 'result',
        -values       => [ sort { $VALIDRESULT->{$a} <=> $VALIDRESULT->{$b} } keys %{$VALIDRESULT} ],
        -autocomplete => 'off',
        -default      => 'unknown',
        -linebreak    => 0,
    };

    return (
        $q->p({}, 'Create a new job. Give a server name (or <i>multijob</i>), a job name and its state and result.'),
        $q->start_form(-method => 'POST', -action => $q->url(), id => 'create-job-form' ),
        $q->table(
                  $q->Tr(
                         $q->th('Server:'),
                         $q->td($q->input({
                                           -type => 'text',
                                           -name => 'server',
                                           -size => 20,
                                           -autocomplete => 'off',
                                           -default => '',
                                          })),
                        ),
                  $q->Tr(
                         $q->th('Job:'),
                         $q->td($q->input({
                                           -type => 'text',
                                           -name => 'job',
                                           -size => 20,
                                           -autocomplete => 'off',
                                           -default => '',
                                          })),
                        ),
                  $q->Tr(
                         $q->th('State:'),
                         $q->td($q->radio_group($stateSelectArgs)),
                        ),
                  $q->Tr(
                         $q->th('Result:'),
                         $q->td($q->radio_group($resultSelectArgs)),
                        ),
                  $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                $q->submit(-value => 'create')))
                 ),
        ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
        $q->hidden(-name => 'cmd', -default => 'add'),
        $q->hidden(-name => 'redirect', -default => 'cmd=gui#create'),
        $q->end_form()
    );
}

sub _gui_delete
{
    my ($db, $jobSelectRadios) = @_;
    my $debug = $q->param('debug') || 0;

    return (
            $q->p({}, 'Delete a job from the list of known jobs.'),
            $q->start_form(-method => 'POST', -action => $q->url(), id => 'delete-job-form' ),
            $q->table(
                      $q->Tr(
                             $q->th('Job:'),
                             $q->td({ -class => 'max-width' }, $jobSelectRadios),
                            ),
                      $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                    $q->submit(-value => 'delete info')))
                     ),
            ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
            $q->hidden(-name => 'cmd', -default => 'del'),
            $q->hidden(-name => 'redirect', -default => 'cmd=gui#delete'),
            $q->end_form()
           );
}

sub _gui_clients
{
    my ($db, $leds) = @_;

    my @trs = ();
    my $now = time();
    foreach my $clientId (sort  @{$db->{_clientIds}})
    {
        my $client = $db->{clients}->{$clientId};
        my $config = $db->{config}->{$clientId};
        my $name     = $client->{name} || 'unknown';
        my $cfgName  = $config->{name} || 'unknown';
        my $cfgModel = $config->{model} || 'unknown';
        my $last     = $client->{ts} ? _age_str($now, $client->{ts}) . ' ago' : 'unknown';
        my $online   = $client->{check} && (($now - $client->{check}) < 15) ? 'online' : 'offline';
        my $check    = $online eq 'online' ? int($now - $client->{check} + 0.5) : 'n/a';
        my $pid      = $client->{pid} || 'n/a';
        my $staIp    = $client->{staip} || 'unknown';
        my $staSsid  = $client->{stassid} || 'unknown';
        my $version  = $client->{version} || 'unknown';
        my $edit     = $q->span({ -class => 'action action-configure-client', -data_clientid => $clientId }, 'configure');

        my @leds = ();
        foreach my $jobId (@{$config->{jobs}})
        {
            if ($jobId)
            {
                push(@leds, $leds->{$jobId});
            }
        }

        push(@trs, $q->Tr({},
                          $q->td({ -class => 'clientid', -data_sort => hex($clientId) }, $clientId),
                          $q->td({ -class => 'nowrap' }, $name),
                          $q->td({ -class => 'nowrap' }, $cfgName),
                          $q->td({}, @leds),
                          $q->td({ -class => 'class nowrap right', -data_sort => ($client->{ts} || 0) }, $last),
                          $q->td({ -class => "$online center nowrap", -data_sort => "$online $pid" }, "$pid ($check)"),
                          $q->td({ -class => 'center nowrap' }, $staIp),
                          $q->td({ -align => 'center nowrap' }, $staSsid),
                          $q->td({ }, $cfgModel),
                          $q->td({ -class => 'center' }, $version), $q->td({}, $edit)));
    }
    return (
         $q->p({}, 'Here is a list of all known Lämpli.'),
         $q->table({},
                   $q->thead({}, $q->Tr({},
                                        $q->th({ -class => 'sort' }, 'ID'),
                                        $q->th({ -class => 'sort' }, 'Client'),
                                        $q->th({ -class => 'sort' }, 'Name'),
                                        $q->th({ -class => 'max-width' }, 'Status'),
                                        $q->th({ -class => 'sort' }, 'Connected'),
                                        $q->th({ -class => 'sort' }, 'PID'),
                                        $q->th({ -class => 'sort nowrap' }, 'Sta IP'),
                                        $q->th({ -class => 'sort nowrap' }, 'Sta SSID'),
                                        $q->th({ -class => 'sort' }, 'Model'),
                                        $q->th({ -class => 'sort' }, 'Version'),
                                        $q->th({}, 'Actions'),
                                       ),
                             ),
                   $q->tbody({}, @trs)));
}

sub _gui_config
{
    my ($db, $leds) = @_;

    my @tabs = ();

    foreach my $clientId (sort  @{$db->{_clientIds}})
    {
        my $name = $db->{config}->{$clientId}->{name} || $clientId;

        push(@tabs,
                   $q->input({ -class => 'tab-input', -name => 'tabs-config', type => 'radio', id => "tab-config-$clientId", -autocomplete => 'off' }),
                   $q->label({ -class => 'tab-label', -for => "tab-config-$clientId" }, $q->h3($name)),
                   $q->div({ -class => 'tab-contents' }, __gui_config_client($db, $clientId, $leds) ),
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

    return (
            $q->p({}, 'Configure a Lämpli. Select yours:'),
            $q->div({ -class => 'tabs' }, @tabs, $q->popup_menu($jobSelectArgs)),
           );
}

sub __gui_config_client
{
    my ($db, $clientId, $leds) = @_;
    my $client = $db->{clients}->{$clientId};
    my $config = $db->{config}->{$clientId};
    if (!$client || !$config)
    {
        return $q->p('nothing here :-(');
    }
    my $debug = $q->param('debug') || 0;

    # config: jobs
    my @jobsTrs = ();
    my $maxch = $client->{maxch} || 0;
    for (my $ix = 0; $ix < $maxch; $ix++)
    {
        my $jobId = $config->{jobs}->[$ix] || '';
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        push(@jobsTrs, # Note: the job selection popup menu is populated run-time (JS), since $q->popup_menu() is very expensive
             $q->Tr({}, $q->td({ -align => 'right' }, $ix), $q->td({ -class => 'jobid' }, $jobId),
                    $q->td({}, $leds->{$jobId} || ''), $q->td({ -class => 'jobSelectPopup' }, $jobId))
            );
    }
    my $htmlJobs =
      $q->div({ },
              $q->start_form(-method => 'GET', -action => $q->url() ),
              $q->table({},
                        $q->Tr({}, $q->th({}, 'Ix'), $q->th({}, 'ID'), $q->th({}, ''), $q->th({ -class => 'max-width' }, 'Job')),
                        @jobsTrs,
                        ($maxch ? $q->Tr({ }, $q->td({ -colspan => 4, -align => 'center' }, $q->submit(-value => 'save channel config'))) : ''),
                       ),
              ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
              $q->hidden(-name => 'cmd', -default => 'cfgjobs'),
              $q->hidden(-name => 'client', -default => $clientId),
              $q->hidden(-name => 'redirect', -default => "cmd=gui#config-$clientId"),
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
                        $q->Tr({}, $q->th({ -colspan => 2 }, 'Lämpli Configuration')),
                        $q->Tr({}, $q->td({}, 'Lämpli Model:'), $q->td({}, $q->popup_menu($modelSelectArgs))),
                        $q->Tr({}, $q->td({}, 'LED Driver:'), $q->td({}, $q->popup_menu($driverSelectArgs))),
                        $q->Tr({}, $q->td({}, 'LED Colours:'), $q->td({}, $q->popup_menu($orderSelectArgs))),
                        $q->Tr({}, $q->td({}, 'LED Brightness:'), $q->td({}, $q->popup_menu($brightSelectArgs))),
                        $q->Tr({}, $q->td({}, 'Noise Level:'), $q->td({}, $q->popup_menu($noiseSelectArgs))),
                        $q->Tr({}, $q->td({}, 'Lämpli Name:'), $q->td({}, $q->input($nameInputArgs))),
                        $q->Tr({ }, $q->td({ -colspan => 2, -align => 'center' }, $q->submit(-value => 'save config'))),
                       ),
              ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
              $q->hidden(-name => 'cmd', -default => 'cfgdevice'),
              $q->hidden(-name => 'client', -default => $clientId),
              $q->hidden(-name => 'redirect', -default => "cmd=gui#config-$clientId"),
              $q->end_form()
             );

    # commands
    # my $cmdSelectArgs =
    # {
    #     -name         => 'cfgcmd',
    #     -values       => [ '', qw(reset reconnect identify random chewie hello dummy) ],
    #     -autocomplete => 'off',
    #     -default      => '',
    # };
    # my $htmlCommands =
    #   $q->div({  },
    #           $q->start_form(-method => 'POST', -action => $q->url() ),
    #           $q->table({},
    #                     $q->Tr({}, $q->td({}, 'command:'), $q->td({}, $q->popup_menu($cmdSelectArgs))),
    #                     $q->Tr({ }, $q->td({ -colspan => 2, -align => 'center' }, $q->submit(-value => 'send command'))),
    #                    ),
    #           $q->hidden(-name => 'cmd', -default => 'cfgcmd'),
    #           $q->end_form()
    #          );

    my @cmdHiddenFields = (
                           ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                           $q->hidden(-name => 'client', -default => $clientId),
                           $q->hidden(-name => 'cmd', -default => 'cfgcmd'),
                           $q->hidden(-name => 'redirect', -default => "cmd=gui#config-$clientId"),
                          );
    my @commandForms = map
    {
        $q->start_form(-method => 'POST', -action => $q->url()),
        @cmdHiddenFields,
        $q->hidden(-name => 'cfgcmd', -default => $_),
        $q->submit(-value => $_),
        $q->end_form(),
    } qw(reset reconnect identify random chewie hello dummy);
    my $htmlCommands =
      $q->div({  },
              $q->table({},
                        $q->Tr({}, $q->th({}, 'Commands')),
                        $q->Tr({}, $q->td({ -class => 'command-buttons' },
                                          @commandForms,
                                          $q->start_form(-method => 'POST', -action => $q->url() ),
                                          $q->hidden(-name => 'cmd', -default => 'rmclient'),
                                          $q->hidden(-name => 'client', -default => $clientId),
                                          ($debug ? $q->hidden(-name => 'debug', -default => $debug ) : ''),
                                          $q->submit(-value => 'delete info & config'),
                                          $q->hidden(-name => 'redirect', -default => "cmd=gui#config-$clientId"),
                                          $q->end_form()
                                         )),
                       )
              );

    # info (and raw config)
    my $htmlInfo =
      $q->div({ },
              $q->table({},
                        $q->Tr({}, $q->th({ -colspan => 2 }, 'Client Info')),
                        (map { $q->Tr({},
                                      $q->td({}, $_),
                                      $q->td({}, $_ =~ m{^(ts|check)$} ?  $client->{$_} . ' (' . _age_str(time(), $client->{$_}) . ')' : $client->{$_})
                                     ) } sort keys %{$client}),
                        #(map { $q->Tr({}, $q->th({}, $_), $q->td({}, $config->{$_})) } sort keys %{$config}),
                       ),
             );

    return (
            $q->div({ -class => 'config-left' }, $htmlJobs ),
            $q->div({ -class => 'config-right' }, $htmlDevice, $htmlCommands, $htmlInfo),
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

sub _age_str
{
    my ($now, $then) = @_;
    my $dt = $now - $then;
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

__END__
