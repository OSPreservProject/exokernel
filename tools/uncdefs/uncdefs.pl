#!/usr/local/bin/perl -i.ori 


while(<>) {
    s/(\#\s*include\s*\<\s*sys\/cdefs.h\s*\>)/\/\* \1  commented out by uncdefs.pl \*\//;
    s/__P\s*\((.*)\)/\1/;
    s/__BEGIN_DECLS//;
    s/__END_DECLS//;
    s/__CONCAT\s*\((.*)\,(.*)\)/\1 \#\# \2/;
    s/__STRING\s*\((.*)\)/\"\1\"/;
    s/__pure//;
    s/__dead//;
    s/__volatile/volatile/;
    s/__signed/signed/;
    s/__const/const/;
    print;
}

