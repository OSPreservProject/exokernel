
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#include <sys/types.h>
struct oscalltableentry {
  int num;
  char *name;
};

static struct oscalltableentry oscalltable[] = {
  { 0, "syscall" },
  { 1, "exit" },
  { 2, "fork" },
  { 3, "read" },
  { 4, "write" },
  { 5, "open" },
  { 6, "close" },
  { 7, "wait4" },
  { 9, "link" },
  { 10, "unlink" },
  { 12, "chdir" },
  { 13, "fchdir" },
  { 14, "mknod" },
  { 15, "chmod" },
  { 16, "chown" },
  { 17, "break" },
  { 18, "getfsstat" },
  { 20, "getpid" },
  { 21, "mount" },
  { 22, "unmount" },
  { 23, "setuid" },
  { 24, "getuid" },
  { 25, "geteuid" },
  { 26, "ptrace" },
  { 27, "recvmsg" },
  { 28, "sendmsg" },
  { 29, "recvfrom" },
  { 30, "accept" },
  { 31, "getpeername" },
  { 32, "getsockname" },
  { 33, "access" },
  { 34, "chflags" },
  { 35, "fchflags" },
  { 36, "sync" },
  { 37, "kill" },
  { 39, "getppid" },
  { 41, "dup" },
  { 42, "pipe" },
  { 43, "getegid" },
  { 44, "profil" },
  { 45, "ktrace" },
  { 46, "sigaction" },
  { 47, "getgid" },
  { 48, "sigprocmask" },
  { 49, "getlogin" },
  { 50, "setlogin" },
  { 51, "acct" },
  { 52, "sigpending" },
  { 53, "sigaltstack" },
  { 54, "ioctl" },
  { 55, "YS_reboot" },
  { 56, "revoke" },
  { 57, "symlink" },
  { 58, "readlink" },
  { 59, "execve" },
  { 60, "umask" },
  { 61, "chroot" },
  { 65, "msync" },
  { 66, "vfork" },
  { 69, "sbrk" },
  { 70, "sstk" },
  { 72, "vadvise" },
  { 73, "munmap" },
  { 74, "mprotect" },
  { 75, "madvise" },
  { 78, "mincore" },
  { 79, "getgroups" },
  { 80, "setgroups" },
  { 81, "getpgrp" },
  { 82, "setpgid" },
  { 83, "setitimer" },
  { 85, "swapon" },
  { 86, "getitimer" },
  { 90, "dup2" },
  { 92, "fcntl" },
  { 93, "select" },
  { 95, "fsync" },
  { 96, "setpriority" },
  { 97, "socket" },
  { 98, "connect" },
  { 100, "getpriority" },
  { 103, "sigreturn" },
  { 104, "bind" },
  { 105, "setsockopt" },
  { 106, "listen" },
  { 111, "sigsuspend" },
  { 115, "vtrace" },
  { 116, "gettimeofday" },
  { 117, "getrusage" },
  { 118, "getsockopt" },
  { 120, "readv" },
  { 121, "writev" },
  { 122, "settimeofday" },
  { 123, "fchown" },
  { 124, "fchmod" },
  { 128, "rename" },
  { 131, "flock" },
  { 132, "mkfifo" },
  { 133, "sendto" },
  { 134, "shutdown" },
  { 135, "socketpair" },
  { 136, "mkdir" },
  { 137, "rmdir" },
  { 138, "utimes" },
  { 140, "adjtime" },
  { 147, "setsid" },
  { 148, "quotactl" },
  { 155, "nfssvc" },
  { 157, "statfs" },
  { 158, "fstatfs" },
  { 161, "getfh" },
  { 165, "sysarch" },
  { 175, "ntp_gettime" },
  { 176, "ntp_adjtime" },
  { 181, "setgid" },
  { 182, "setegid" },
  { 183, "seteuid" },
  { 184, "lfs_bmapv" },
  { 185, "lfs_markv" },
  { 186, "lfs_segclean" },
  { 187, "lfs_segwait" },
  { 188, "stat" },
  { 189, "fstat" },
  { 190, "lstat" },
  { 191, "pathconf" },
  { 192, "fpathconf" },
  { 194, "getrlimit" },
  { 195, "setrlimit" },
  { 196, "getdirentries" },
  { 197, "mmap" },
  { 198, "__syscall" },
  { 199, "lseek" },
  { 200, "truncate" },
  { 201, "ftruncate" },
  { 202, "__sysctl" },
  { 203, "mlock" },
  { 204, "munlock" },
  { 205, "undelete" },
  { 206, "futimes" },
  { 207, "getpgid" },
  { 220, "__semctl" },
  { 221, "semget" },
  { 222, "semop" },
  { 223, "semconfig" },
  { 224, "msgctl" },
  { 225, "msgget" },
  { 226, "msgsnd" },
  { 227, "msgrcv" },
  { 228, "shmat" },
  { 229, "shmctl" },
  { 230, "shmdt" },
  { 231, "shmget" },
  { 232, "clock_gettime" },
  { 233, "clock_settime" },
  { 234, "clock_getres" },
  { 240, "nanosleep" },
  { 250, "minherit" },
  { 251, "rfork" },
  { 252, "poll" },
  { 253, "issetugid" },
  { 254, "lchown" },
  { 0, 0 }
};

char *oscallstr(u_int oscall) {
  int i;

  for (i=0; oscalltable[i].name != 0; i++)
    if (oscalltable[i].num == oscall)
      return oscalltable[i].name;
  return 0;
}
