#!/usr/bin/perl

=pod

=encoding utf8

=head1 tschenggins-status.pl -- Tschenggins Lämpli Backend

Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
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
use Pod::Usage;

my $q = CGI->new();

my @DEBUGSTRS     = ();
my $DATADIR       = $FindBin::Bin;

my $VALIDRESULT   = { unknown => 1, success => 2, unstable => 3, failure => 4 };
my $VALIDSTATE    = { unknown => 1, running => 2, idle => 3 };
my $UNKSTATE      = { name => 'unknown', server => 'unknown', result => 'unknown', state => 'unknown', ts => time() };
my $JOBNAMERE     = qr{^[-_a-zA-Z0-9]{5,50}$};
my $JOBIDRE       = qr{^[0-9a-z]{8,8}$};
my $DEFAULTCMD    = 'gui';
my $DBFILE        = $ENV{'REMOTE_USER'} ? "$DATADIR/tschenggins-status-$ENV{'REMOTE_USER'}.json" : "$DATADIR/tschenggins-status.json";

#DEBUG("DATADIR=%s, VALIDRESULT=%s, VALIDSTATE=%s", $DATADIR, $VALIDRESULT, $VALIDSTATE);

do
{
    my $TITLE  = 'Tschenggins Lämpli';

    ##### get parameters #####

=pod

=head2 Possible Parameters

=over

=item * C<cmd> -- the command

=item * C<debug> -- debugging on (1) or off (0, default)

=item * C<job> -- job ID

=item * C<result> -- job result ('unknown', 'success', 'unstable', 'failure')

=item * C<state> -- job state ('unknown', 'running', 'idle')

=item * C<redirct> -- C<cmd> to redirect to

=item * C<ascii> -- US-ASCII output (1) or UTF-8 (0, default)

=item * C<client> -- client ID

=item * C<server> -- server name

=item * C<offset> -- offset into list of results (default 0)

=item * C<limit> -- limit of list of results (default 0, i.e. all)

=item * C<strlen> -- chop long strings at length (default 256)

=item * C<name> -- client name

=item * C<staip> -- client station IP address

=item * C<version> -- client software version

=item * C<led> -- (multiple) job ID for the LEDs

=back

=cut

    # query string, application/x-www-form-urlencoded or multipart/form-data
    my $cmd      = $q->param('cmd')      || $DEFAULTCMD;
    my $debug    = $q->param('debug')    || 0;
    my $job      = $q->param('job')      || '';
    my $result   = $q->param('result')   || ''; # 'unknown', 'success', 'unstable', 'failure'
    my $state    = $q->param('state')    || ''; # 'unknown', 'running', 'idle'
    my $redirect = $q->param('redirect') || '';
    my $ascii    = $q->param('ascii')    || 0;
    my $client   = $q->param('client')   || ''; # client id
    my $server   = $q->param('server')   || ''; # server name
    my $offset   = $q->param('offset')   || 0;
    my $limit    = $q->param('limit')    || 0;
    my $strlen   = $q->param('strlen')   || 256;
    my $name     = $q->param('name')     || '';
    my $staip    = $q->param('staip')    || '';
    my $version  = $q->param('version')  || '';
    my @states   = (); # $q->multi_param('states');
    my @ch       = $q->multi_param('ch');

    # text/json
    my $contentType = $q->content_type();
    if ( $contentType && ($contentType eq 'text/json') )
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
    $q->param('debug', $debug);
    DEBUG("cmd=%s user=%s", $cmd, $ENV{'REMOTE_USER'} || 'anonymous');

    ##### output is HTML, JSON or text #####

    my @html = ();
    my $data = undef;
    my $text = '';
    my $error = '';


    ##### load "database" #####

    # open database
    my $dbHandle;
    my $db;
    unless (open($dbHandle, '+>>', $DBFILE))
    {
        $error = "failed opening database file";
        $cmd = '';
    }
    unless (flock($dbHandle, LOCK_EX))
    {
        $error = "failed locking database file";
        $cmd = '';
    }

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
    foreach my $clientId (sort keys %{$db->{clients}})
    {
        push(@clientIds, $clientId);
    }
    $db->{_clientIds} = \@clientIds;
    #DEBUG("db=%s", Dumper($db));


    ##### handle requests #####

=pod

=head2 Commands

=cut

=pod

=head3 C<< cmd=help >>

Print help.

=cut

    # help
    if ($cmd eq 'help')
    {
        my $outFh;
        open($outFh, '>', \$text);
        pod2usage({ -output => $outFh, -exitval => 'NOEXIT', -verbose => 3,
                    #-perldocopt => '-ohtml'
                  });
        close($outFh);
    }

=pod

=head3 C<< cmd=hello >> or C<< cmd=delay >>

Connection check. Responds with "hi there" (immediately or after a delay).

=cut

    # connection check
    elsif ($cmd eq 'hello')
    {
        $text = 'hi there';
    }
    elsif ($cmd eq 'delay')
    {
        sleep(2);
        $text = 'hi there';
    }

=pod

=head3 C<< cmd=update >>

Interface for C<tschenggins-watcher.pl>.

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

=head3 C<< cmd=gui >>

The GUI.

=cut

    # GUI
    elsif ($cmd eq 'gui')
    {
        push(@html, _gui($db));
    }

=pod

=head3 C<< cmd=set job=<jobid> state=<state string> result=<result string> >>

Set job state and/or result.

=cut

    # set entry state and/or result
    elsif ($cmd eq 'set')
    {
        (my $res, $error) = _set($db, $job, $state, $result);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=head3 C<< cmd=add job=<job name> server=<server name> state=<state string> result=<result string> >>

Add job.

=cut

    # add entry
    elsif ($cmd eq 'add')
    {
        (my $res, $error) = _add($db, $server, $job, $state, $result);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=head3 C<< cmd=del job=<jobid> >>

Delete job.

=cut

    # delete entry
    elsif ($cmd eq 'del')
    {
        (my $res, $error) = _del($db, $job);
        if ($res)
        {
            $text = "db updated";
        }
    }

=pod

=head3 C<< cmd=rawdb >>

Returns the raw database (JSON).

=cut

    # raw db
    elsif ($cmd eq 'rawdb')
    {
        delete $db->{_dirtiness};
        $data = { db => $db, res => 1 };
    }

=pod

=head3 C<< cmd=list offset=<number> limit=<number> >>

List available jobs with optional offset and/or limit.

=cut

    # list of jobs
    elsif ($cmd eq 'list')
    {
        ($data, $error) = _list($db, $offset, $limit);
    }

=pod

=head3 C<< cmd=get job=<jobid> >>

Get job state.

=cut

    # get job
    elsif ($cmd eq 'get')
    {
        ($data, $error) = _get($db, $job);
    }

=pod

=head3 C<< cmd=rmclient client=<clientid> >>

Remove client info.

=cut

    # remove client data
    elsif ($cmd eq 'rmclient')
    {
        if ($client && $db->{clients}->{$client})
        {
            delete $db->{clients}->{$client};
            $db->{_dirtiness}++;
            $text = "client $client removed";
        }
        else
        {
            $error = 'illegal parameter';
        }
    }

=pod

=head3 C<< cmd=leds client=<clientid> name=<client name> staip=<client station IP> version=<client sw version> ch=<jobid> ... strlen=<number> >>

Returns info for a client given one or more job IDs.

=head3 C<< cmd=realtime client=<clientid> name=<client name> staip=<client station IP> version=<client sw version> ch=<jobid> ... strlen=<number> >>

Returns info for a client given one or more job IDs. Endless connection with real-time update as things happen.

=cut

    # LEDs results for client
    elsif ($cmd eq 'leds')
    {
        ($data, $error) = _leds($db, $client, $strlen, { name => $name, staip => $staip, version => $version }, @ch);
    }
    # special mode: realtime status change notification
    elsif ($cmd eq 'realtime')
    {
        # first like cmd=leds to update the client info and then below dispatch to real-time monitoring
        ($data, $error) = _leds($db, $client, $strlen, { name => $name, staip => $staip, version => $version }, @ch);

        # save pid so that previous instances can terminate in case they're still running
        # and haven't noticed yet that the Lämpli is gone (Apache waiting "forever" for TCP timeout)
        unless ($error)
        {
            $db->{clients}->{$client}->{pid} = $$;
            $db->{_dirtiness}++;
        }
    }

    # illegal command
    else
    {
        $error = 'illegal command';
    }


    ##### update database with changes #####

    if (!$error && $db->{_dirtiness})
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


    ##### real-time status monitoring #####

    if ( !$error && ($cmd eq 'realtime') )
    {
        _realtime($client, $strlen, { name => $name, staip => $staip, version => $version}, @ch); # this doesn't return
        exit(0);
    }


    ##### render output #####

    # redirect?
    if (!$error && $redirect )
    {
        $q->delete_all();
        $q->param('cmd', $redirect) unless ($redirect eq $DEFAULTCMD);
        $q->param('debug', $debug) if ($debug);
        print($q->redirect());
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
        $css .= "body { font-family: sans-serif; }\n";
        $css .= "table { padding: 0; border: 1px solid #000; border-collapse: collapse; }\n";
        $css .= "table td, table th { margin: 0; padding: 0.1em 0.25em 0.1em 0.25em;  border: 1px solid #000; }\n";
        $css .= "table th { font-weight: bold; background-color: #ddd; text-align: left; border-bottom: 1px solid #000; }\n";

        print(
              $q->header( -type => 'text/html', -expires => 'now', charset => 'UTF-8',
                          '-Access-Control-Allow-Origin' => '*' ),
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
                          '-Content-Length' => length($content), '-Access-Control-Allow-Origin' => '*' ),
              $content
             );
    }
    elsif (!$error && $data)
    {
        $data->{debug} = \@DEBUGSTRS if ($debug );
        $data->{res} = 0 unless ($data->{res});
        my $json = JSON::PP->new()->ascii($ascii ? 1 : 0)->utf8($ascii ? 0 : 1)->canonical(1)->pretty($debug ? 1 : 0)->encode($data);
        print(
              $q->header( -type => 'text/json', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                          # avoid "Transfer-Encoding: chunked" by specifying the actual content length
                          # so that the raw output will be exactly and only the json string
                          # (i.e. no https://en.wikipedia.org/wiki/Chunked_transfer_encoding markers)
                          '-Content-Length' => length($json), '-Access-Control-Allow-Origin' => '*'
                        ),
              $json
             );
    }
    else
    {
        $error ||= "illegal request parameter(s)";
        my $content = "400 Bad Request: $error\n\n" . join('', map { "$_\n" } @DEBUGSTRS);
        print(
              $q->header(-type => 'text/plain', -expires => 'now', charset => ($ascii ? 'US-ASCII' : 'UTF-8'),
                         -status => 400, '-Access-Control-Allow-Origin' => '*',
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
        my $jobName = $st->{name}   || '';
        my $jState  = $st->{state}  || '';
        my $jResult = $st->{result} || '';
        if (!$st->{server})
        {
            $error = "missing server";
            $ok = 0;
            last;
        }
        my $server = $st->{server};

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
        $db->{jobs}->{$id}->{ts}     = int(time());
        $db->{jobs}->{$id}->{name}   = $jobName;
        $db->{jobs}->{$id}->{server} = $server;
        $db->{jobs}->{$id}->{state}  = $jState       if ($jState);
        $db->{jobs}->{$id}->{result} = $jResult      if ($jResult);
        $db->{_dirtiness}++;
    }
    return $ok, $error;
}


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


sub _leds
{
    my ($db, $client, $strlen, $info, @ch) = @_;

    DEBUG("_leds() $client $strlen @ch");

    if ( !$client || ($#ch < 0) )
    {
        return undef, 'missing parameters';
    }
    my $data = { leds => [], res => 1 };
    my $now = time();
    foreach my $jobId (@ch)
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        my $state  = $st->{state};
        my $result = $st->{result};
        my $name   = $st->{name};
        my $server = $st->{server};
        my $ts     = $st->{ts};
        push(@{$data->{leds}},
             [ substr($name, 0, $strlen), substr($server, 0, $strlen), $state, $result, $ts ]);
    }

    $db->{clients}->{$client} =
    {
        leds => [ map { $_ =~ m{$JOBIDRE} ? $_ : 'ffffffff' } @ch ], ts => int($now + 0.5),
    };
    $db->{clients}->{$client}->{$_} = $info->{$_} for (keys %{$info});
    $db->{_dirtiness}++;

    return $data, '';
}


sub _gui
{
    my ($db) = @_;

    DEBUG("_gui()");
    my @html = ();
    my $debug = $q->param('debug') || 0;

    ##### Jobs #####

    push(@html, $q->h2('Jobs'));

    # results
    push(@html, $q->h3('Results'), _gui_results($db, @{$db->{_jobIds}}));

    # helpers
    my $jobSelectArgs =
    {
        -name         => 'job',
        -values       => [ '', @{$db->{_jobIds}} ],
        -labels       => { map { $_, "$db->{jobs}->{$_}->{server}: $db->{jobs}->{$_}->{name}" } @{$db->{_jobIds}} },
        -autocomplete => 'off',
        -default      => '',
    };
    my $stateSelectArgs =
    {
        -name         => 'state',
        -values       => [ '', sort { $VALIDSTATE->{$a} <=> $VALIDSTATE->{$b} } keys %{$VALIDSTATE} ],
        -autocomplete => 'off',
        -default      => '',
    };
    my $resultSelectArgs =
    {
        -name         => 'result',
        -values       => [ '', sort { $VALIDRESULT->{$a} <=> $VALIDRESULT->{$b} } keys %{$VALIDRESULT} ],
        -autocomplete => 'off',
        -default      => '',
    };

    # override
    push(@html, $q->h3('Override'));
    push(@html, $q->start_form(-method => 'GET', -action => $q->url() ));
    push(@html,
         $q->table(
                   $q->Tr(
                          $q->td('job: '),
                          $q->td($q->popup_menu($jobSelectArgs))
                         ),
                   $q->Tr(
                          $q->td('state: '),
                          $q->td($q->popup_menu($stateSelectArgs)),
                         ),
                   $q->Tr(
                          $q->td('result: '),
                          $q->td($q->popup_menu($resultSelectArgs)),
                         ),
                   $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                 $q->submit(-value => 'override')))
                   )
        );
    push(@html, $q->hidden(-name => 'debug', -default => $debug )) if ($debug);
    push(@html, $q->hidden(-name => 'cmd', -default => 'set'));
    push(@html, $q->hidden(-name => 'redirect', -default => 'gui'));
    push(@html, $q->end_form());

    # create
    push(@html, $q->h3('Create'));
    push(@html, $q->start_form(-method => 'GET', -action => $q->url() ));
    push(@html,
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
                          $q->td($q->popup_menu($stateSelectArgs)),
                         ),
                   $q->Tr(
                          $q->td('result: '),
                          $q->td($q->popup_menu($resultSelectArgs)),
                         ),
                   $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                 $q->submit(-value => 'create')))
                   )
        );
    push(@html, $q->hidden(-name => 'debug', -default => $debug )) if ($debug);
    push(@html, $q->hidden(-name => 'cmd', -default => 'add'));
    push(@html, $q->hidden(-name => 'redirect', -default => 'gui'));
    push(@html, $q->end_form());

    # delete
    push(@html, $q->h3('Delete'));
    push(@html, $q->start_form(-method => 'GET', -action => $q->url() ));
    push(@html,
         $q->table(
                   $q->Tr(
                          $q->td('job: '),
                          $q->td($q->popup_menu($jobSelectArgs)),
                         ),
                   $q->Tr($q->td({ -colspan => 2, -align => 'center' },
                                 $q->submit(-value => 'delete')))
                   )
        );
    push(@html, $q->hidden(-name => 'debug', -default => $debug )) if ($debug);
    push(@html, $q->hidden(-name => 'cmd', -default => 'del'));
    push(@html, $q->hidden(-name => 'redirect', -default => 'gui'));
    push(@html, $q->end_form());


    ##### Clients #####

    push(@html, $q->h2('Clients'));

    do
    {
        foreach my $clientId (@{$db->{_clientIds}})
        {
            my $cl = $db->{clients}->{$clientId};
            push(@html,
                 $q->h3($cl->{name} . ' (' . $clientId . ')'),
                 $q->table(
                           $q->Tr(
                                  $q->th('Last connect:'), $q->td(sprintf('%.1f hours ago', (time() - $cl->{ts}) / 3600.0)),
                                  $q->th('Station IP:'), $q->td($cl->{staip}),
                                  $q->th('Version:'), $q->td($cl->{version})
                                 ),
                          ),
                 $q->br(),
                 _gui_results($db, @{$cl->{leds}}),
                 $q->a({ -href => $q->url() . '?cmd=rmclient;client=' . $clientId . ';redirect=gui' }, 'delete info'),
                );
        }
    };


    push(@html, $q->br(), $q->br(),
         $q->a({ -href => ($q->url() . '?cmd=help') }, 'help'), ', ',
         $q->a({ -href => ($q->url() . '?cmd=rawdb;debug=1') }, 'raw db'));

    return @html;
}

