#!/usr/bin/perl -w
#
# Usage : ./hanja-sanity.pl [-v] [nabi-hanja.txt]
# checks the sanity of nabi-hanja.txt and shows entries presumably wrong-commented.

require 5.8.0;

use strict;
use Carp;
use Getopt::Std;
use Fcntl qw/:flock/;
use encoding qw/utf8/;

# options
our $opt_v;
getopts('v');

my $file = shift || "nabi-hanja.txt";

# open file
open  IN, "<:utf8",$file  or croak "cannot open `$file'";
flock IN, LOCK_SH         or croak "cannot shlock `$file'";

# loop
my ($p,$flag) = (undef,0);
foreach (<IN>)
{
  # check if new pronunciation found
  if (m/^\[(.)\]$/) { ($p,$flag) = ($1,0); next; }

  # check sanity
  next if m/^.=(?:[^,]+ $p, )*[^,]+ $p$/;   # ordinary case
  next if m/^.=.\x{c758} (?:\x{7c21}\x{9ad4}|\x{53e4}|\x{7565}|\x{672c}|\x{4fd7}|\x{8a1b}|\x{8b4c}|\x{6b63}|\x{7c21}(?:\x{9ad4}|\x{4f53}))\x{5b57}$/;   # misc
  next if m/^.=.\x{acfc} \x{540c}\x{5b57}$/ &&  (((ord $p)-hex"ac00")%28);    # has jongseong
  next if m/^.=.\x{c640} \x{540c}\x{5b57}$/ && !(((ord $p)-hex"ac00")%28);    # has no jongseong
  next if m/^(.)=$/ && (((ord $1)&0xf000)!=hex"f000");    # has no meaning and not a char for compatibility
  next if m/^$/;      # possible blank lines

  # check an avoidance of liquid at the head phoneme unless verbose
  unless ($opt_v || (my @p=&_alternate_pronunciations($p))<2)
  {
    my $ap = "(?:".+(join"|",@p).")";
    next if m/^.=(?:[^,]+ $ap, )*[^,]+ $ap$/;
  }

  # print
  print "[$p]\n" unless $flag++;
  print "U+".+(uc unpack "H4", pack "n", ord $1)." $_" if m/^(.)/;
}

# close file
flock IN, LOCK_UN;
close IN;

# returns all possible pronunciations
sub _alternate_pronunciations
{
  my $p = shift;
  my %t = ( "\x{b140}" => "\x{c5ec}", #  NEYO =>  YEO   : Nasal#{{{
            "\x{b1e8}" => "\x{c694}", #   NYO =>   YO
            "\x{b274}" => "\x{c720}", #   NYU =>   YU
            "\x{b2c8}" => "\x{c774}", #    NI =>    I
            "\x{b7b4}" => "\x{c57c}", #   RYA =>   YA   : Liquid
            "\x{b824}" => "\x{c5ec}", #  RYEO =>  YEO
            "\x{b840}" => "\x{c608}", #   RYE =>   YE
            "\x{b8cc}" => "\x{c694}", #   RYO =>   YO
            "\x{b958}" => "\x{c720}", #   RYU =>   YU
            "\x{b9ac}" => "\x{c774}", #    RI =>    I
            "\x{b82c}" => "\x{c5f4}", # RYEOL => YEOL
            "\x{b960}" => "\x{c728}", #  RYUL =>  YUL
            "\x{b77c}" => "\x{b098}", #    RA =>   NA
            "\x{b798}" => "\x{b0b4}", #   RAE =>  NAE
            "\x{b85c}" => "\x{b178}", #    RO =>   NO
            "\x{b8b0}" => "\x{b1cc}", #   ROE =>  NOE
            "\x{b8e8}" => "\x{b204}", #    RU =>   NU
            "\x{b974}" => "\x{b290}", #   REU =>  NEU
  );#}}}
  my @p = ($p);
  my $jong = ((ord $p)-hex"ac00")%28; # JONGSEONG of $p
  $p = pack "U", ord($p)-$jong;
  map { push @p, pack "U", ord($_)+$jong if $t{$_} eq $p; } keys %t;
  return @p;
}
