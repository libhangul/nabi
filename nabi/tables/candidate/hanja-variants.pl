#!/usr/bin/perl -w
#
# Usage : ./hanja-variants.pl [-v] [Unihan.txt]
# checks the sanity of {simplified,traditional} variants with kKorean.
# `-v' makes the result very verbose, showing every variants.
#
# how it works.?
# --------------
# 1. grab every single kKorean with its variant informations.
# 2. double check its variant :
#    a. `a' is one of simplified variants of `b' if `b' has a tag of kTraditionalVariant of `a'.
#    b. `a' is one of traditional variants of `b' if `b' has a tag of kSimplifiedVariant of `a'.
#    c. `a' should have a tag of kKorean, meaning korean pronunciation.
# 3. might be erroneous if :
#    a. two arrays of isSimplifiedVariantOf and kTraditionalVariant are different.
#    a. two arrays of isTraditionalVariantOf and kSimplifiedVariant are different.

require 5.8.0;

use strict;
use Carp;
use Getopt::Std;
use Fcntl qw/:flock/;
use encoding qw/utf8/;

# options
our $opt_v;
getopts('v');

# filename
my $unihan = shift || "Unihan.txt";
#my @vtags = qw/kKorean kSimplifiedVariant kTraditionalVariant kZVariant/;
my @vtags = qw/kKorean kSimplifiedVariant kTraditionalVariant/;

# open file
open  IN, "<:utf8",$unihan or croak "cannot open `$unihan'";
flock IN, LOCK_SH          or croak "cannot shlock `$unihan'";

# loop
my $char = { };   # hashref of kKorean
my $this = undef; # temporary
while (<IN>)
{
  # split
  my ($hex,$tag,$value) = split /\t/, $_, 3;
  next if $hex =~ m/^#/;  # skip if commented
  # push the information of previous char if new char found
  if (defined $this->{'hex'} && $this->{'hex'} ne $hex)
  {
    unless (defined $this->{'kKorean'}) { undef $this; next; }  # skip if no kKorean
    map { $char->{$this->{'hex'}}->{$_} = $this->{$_} } grep ! /^hex$/, keys %{$this};
    undef $this;
  }

  # this char
  $this->{'hex'} = $hex;
  foreach (@vtags)
  {
    push @{$this->{$_}}, split /\s+/, $value if $tag eq $_;
    last if $tag eq $_; # foreach (@vtags)
  }
} # while (<IN>)

# close file
flock IN, LOCK_UN;
close IN;

# $char is now hashref of whole kKorean :
# $char->{'U+XXXX'}->{'kKorean'}              = pronunciation(s) of this char
#                  ->{'kSimplifiedVariant'}   = simplified Chinese variant(s) of this char if any
#                  ->{'kTraditionalVariant'}  = traditional Chinese variant(s) of this char if any
# search Variants
foreach my $hex (keys %{$char})
{
  if    (defined $char->{$hex}->{'kSimplifiedVariant'})
  {
    foreach (@{$char->{$hex}->{'kSimplifiedVariant'}})
    {
      push @{$char->{$_}->{'isSimplifiedVariantOf'}},  $hex if defined $char->{$_} && $char->{$_}->{'kKorean'}; # XXX beware
    }
  }
  elsif (defined $char->{$hex}->{'kTraditionalVariant'})
  {
    foreach (@{$char->{$hex}->{'kTraditionalVariant'}})
    {
      push @{$char->{$_}->{'isTraditionalVariantOf'}}, $hex if defined $char->{$_} && $char->{$_}->{'kKorean'};
    }
  }
}

# $char has additionally :
# $char->{'U+XXXX'}->{'isSimplifiedVariantOf'}   = traditional Chinese variant(s) of this char if any
#                  ->{'isTraditionalVariantOf'}  = simplified Chinese variant(s) of this char if any
# show results
foreach my $hex (sort keys %{$char})
{
  my @tags = grep ! /^kKorean$/, keys %{$char->{$hex}};
  # skip if not variant
  next unless $char->{$hex}->{'isSimplifiedVariantOf'} ||
              $char->{$hex}->{'isTraditionalVariantOf'} ||
              $char->{$hex}->{'kSimplifiedVariant'} ||
              $char->{$hex}->{'kTraditionalVariant'};
  my $has_error = 0;
  # if a char is a simplified variant of others
  $has_error ++ if $char->{$hex}->{'isSimplifiedVariantOf'} &&
                   ! &_is_identical(\$char->{$hex}->{'isSimplifiedVariantOf'},\$char->{$hex}->{'kTraditionalVariant'});
  # if a char is a traditional variant of others
  $has_error ++ if $char->{$hex}->{'isTraditionalVariantOf'} &&
                   ! &_is_identical(\$char->{$hex}->{'isTraditionalVariantOf'},\$char->{$hex}->{'kSimplifiedVariant'});
  next unless $has_error || $opt_v;
  print &_uchar($hex).($has_error?" might have some errors on its variant information..":"")."\n";
  printf "%22s : %s\n", "kKorean", join " ", map{&_phone($_)}sort@{$char->{$hex}->{'kKorean'}};
  printf "%22s : %s\n",        $_, join ", ",map{&_uchar($_)}sort@{$char->{$hex}->{$_}} foreach sort @tags;
  print +("-"x76)."\n";
}

