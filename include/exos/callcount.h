
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

#ifndef _CALLCOUNT_H_
#define _CALLCOUNT_H_

#include <exos/callcountdefs.h>

void __call_count_init();
void __call_count_results();

#ifdef KERN_CALL_COUNT
extern unsigned int *__kern_call_counts;
#endif

#ifdef SYS_CALL_COUNT
extern void __oscallenter(int c);
extern void __oscallexit(int c);
#define OSCALLENTER(a) __oscallenter(a);
#define OSCALLEXIT(a) __oscallexit(a);

#else

#define OSCALLENTER(a)
#define OSCALLEXIT(a)

#endif

#define OSCALL_syscall	0
#define OSCALL_exit	1
#define OSCALL_fork	2
#define OSCALL_read	3
#define OSCALL_write	4
#define OSCALL_open	5
#define OSCALL_close	6
#define OSCALL_wait4	7
				/* 8 is compat_43 ocreat */
#define OSCALL_link	9
#define OSCALL_unlink	10
				/* 11 is obsolete execv */
#define OSCALL_chdir	12
#define OSCALL_fchdir	13
#define OSCALL_mknod	14
#define OSCALL_chmod	15
#define OSCALL_chown	16
#define OSCALL_break	17
#define OSCALL_getfsstat	18
				/* 19 is compat_43 olseek */
#define OSCALL_getpid	20
#define OSCALL_mount	21
#define OSCALL_unmount	22
#define OSCALL_setuid	23
#define OSCALL_getuid	24
#define OSCALL_geteuid	25
#define OSCALL_ptrace	26
#define OSCALL_recvmsg	27
#define OSCALL_sendmsg	28
#define OSCALL_recvfrom	29
#define OSCALL_accept	30
#define OSCALL_getpeername	31
#define OSCALL_getsockname	32
#define OSCALL_access	33
#define OSCALL_chflags	34
#define OSCALL_fchflags	35
#define OSCALL_sync	36
#define OSCALL_kill	37
				/* 38 is compat_43 ostat */
#define OSCALL_getppid	39
				/* 40 is compat_43 olstat */
#define OSCALL_dup	41
#define OSCALL_pipe	42
#define OSCALL_getegid	43
#define	OSCALL_profil	44
#define OSCALL_ktrace	45
#define OSCALL_sigaction	46
#define OSCALL_getgid	47
#define OSCALL_sigprocmask	48
#define OSCALL_getlogin	49
#define OSCALL_setlogin	50
#define OSCALL_acct	51
#define OSCALL_sigpending	52
#define OSCALL_sigaltstack	53
#define OSCALL_ioctl	54
#define	OSCALL_reboot	55
#define OSCALL_revoke	56
#define OSCALL_symlink	57
#define OSCALL_readlink	58
#define OSCALL_execve	59
#define OSCALL_umask	60
#define OSCALL_chroot	61
				/* 62 is compat_43 ofstat */
				/* 63 is compat_43 ogetkerninfo */
				/* 64 is compat_43 ogetpagesize */
#define OSCALL_msync	65
#define OSCALL_vfork	66
				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
#define OSCALL_sbrk	69
#define OSCALL_sstk	70
				/* 71 is compat_43 ommap */
#define OSCALL_vadvise	72
#define OSCALL_munmap	73
#define OSCALL_mprotect	74
#define OSCALL_madvise	75
				/* 76 is obsolete vhangup */
				/* 77 is obsolete vlimit */
#define OSCALL_mincore	78
#define OSCALL_getgroups	79
#define OSCALL_setgroups	80
#define OSCALL_getpgrp	81
#define OSCALL_setpgid	82
#define OSCALL_setitimer	83
				/* 84 is compat_43 owait */
#define OSCALL_swapon	85
#define OSCALL_getitimer	86
				/* 87 is compat_43 ogethostname */
				/* 88 is compat_43 osethostname */
				/* 89 is compat_43 ogetdtablesize */
#define OSCALL_dup2	90
#define OSCALL_fcntl	92
#define OSCALL_select	93
#define OSCALL_fsync	95
#define OSCALL_setpriority	96
#define OSCALL_socket	97
#define OSCALL_connect	98
				/* 99 is compat_43 oaccept */
#define OSCALL_getpriority	100
				/* 101 is compat_43 osend */
				/* 102 is compat_43 orecv */
