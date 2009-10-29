#!/usr/local/bin/perl

foreach (@ARGV) {
    @sbuf = stat $_;
    next unless @sbuf;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
     $atime,$mtime,$ctime,$blksize,$blocks) = @sbuf;
    print "FILE: $_\n";
    printf "dev = 0x%-8x   ino = %-6d       mode = 0x%-4x\n",$dev,$ino,$mode;
    printf "nlink = %-4d       uid = %-6d       gid = %-6d\n",$nlink,$uid,$gid;
    printf "rdev = 0x%-8x  size = %-d \n",$rdev,$size;
    printf "atime = 0x%-8x mtime = 0x%-8x ctime = 0x%-8x\n",$atime,$mtime,$ctime;
    printf "blksize = %-8d blocks = %d\n",$blksize,$blocks;
    print "\n";
}
