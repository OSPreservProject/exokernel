#!/usr/local/bin/perl.pl
#debug = 1;
$; = ' ';

$nrphases = 5;

%results = ();
@files = ();
foreach $file (@ARGV) {
    open(FILE, $file);
    push(@files, $file);
    $index = 0;
    while(<FILE>) {
	if (/^date/) {
	    $_ = <FILE>;
#	    print "DATE: $_\n";
	    ($hour,$min,$sec) = /^... ... .. (\d+):(\d+):(\d+)/;
#	    printf "TIME $file,$index: %d\n", ($hour * 3600 + $min * 60 + $sec);
	    $results{$file,$index++ } = $hour * 3600 + $min * 60 + $sec;
	}
    }
    close(FILE);
}

%number = ();
%sum = ();
foreach $file (@files) {
#    print "FILE $file\n";
    ($os, $type,$sample) = $file =~ /^(\w+)\.(\w+)\/[\w\.]*(\d+)/;

    @phase = ();
    printf "%20s:  ","$os,$type,$sample" if ($debug);
    foreach $i (1..$nrphases) {
	$phase[$i] = $results{$file,$i} -  $results{$file,$i - 1};
	printf "%2d ",$phase[$i] if ($debug);
	$sum{$os,$type,$i} += $results{$file,$i} -  $results{$file,$i - 1};
    }
    printf "  TOTAL %d\n",($results{$file,$nrphases} -  $results{$file,0}) if ($debug);

    $sum{$os,$type,'all'} += $results{$file,$nrphases} -  $results{$file,0};
    $number{$os,$type}++;
}


print <<'EOF';

MODIFIED ANDREW BENCHMARK

There are number of phases to the benchmark, each exercising a different
aspect of the file system:
        1. Phase I:   many subdirectories are recursively created.
        2. Phase II:  stresses the file system's ability to transfer
                      large amounts of data by copying files.
        3. Phase III: recursively examines the status of every file in
                      fscript, without actually examining the data
                      in the files.
        4. Phase IV:  examines every byte of every file in fscript.
        5. Phase V:   is computationally intensive.

EOF

printf "%6s %6s: Avg Total  \#samp. ","OS","FStype";
foreach $i (1..$nrphases) {
    printf " %5s  ",$i;
}
print "\n";

foreach $key (keys(%number)) {
($os,$fs) = split(/ /,$key);
    printf "%6s %6s:  %03.3f    %3d     ",$os,$fs,
       $sum{$key,'all'} / $number{$key},$number{$key};

    foreach $i (1..$nrphases) {
	printf " %03.3f  ",$sum{$key,$i} / $number{$key};
    }
    print "\n";
}


print "\n\n";
