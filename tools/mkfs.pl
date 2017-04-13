#!/usr/bin/perl
################################################################################
#
# Jenkins Lämpli -- filesystem tool
#
# Helper for creating the filesystem for larger files. See src/user_fs.h.
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin";

use Time::HiRes qw(time sleep usleep);

use Ffi::Debug ':all';


my $imgFile = '';
my @inFiles = ();

# parse command line
while (my $arg = shift(@ARGV))
{
    if    ($arg eq '-v') { $Ffi::Debug::VERBOSITY++; }
    elsif ($arg eq '-q') { $Ffi::Debug::VERBOSITY--; }
    elsif (!$imgFile) { $imgFile = $arg; }
    elsif (-f $arg)
    {
        push(@inFiles, $arg);
    }
    else
    {
        ERROR("Illegal argument '%s'!", $arg);
        exit(1);
    }
}

DEBUG('imgFile=%s inFiles=%s', $imgFile, \@inFiles);

if ( !$imgFile || ($#inFiles < 0) )
{
    PRINT("Usage:",
          "",
          "  $0 [-v] [-q] <imgfile> <file> ...",
          "",
         );
    exit(1);
}

my $sectSize = 0x1000; # 4k

# create filesystem

# read all files
my @files = ();
foreach my $file (@inFiles)
{
    my $name = $file;
    $name =~ s{.*/}{};
    die("name too long: $name") unless (length($name) < 32);

    # read file
    my $fsize = -s $file;
    open(F, '<', $file);
    binmode(F);
    my $data;
    read(F, $data, $fsize);
    close(F);
    my $dsize = length($data);
    die("size mismatch $fsize != $dsize") unless ($fsize == $dsize);
    DEBUG("mkfs: %-32s %6i %s", $name, $fsize, $file);

    my $type = 'application/octet-stream';
    if ($name =~ m{\.js$})
    {
        $type = 'text/javascript';
    }
    elsif ($name =~ m{(\.json|\.map)$})
    {
        $type = 'application/json';
    }
    elsif ($name =~ m{\.css$})
    {
        $type = 'text/css';
    }
    elsif ($name =~ m{\.jpe?g$})
    {
        $type = 'image/jpeg';
    }
    elsif ($name =~ m{\.png$})
    {
        $type = 'image/png';
    }

    push(@files, { name => $name, data => $data, size => $fsize, type => $type });
}


# calculate fs layout
my $offset = 0;
my $totsize = 0;
PRINT("mkfs: ix name                             offset       size type");
for (my $ix = 0; $ix <= $#files; $ix++)
{
    my $f = $files[$ix];
    $f->{offset} = $offset;
    my $nSect = int( ($f->{size} + ($sectSize - 1)) / $sectSize );
    $f->{padsize} = $nSect * $sectSize;
    $offset += $f->{padsize};
    $totsize += $f->{size};

    PRINT("mkfs: %2i %-32s %6i (%3i) %6i (%6i, %3i) %s",
          $ix, $f->{name}, $f->{offset}, $f->{offset} / $sectSize, $f->{size},
          $f->{padsize}, $f->{padsize} / $sectSize, $f->{type});
}
PRINT("mkfs: total %i files, %i bytes (%.1fkb) net, %i bytes (%.1fkb, %i sectors) with padding",
      $#files + 1, $totsize, $totsize / 1024, $offset, $offset / 1024, $offset / $sectSize);

# make filesystem data
my $toc = '';
my $data = '';
for (my $ix = 0; $ix <= $#files; $ix++)
{
    my $f = $files[$ix];
    my $d = pack("C$f->{padsize}", unpack('C*', $f->{data}));
    $data .= $d;

    # like FS_TOC_ENTRY_t in user_fs.c
    my $t = pack('Z32Z32VVNCCCC', $f->{name}, $f->{type}, $f->{size}, $f->{offset}, 0xb16b00b5, 0xff, 0xff, 0xff, 0xff);
    $toc .= $t;
}
die("toc too big, too many files") if (length($toc) > $sectSize);

$toc = pack("C$sectSize", unpack('C*', $toc));

# write fs image
PRINT("mkfs: writing $imgFile");
open(F, '>', $imgFile);
binmode(F);
print(F $toc);
print(F $data);
close(F);



__END__
