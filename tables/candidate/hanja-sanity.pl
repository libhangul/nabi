#!/usr/bin/perl -w
#
# by Chloe Lewis
# check nabi-hanja.txt for its sanity
#
# $Id$

require 5.8.2;

use strict;
use encoding "utf8";

my $_h = "nabi-hanja.txt";

# open file
open  IN, "<:utf8",$_h or die "cannot open `$_h'";
flock IN, 1            or die "cannot shlock `$_h'";
my @hanja = <IN>;
flock IN, 8            or die "cannot unlock `$_h'";
close IN               or die "cannot close `$_h'";

my ($p,$flag) = (undef,0);
foreach (@hanja)
{
  # check if new pronunciation found
  if (m/^\[(.)\]$/) { ($p,$flag) = ($1,0); next; }

  # check sanity
  next if m/^.=(?:[^,]+ $p, )*[^,]+ $p$/;   # ordinary case
  next if m/^.=.\x{c758} (?:\x{7c21}\x{9ad4}|\x{53e4}|\x{7565}|\x{672c}|\x{4fd7}|\x{8a1b}|\x{8b4c}|\x{6b63})\x{5b57}$/;   # misc
  next if m/^.=.\x{acfc} \x{540c}\x{5b57}$/ &&  (((ord $p)-hex"ac00")%28);    # has jongseong
  next if m/^.=.\x{c640} \x{540c}\x{5b57}$/ && !(((ord $p)-hex"ac00")%28);    # has no jongseong
  next if m/^(.)=$/ && (((ord $1)&0xf000)!=hex"f000");    # has no meaning and not a char for compatibility
  next if m/^$/;      # blank line at the end of file

  # print
  print "[$p]\n" unless $flag++;
  print "U+".+(unpack "H4", pack "n", ord $1)." $_" if m/^(.)/;
}

