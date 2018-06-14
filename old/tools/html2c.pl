#!/usr/bin/perl
################################################################################
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
# https://oinkzwurgl.org/projaeggd/tschenggins-laempli
#
################################################################################

use strict;
use warnings;
use HTML::Packer;
use JavaScript::Packer;
use CSS::Packer;
#use Text::Wrap;

my $debug = 0;

print("#ifndef __HTML_GEN_H__\n");
print("#define __HTML_GEN_H__\n");
print("\n\n");
print("#include \"version_gen.h\"\n");
print("\n\n");

foreach my $infile (@ARGV)
{
    open(IN, '<', $infile) || die("Cannot read $infile: $!");
    my $html = do { local $/; <IN> };
    close(IN);

    print(STDERR "\n\n\n$html") if ($debug);

    my $packer = HTML::Packer->init();

    my $opts =
    {
        remove_comments      => 1,
        remove_newlines      => 1,
        do_javascript        => 'best',
        do_stylesheet        => 'minify',
        no_compress_comments => 1,
        html5                => 1,
    };

    my $packed = $packer->minify(\$html, $opts);

    #$packed =~ s{>\s+<}{>}g; # too aggressive

    # escape for c string
    $packed =~ s{"}{\\"}g;

    # replace #define-d stuff
    $packed =~ s{%(FF_[A-Z_]+)%}{" $1 "}g;

    # bug in packer?
    $packed =~ s{keyframes}{keyframes }g;

    print(STDERR "\n\n\n$packed") if ($debug);

    # wrap
    #$Text::Wrap::columns = 100;
    #$Text::Wrap::break = '[\s>:]';
    #$Text::Wrap::unexpand = 0;
    #$Text::Wrap::separator = "\"\n";
    #
    print(STDERR "\n\nwrap..\n\n") if ($debug);

    #my $wrapped = Text::Wrap::wrap("", "    \"", "static const char foo[] PROGMEM = \"$packed\";");

    #
    my $wrapped = $packed;
    #$wrapped =~ s{(.{90,}?[:>])}{    "$1"\n}g;
    #$wrapped =~ s{([^\n]+$)}{    "$1"};

    $wrapped =~ s{(.{90,}?[:>])}{$1\n}g;
    $wrapped =~ s{\n\s*([^\n]+$)}{$1}m;
    $wrapped = join(" \\\n", map { "    \"$_\"" } split("\n", $wrapped));

    #$wrapped =~ s{\n}{ \\\n}g;

    print(STDERR "\n\n\n$wrapped") if ($debug);

    my $defname = $infile;
    $defname =~ s{.*/}{};
    $defname =~ s{\.}{_}g;
    $defname = uc($defname) . '_STR';

    print("//----- $infile -----\n");
    print(map { $_ =~ s{\s+$}{}; $_ ? "// $_\n" : "//\n" } split(/\r?\n/, $html));
    printf("#define $defname /* %i -> %i bytes */ \\\n", length($html), length($packed));
    print($wrapped);
    print("\n\n");
}


print("\n#endif // __HTML_GEN_H__\n");

exit(0);
