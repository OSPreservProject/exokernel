#! /usr/bin/perl

# A simple parser to sort a bb.out file by frequency
#
# recommended usage: 
#        cat <infile> | bboutparse.pl | less
#
# simply takes standard input and writes to standard output

while (<STDIN>) {
   unless (/.*executed(\s+)0.*/) {
        unless (/~\s+$/) {
             push @lines, $_;
	 }
   }
}
sub order_lines{
    $_ = $a;
    /.*executed\s+(\d+).*/;
    $a_key = $1;
    $_ = $b;
    /.*executed\s+(\d+).*/;
    $b_key = $1;
    return $b_key <=> $a_key;
}
@sorted = sort order_lines @lines;

printf "Basic Block counts, in decreasing order:\n\n";
printf"File:            Function:                  Address:  Line:     Executed:\n\n";                 
format STDOUT = 
@<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<< @<<<<<<<< @<<<<<<<<
$file,           $function,                 $address, $line,    $executed
.

foreach $line (@sorted) {
    $fnd = 0;
    $fnd = ($line =~ /.*executed\s+(\d+)\s+time\(s\)(.*)/);
    $executed = $1; 
    $2 =~ /\s+address=\s+0x([\d|a-f]+)\s+(.*)/;
    $address = $1;
    $2 =~ /function=\s+(\S+)\s+(.*)/;
    $function = $1;
    $2 =~ /line=\s+(\d+)\s+(.*)/;
    $line = $1;
    $2 =~ /file=\s+.*\/([\d|\w|\_]+\.[c|s|S]).*/;
    $file = $1;
    #    print $executed, "\t0x", $address, "\t", $file, "\t", $function, "\t", $line, "\n"; 
    if ($fnd) {write};
}

