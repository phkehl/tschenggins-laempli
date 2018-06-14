#!/usr/bin/perl
################################################################################
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;


my $dir = shift(@ARGV);
if ($dir)
{
    chdir($dir);
}

my $debug = 0;

my $svnversion = qx{LC_ALL=C svnversion 2>/dev/null};
$svnversion =~ s{\r?\n}{};
if ($svnversion =~ m{(unversioned|uncommited|copy or move)}i)
{
    $svnversion = '';
}
print(STDERR "svnversion=[$svnversion]\n") if ($debug);
$svnversion =~ s{.*:}{};
if ($svnversion)
{
    print("r$svnversion");
    exit(0);
}

my $hash = qx{LC_ALL=C git log --pretty=format:%H -n1 2>/dev/null};
my $dirty = qx{LC_ALL=C git describe --always --dirty 2>/dev/null};
$hash =~ s{\r?\n}{};
$dirty =~ s{\r?\n}{};
print(STDERR "hash=[$hash]\ndirty=[$dirty]\n") if ($debug);

if ($hash && $dirty)
{
    my $str = substr($hash, 0, 8);
    if ($dirty =~ m{-dirty$})
    {
        $str .= 'M' ;
    }
    print($str);
    exit(0);
}

print("unknown");


