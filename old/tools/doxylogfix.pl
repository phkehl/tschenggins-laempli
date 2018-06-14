#!/usr/bin/perl
################################################################################
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;

my @lines = grep { $_ } map { $_ =~ s/\r?\n//; $_ } <STDIN>;

for (my $ix =  $#lines; $ix > 0; $ix--)
{
    if ($lines[$ix] =~ m/^\s+/)
    {
        $lines[$ix-1] .= " $lines[$ix]";
        $lines[$ix] = undef;
    }
}


print(map { "$_\n" } grep
{
    my $l = $_;
    #print(STDERR "$l\n");
    defined $l &&
    ($l !~ m{^3rdparty/}) &&
    ($l !~ m{^examples/}) &&
    # STFU, Doxygen
    ($l !~  m{^.+\.[ch]:[0-9]: warning: Unsupported xml/html}) &&
    ($l !~ m{^.+\.c:[0-9]+:.*documented symbol.*was not declared or defined})
} @lines);

1;
__END__
