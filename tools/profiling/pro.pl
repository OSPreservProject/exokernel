#!/usr/local/bin/perl

@files = ();
while(<>) {
    if (/^File (\S+),/) {
	$file = $1;
	$file =~ s/\.d$/\.c/;
	push(@files,$file);
	print "FILE: $file\n";
    } elsif (($n,$line) = /^.*executed\s+(\d+) .* line=\s+(\d+)/) {
	print "LINE: $n,$line\n";
	$db{$file,$line} += $n;
    }				
}
print "FILES: @files\n";
foreach $file (@files) {
    print "FILE: $file\n";
    open(FILE,$file);
    $line = 0;
    while(<FILE>) {
	$line++;
	write;
    }
}

format STDOUT =
@>>>>>:    @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    $db{$file,$line},$_
.