#define OSCALL_sigreturn	103
#define OSCALL_bind	104
#define OSCALL_setsockopt	105
#define OSCALL_listen	106
				/* 107 is obsolete vtimes */
				/* 108 is compat_43 osigvec */
				/* 109 is compat_43 osigblock */
				/* 110 is compat_43 osigsetmask */
#define OSCALL_sigsuspend	111
				/* 112 is compat_43 osigstack */
				/* 113 is compat_43 orecvmsg */
				/* 114 is compat_43 osendmsg */
#define OSCALL_vtrace	115
				/* 115 is obsolete vtrace */
#define OSCALL_gettimeofday	116
#define OSCALL_getrusage	117
#define OSCALL_getsockopt	118
				/* 119 is obsolete resuba */
#define OSCALL_readv	120
#define OSCALL_writev	121
#define OSCALL_settimeofday	122
#define OSCALL_fchown	123
#define OSCALL_fchmod	124
				/* 125 is compat_43 orecvfrom */
				/* 126 is compat_43 osetreuid */
				/* 127 is compat_43 osetregid */
#define OSCALL_rename	128
				/* 129 is compat_43 otruncate */
				/* 130 is compat_43 oftruncate */
#define OSCALL_flock	131
#define OSCALL_mkfifo	132
#define OSCALL_sendto	133
#define OSCALL_shutdown	134
#define OSCALL_socketpair	135
#define OSCALL_mkdir	136
#define OSCALL_rmdir	137
#define OSCALL_utimes	138
				/* 139 is obsolete 4.2 sigreturn */
#define OSCALL_adjtime	140
				/* 141 is compat_43 ogetpeername */
				/* 142 is compat_43 ogethostid */
				/* 143 is compat_43 osethostid */
				/* 144 is compat_43 ogetrlimit */
				/* 145 is compat_43 osetrlimit */
				/* 146 is compat_43 okillpg */
#define OSCALL_setsid	147
#define OSCALL_quotactl	148
				/* 149 is compat_43 oquota */
				/* 150 is compat_43 ogetsockname */
#define OSCALL_nfssvc	155
				/* 156 is compat_43 ogetdirentries */
#define OSCALL_statfs	157
#define OSCALL_fstatfs	158
#define OSCALL_getfh	161
				/* 162 is compat_09 ogetdomainname */
				/* 163 is compat_09 osetdomainname */
				/* 164 is compat_09 ouname */
#define OSCALL_sysarch	165
				/* 169 is compat_10 osemsys */
				/* 170 is compat_10 omsgsys */
				/* 171 is compat_10 oshmsys */
#define OSCALL_ntp_gettime	175
#define OSCALL_ntp_adjtime	176
#define OSCALL_setgid	181
#define OSCALL_setegid	182
#define OSCALL_seteuid	183
#define OSCALL_lfs_bmapv	184
#define OSCALL_lfs_markv	185
#define OSCALL_lfs_segclean	186
#define OSCALL_lfs_segwait	187
#define OSCALL_stat	188
#define OSCALL_fstat	189
#define OSCALL_lstat	190
#define OSCALL_pathconf	191
#define OSCALL_fpathconf	192
#define OSCALL_getrlimit	194
#define OSCALL_setrlimit	195
#define OSCALL_getdirentries	196
#define OSCALL_mmap	197
#define OSCALL___syscall	198
#define OSCALL_lseek	199
#define OSCALL_truncate	200
#define OSCALL_ftruncate	201
#define OSCALL___sysctl	202
#define OSCALL_mlock	203
#define OSCALL_munlock	204
#define OSCALL_undelete	205
#define OSCALL_futimes	206
#define OSCALL___semctl	220
#define OSCALL_semget	221
#define OSCALL_semop	222
#define OSCALL_semconfig	223
#define OSCALL_msgctl	224
#define OSCALL_msgget	225
#define OSCALL_msgsnd	226
#define OSCALL_msgrcv	227
#define OSCALL_shmat	228
#define OSCALL_shmctl	229
#define OSCALL_shmdt	230
#define OSCALL_shmget	231
#define OSCALL_nanosleep	240
#define OSCALL_minherit	250
#define OSCALL_rfork	251
#define OSCALL_poll	252
#define OSCALL_issetugid	253
#define OSCALL_MAXSYSCALL	254

#endif
