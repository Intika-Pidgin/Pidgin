#!/usr/bin/env perl

# Copyright 2003-2005 Nathan Walp <faceprint@faceprint.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 50 Temple Place, Suite 330, Boston, MA 02111-1301  USA
#

use POSIX qw(strftime);


my $PACKAGE="pidgin";


use Locale::Language;

$lang{'ca@valencia'} = "Catalan (Valencian)";
$lang{'be@latin'} = "Belarusian (Latin)";
$lang{bn_IN} = "Bengali-India";
$lang{en_AU} = "English (Australian)";
$lang{en_CA} = "English (Canadian)";
$lang{en_GB} = "English (British)";
$lang{mai} = "Maithili";
$lang{mhr} = "Meadow Mari";
$lang{ms_MY} = "Malay";
$lang{my_MM} = "Burmese (Myanmar)";
$lang{pt_BR} = "Portuguese (Brazilian)";
$lang{'sr@latin'} = "Serbian (Latin)";
$lang{zh_CN} = "Chinese (Simplified)";
$lang{zh_HK} = "Chinese (Hong Kong)";
$lang{zh_TW} = "Chinese (Traditional)";

opendir(DIR, ".") || die "can't open directory: $!";
@pos = grep { /\.po$/ && -f } readdir(DIR);
foreach (@pos) { s/\.po$//; };
closedir DIR;

@pos = sort @pos;

$now = `date`;

system("intltool-update --pot > /dev/null");

$_ = `msgfmt --statistics $PACKAGE.pot -o /dev/null 2>&1`;

die "unable to get total: $!" unless (/(\d+) untranslated messages/);

$total = $1;
$generated = strftime "%Y-%m-%d %H:%M:%S", gmtime;

print "<?xml version='1.0'?>\n";
print "<?xml-stylesheet type='text/xsl' href='l10n.xsl'?>\n";
print "<project version='1.0' xmlns:l10n='http://faceprint.com/code/l10n' name='$PACKAGE' pofile='$PACKAGE.pot' strings='$total' generated='$generated'>\n";

foreach $index (0 .. $#pos) {
	$trans = $fuzz = $untrans = 0;
	$po = $pos[$index];
	print STDERR "$po..." if($ARGV[0] eq '-v');
	system("msgmerge --no-location -U $po.po $PACKAGE.pot 2>/dev/null");
	if (($? & 127) == 2) {
		printf STDERR "Caught keyboard interrupt--exiting\n";
		exit
	}
	$_ = `msgfmt --statistics $po -o /dev/null 2>&1`;
	chomp;
	if(/(\d+) translated message/) { $trans = $1; }
	if(/(\d+) fuzzy translation/) { $fuzz = $1; }
	if(/(\d+) untranslated message/) { $untrans = $1; }

	$name = "";
	$name = $lang{$po};
	$name = code2language($po) unless $name ne "";
	$name = "???" unless $name ne "";

	print "<lang code='$po' name='$name' translated='$trans' fuzzy='$fuzz' />\n";
	print STDERR "done ($untrans untranslated strings).\n" if($ARGV[0] eq '-v');
}

print "</project>\n";

