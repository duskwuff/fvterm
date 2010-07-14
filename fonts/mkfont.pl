#!/usr/bin/perl
use strict;
use GD::Simple;
use Data::Dumper;

my $srcfile = shift @ARGV || die "usage: bdf2font.pl font.bdf dstname";
my $dstname = shift @ARGV || die "usage: bdf2font.pl font.bdf dstname";

my ($dirname) = $dstname =~ m{^(.*)/};
$dirname ||= $dstname;
unless(-d $dirname) {
    mkdir $dirname or die "$!";
}

if(-d $dstname) {
    $dstname .= "/page";
}

sub ReadBDF {
    my ($srcfile) = @_;
    my %chars;
    my $bbox;
    my ($curpage, $curidx);

    open BDF, "<", $srcfile or die "$!";
    while(my $line = <BDF>) {
        chomp $line;
        $line =~ s/\r//;
        my ($tag, $value) = split / /, $line, 2;

        if($tag eq 'FONTBOUNDINGBOX') {
            $bbox = $value;
        } elsif($tag eq 'ENCODING') {
            my $curchar = $value;
            $curpage = ($curchar >> 8);
            $curidx = $curchar & 0xff;
        } elsif($tag eq 'BBX') {
            die "char bbox doesn't match font" unless $value eq $bbox;
        } elsif($tag eq 'BITMAP') {
            while(my $line = <BDF>) {
                chomp $line;
                $line =~ s/\r//;
                last if $line eq 'ENDCHAR';
                $line =~ tr{0123456789ABCDEF}
                           {084C2A6E195D3B7F}; # bit reversal
                $line = hex(reverse($line));
                push @{$chars{$curpage}->[$curidx]}, $line;
            }
        }
    }
    close BDF;

    return (\%chars, $bbox);
}

sub UPR {
    my ($fh, $format, $n, $offset) = @_;
    my $data;
    sysread($fh, $data, $n, $offset) or die "$! (EOF?)";
    my @p = unpack $format, $data;
    return wantarray ? @p : $p[0];
}

sub ReadPSF {
    my ($srcfile) = @_;
    my ($fh, %chars, $width, $height);

    open $fh, "<", $srcfile or die "$!";
    my $head = UPR($fh, "S>", 2);
    if($head == 0x3604) {
        print "Version 0 PSF\n";
        my ($flags, $height) = UPR($fh, "CC", 2);
        my $ccount = ($flags & 1) ? 512 : 256;
        my $unicode = ($flags & 2) ? 1 : 0;
        printf "%sunicode, %d chars\n", ($unicode ? "" : "not"), $ccount;

        for(my $i = 0; $i < $ccount; $i++) {
            my $row = pack("B8" x $height, UPR($fh, "b8" x $height, $height));
            $chars{$i} = [ unpack("C" x $height, $row) ];
        }
        close PSF;
        return \%chars, 8, $height;
    } else {
        die "Unhandled PSF file";
    }
}

sub ReadFontFile {
    my ($path) = @_;
    return ReadBDF($path) if $path =~ /\.bdf$/;
    return ReadPSF($path) if $path =~ /\.psf$/;
    die "Unknown extension on $path";
}

sub Paginate {
    my ($h1) = @_;
    my $h2 = {};

    while(my ($key, $val) = each %$h1) {
        my $page = $key >> 8;
        my $idx  = $key & 0xff;
        $h2->{$page}->[$idx] = $val;
    }

    %$h1 = %$h2;
}

my ($chars, $cwidth, $cheight) = ReadFontFile($srcfile);
Paginate($chars);

print "Defined pages:\n";
for my $page (sort { $a <=> $b } keys %$chars) {
    my $count = scalar grep { defined $_ } @{$chars->{$page}};
    printf " - %02x (%d/256 defined)\n", $page, $count;
    my $img = GD::Simple->new($cwidth * 32, $cheight * 8);
    my $black = $img->colorAllocate(0, 0, 0);
    my $white = $img->colorAllocate(255, 255, 255);
    my $replChar = $chars->{0}->[0];
    for(my $i = 0; $i < 256; $i++) {
        my $cdata = $chars->{$page}->[$i] || $replChar;
        my $xoff = ($i & 31) * $cwidth;
        my $yoff = int($i / 32) * $cheight;
        for(my $y = 0; $y < $cheight; $y++) {
            my $row = $cdata->[$y];
            for(my $x = 0; $x < $cwidth; $x++) {
                if($row & 1) {
                    $img->setPixel($x + $xoff, $y + $yoff, $black);
                }
                $row >>= 1;
            }
        }
    }
    open PNG, ">", sprintf("%s%02x.png", $dstname, $page) or die "$!";
    binmode PNG;
    print PNG $img->png;
    close PNG;
}