# return true if two arrayref has same elements
sub _is_identical
{
  my ($a1,$a2) = @_;
  my %union;

  map { $union{$_} ++ } @{$$a1};
  map { $union{$_} ++ } @{$$a2};

  return (keys %union == @{$$a1}) ?
         (keys %union == @{$$a2}) ? 1 : 0 : 0;
}

# return utf8 char
sub _uchar
{
  my $u = shift;
  return +(pack"U",hex($1))." $u" if $u =~ m/^U\+(.*)/;
}

# return pronunciation
sub _phone
{
  # stolen from hanjatable.py
  my $jamo = {
    'base' => { 'syllable'  => 0xac00,
                'cho'       => 0x1100,
                'jung'      => 0x1161,
                'jong'      => 0x11a7, },
    'num'  => { 'jung'      => 21,
                'jong'      => 28, },
    'cho'  => { 'k'         => 0x1100,#{{{
                'kk'        => 0x1101,
                'n'         => 0x1102,
                't'         => 0x1103,
                'tt'        => 0x1104,
                'l'         => 0x1105,
                'm'         => 0x1106,
                'p'         => 0x1107,
                'b'         => 0x1107,
                'pp'        => 0x1108,
                's'         => 0x1109,
                'ss'        => 0x110a,
                ''          => 0x110b,  # IEUNG without phonetic value
                'c'         => 0x110c,
                'cc'        => 0x110d,
                'ch'        => 0x110e,
                'kh'        => 0x110f,
                'th'        => 0x1110,
                'ph'        => 0x1111,
                'h'         => 0x1112, },#}}}
    'jung' => { 'a'         => 0x1161,#{{{
                'ay'        => 0x1162,
                'ya'        => 0x1163,
                'yay'       => 0x1164,
                'e'         => 0x1165,
                'ey'        => 0x1166,
                'ye'        => 0x1167,
                'yey'       => 0x1168,
                'o'         => 0x1169,
                'wa'        => 0x116a,
                'way'       => 0x116b,
                'oy'        => 0x116c,
                'woy'       => 0x116c,
                'yo'        => 0x116d,
                'wu'        => 0x116e,
                'we'        => 0x116f,
                'wey'       => 0x1170,
                'wi'        => 0x1171,
                'yu'        => 0x1172,
                'u'         => 0x1173,
                'uy'        => 0x1174,
                'i'         => 0x1175, },#}}}
    'jong' => { ''          => 0x11a7,#{{{
                'k'         => 0x11a8,
                'kk'        => 0x11a9,
                'ks'        => 0x11aa,
                'n'         => 0x11ab,
                'nc'        => 0x11ac,
                'nh'        => 0x11ad,
                't'         => 0x11ae,
                'l'         => 0x11af,
                'lk'        => 0x11b0,
                'lm'        => 0x11b1,
                'lp'        => 0x11b2,
                'ls'        => 0x11b3,
                'lth'       => 0x11b4,
                'lph'       => 0x11b5,
                'lh'        => 0x11b6,
                'm'         => 0x11b7,
                'p'         => 0x11b8,
                'ps'        => 0x11b9,
                's'         => 0x11ba,
                'ss'        => 0x11bb,
                'ng'        => 0x11bc,
                'c'         => 0x11bd,
                'ch'        => 0x11be,
                'kh'        => 0x11bf,
                'th'        => 0x11c0,
                'ph'        => 0x11c1,
                'h'         => 0x11c2, },#}}}
  };
  #my ($cho,$jung,$jong) = ($1,$2,$3) if +shift =~ m/^(.*?)?([aeiouwy]+)(.*)$/i;
  my $e = shift;
  my ($cho,$jung,$jong) = map { lc } ($1,$2,$3) if $e =~ m/^(.*?)?([aeiouwy]+)(.*)$/i;
  unless (defined $jamo->{'cho'}->{$cho})   { carp "no such CHOSEONG $cho ($e)";   return $e; }
  unless (defined $jamo->{'jung'}->{$jung}) { carp "no such JUNGSEONG $jung ($e)"; return $e; }
  unless (defined $jamo->{'jong'}->{$jong}) { carp "no such JONGSEONG $jong ($e)"; return $e; }
  $cho  = $jamo->{'cho'}->{$cho}   - $jamo->{'base'}->{'cho'};
  $jung = $jamo->{'jung'}->{$jung} - $jamo->{'base'}->{'jung'};
  $jong = $jamo->{'jong'}->{$jong} - $jamo->{'base'}->{'jong'};
  return pack "U",($cho*$jamo->{'num'}->{'jung'}+$jung)*$jamo->{'num'}->{'jong'}+$jong+$jamo->{'base'}->{'syllable'};
}