sub _gui_results
{
    my ($db, @jobIds) = @_;
    my $now = time();
    my @trs = ();
    foreach my $jobId (@jobIds)
    {
        my $st = $db->{jobs}->{$jobId} || $UNKSTATE;
        push(@trs, $q->Tr($q->td($jobId), $q->td($st->{server}), $q->td($st->{name}), $q->td($st->{state}), $q->td($st->{result}),
                          $q->td({ -align => 'right' }, sprintf('%.1fh', ($now - $st->{ts}) / 3600))));
    }
    return $q->table($q->Tr($q->th('id'), $q->th('server'), $q->th('job'), $q->th('state'), $q->th('result'), $q->th('age')),
                     @trs);
}

sub _realtime
{
    my ($client, $strlen, $info, $staip, $version, @ch) = @_;
    print($q->header(-type => 'text/plain', -expires => 'now', charset => 'US-ASCII'));
    my $n = 0;
    my $lastTs = 0;
    my $lastStatus = '';
    my $lastCheck = 0;
    my $startTs = time();
    $|++;
    print("hello $client $strlen $info->{name} @ch\n");
    while (1)
    {
        sleep(1);
        my $now = time();
        if ( ($n % 5) == 0 )
        {
            my $nowInt = int($now + 0.5);
            printf("heartbeat $nowInt $n\r\n");
        }
        $n++;

        # don't run forever
        if ( ($now - $startTs) > (4 * 3600) )
        {
            exit(0);
        }

        # check database?
        my $doCheck = 0;

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
            #printf("debug change $ts\n");

            # load database
            my $dbHandle;
            unless (open($dbHandle, '<', $DBFILE))
            {
                next;
            }
            unless (flock($dbHandle, LOCK_EX))
            {
                close($dbHandle);
                next;
            }
            my $db;
            eval
            {
                local $/;
                my $dbJson = <$dbHandle>;
                $db = JSON::PP->new()->utf8()->decode($dbJson);
            };
            close($dbHandle);

            # check if we're interested
            if ($db && $db->{clients} && $db->{clients}->{$client})
            {
                # exit if we're no longer in charge
                if ($db->{clients}->{$client}->{pid} && ($db->{clients}->{$client}->{pid} != $$))
                {
                    exit(0);
                }

                # same as cmd=led to get the data but we won't store the database (this updates $db->{clients}->{$client})
                my ($data, $error) = _leds($db, $client, $strlen, $info, $staip, $version, @ch);
                if ($error)
                {
                    print("error $error\r\n");
                }
                elsif ($data)
                {
                    my $status = join(' ', map { @{$_} } @{$data->{leds}});
                    if ($status ne $lastStatus)
                    {
                        #print("debug $lastStatus\n");
                        #print("debug $status\n");
                        my $json = JSON::PP->new()->ascii(1)->canonical(1)->pretty(0)->encode($data);
                        my $now = int(time() + 0.5);
                        print("status $now $json\r\n");
                        $lastStatus = $status;
                    }
                }
            }
        }

        # FIXME: if we just could read some data from the client here to determine if it is still alive..
        # reading from STDIN doesn't show any data... :-(
    }
}

__END__
