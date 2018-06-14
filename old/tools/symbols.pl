#!/usr/bin/perl
################################################################################
#
# show ram symbols
#
# Usage: objdump -t img.elf | ramsyms
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;

die("Usage: objdump -t foo.elf | $0 <regName> <regStart> <regSize>") unless ($#ARGV == 2);

my $regName  = $ARGV[0];
my $regStart = 1 * ($ARGV[1] =~ m{^0x|h$} ? hex($ARGV[1]) : $ARGV[1]);
my $regSize  = 1 * ($ARGV[2] =~ m{^0x|h$} ? hex($ARGV[2]) : $ARGV[2]);

# dRAM 0x3ffe8000 0x14000
# iROM 0x40200000 0x5c000
# iRAM 0x40100000 0x8000


my $romStart = 0x40200000;
my $romSize = 0x5c000; #0x100000;


my @symbols = ();
while (<STDIN>)
{
    #if (m/^(([0-9a-f]+).*(\.bss|\.data|\.noinit|\.stack|\.text|\.relocate)\s+([0-9a-f]+)\s+(.*))\r*\n*$/i)
    if (m/^(([0-9a-fA-F]+).*(\..+)\s+([0-9a-fA-F]+)\s+(.*))\r*\n*$/i)
    {
        my ($line, $addr, $sec, $size, $sym) = ($1, hex($2), $3, hex($4), $5);
        #printf("--> %x $addr / $sec / $size / $sym\n", $addr);
        if (#$size &&
            ($addr >= $regStart) && ($addr < ($regStart + $regSize)))
        {
            #printf("--> %x $addr / $sec / $size / $sym\n", $addr);
            push(@symbols, { addr => $addr, sec => $sec, size => $size, sym => $sym });
        }
    }
}

my $totSize = 0;
$totSize += $_->{size} for (@symbols);
my $totPadding = 0;

print("***** symbols by address *****\n");
my $prevS = undef;
foreach my $s (sort { $a->{addr} <=> $b->{addr} } @symbols)
{
    my $comment = '';
    if ($prevS)
    {
        if ($s->{addr} > ($prevS->{addr} + $prevS->{size}))
        {
            my $padding = $s->{addr} - ($prevS->{addr} + $prevS->{size});
            printf("0x%08x  %05u  0x%04x   ???                                              padding, rodata or SDK stuff\n", $s->{addr} - $padding, $padding, $padding);
            #$comment = "padded " . ();
            $totPadding += $padding;
        }
        elsif ($s->{addr} < ($prevS->{addr} + $prevS->{size}))
        {
            $comment = "overlap";
            $totSize -= ($prevS->{addr} + $prevS->{size}) - $s->{addr};
        }
    }

    printf("0x%08x  %05u  0x%04x  %-12s  %-35s %s\n",
           $s->{addr}, $s->{size}, $s->{size}, $s->{sec}, $s->{sym}, $comment);
    $prevS = $s;
}
$totSize += $totPadding;
printf("            %05u 0x%04x (%.2f%%)\n", $totSize, $totSize, $totSize / $regSize * 1e2);

push(@symbols, { size => $totPadding, sec => 'dunno', sym => 'padding and SDK stuff' });


print("\n***** symbols by size *****\n");
foreach my $s (sort { $b->{size} <=> $a->{size} or $a->{sym} cmp $b->{sym} } @symbols)
{
    printf("%05u  0x%04x  %-12s  %s\n", $s->{size}, $s->{size}, $s->{sec}, $s->{sym});
}
printf("%05u  0x%04x (%.2f%%)\n", $totSize, $totSize, $totSize / $regSize * 1e2);

print("\n***** size by section *****\n");
my %sections = ();
foreach my $s (@symbols)
{
    $sections{$s->{sec}} += $s->{size};
}
foreach my $s (sort { $sections{$a} <=> $sections{$b} } keys %sections)
{
    printf("%05u 0x%05x %s\n", $sections{$s}, $sections{$s}, $s);
}

printf("\n\ntotal %s (0x%08x+0x%05x) usage: %6u/%6u (%.1f%%) %6u bytes free\n",
       $regName, $regStart, $regSize, $totSize, $regSize, $totSize / $regSize * 1e2, $regSize - $totSize);

################################################################################
1;
__END__
