
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

#ifndef _FD_STAT_H_
#define _FD_STAT_H_
#include <stdio.h>

extern void fd_stat_init(void);
extern void (*pr_fd_statp)(void);

//#define FDSTAT

#ifdef FDSTAT
#include <sys/pctr.h>
#include <sys/sysinfo.h>	/* for sysinfo.si_mhz */

#define ST(module,name) int name ; pctrval name ## _v, name ## _s;

#define APPLYM(module,macro)			\
macro(module,open);				\
macro(module,namei);				\
macro(module,namei_rel);			\
macro(module,read);				\
macro(module,read0);				\
macro(module,read1);				\
macro(module,read2);				\
macro(module,write);				\
macro(module,lseek);				\
macro(module,select);				\
macro(module,select_pred);			\
macro(module,ioctl);				\
macro(module,close0);				\
macro(module,close);				\
macro(module,lookup);				\
macro(module,remove);				\
macro(module,add);				\
macro(module,link);				\
macro(module,symlink);				\
macro(module,unlink);				\
macro(module,mkdir);				\
macro(module,rmdir);				\
macro(module,mknod);				\
macro(module,rename);				\
macro(module,readlink);				\
macro(module,follow_link);			\
macro(module,truncate);				\
macro(module,ftruncate);			\
macro(module,dup);				\
macro(module,release);				\
macro(module,acquire);				\
macro(module,bind);				\
macro(module,connect);				\
macro(module,filepair);				\
macro(module,accept);				\
macro(module,getname);				\
macro(module,listen);				\
macro(module,sendto);				\
macro(module,recvfrom);				\
macro(module,shutdown);				\
macro(module,setsockopt);			\
macro(module,getsockopt);			\
macro(module,fcntl);				\
macro(module,mount);				\
macro(module,unmount);				\
macro(module,chmod);				\
macro(module,chown);				\
macro(module,stat);				\
macro(module,fstat);				\
macro(module,lstat);				\
macro(module,getdirentries);			\
macro(module,chdir);				\
macro(module,fchdir);				\
macro(module,access);				\
macro(module,permission);			\
macro(module,utimes);				\
macro(module,bmap);				\
macro(module,fsync);				\
macro(module,exithandler);			\
macro(module,getfd);				\
macro(module,putfd);				\
macro(module,getfilp);				\
macro(module,putfilp);				\
macro(module,proctime);				\
macro(module,init);				\
macro(module,init2);				\
macro(module,cache_remove);			\
macro(module,cache_lookup);			\
macro(module,cache_add);			\
macro(module,execve);				\
macro(module,fork);				\
macro(module,shm_alloc);			\
macro(module,attach);				\
macro(module,detach);				\
macro(module,get);				\
macro(module,put);				\
macro(module,alloc);				\
macro(module,dealloc);				\
macro(module,insert);				\
macro(module,flush);				\
macro(module,step0);				\
macro(module,step1);				\
macro(module,step2);				\
macro(module,step3);				\
macro(module,step4);				\
macro(module,step5);				\
macro(module,step6);				\
macro(module,step7);				\
macro(module,step8);				\
macro(module,step9);				\
macro(module,step10);				\


struct fdstatistics {
  APPLYM(0,ST);
};

/* if you add something here add it to fdstat.c */
extern struct fdstatistics fd_fd,
  fd_fd_op[],
  fd_ftable,
  fd_namei,
  fd_nfsc,
  fd_nfs_lookup,
  fd_misc;


#define START(module,name)  ({					\
    fd_ ## module.name++ ;					\
    fd_ ## module.name ## _s = rdtsc();})				


#define STOP(module,name) ({					\
  fd_ ## module.name ## _v += rdtsc() - fd_ ## module.name ## _s;})

/* prints the times to stdout */
#define PRNAME(module,name)							\
     if (pr_fd_statp) if ( (fd_ ## module.name) != 0) {						\
       printf("%13s: %-6d ",#name,fd_ ## module.name);				\
  printf(" cycles %12qd ",fd_ ## module.name ## _v);				\
  printf(" time (usecs): %-9qd\n",fd_ ## module.name ## _v / ((quad_t) __sysinfo.si_mhz)); }

/* "kprintfs" the times */
#define KPRNAME(module,name) if (pr_fd_statp) {									\
    char buf[256];				      						\
    if ( (fd_ ## module.name) != 0) {								\
       sprintf(buf,"%13s: %-6d ",#name,fd_ ## module.name);					\
       sys_cputs((char *)&buf[0]);								\
       sprintf(buf," cycles %12qd ",fd_ ## module.name ## _v);					\
       sys_cputs((char *)&buf[0]);								\
       sprintf(buf," time (usecs): %-9qd\n",fd_ ## module.name ## _v / ((quad_t)__sysinfo.si_mhz)); }	\
       sys_cputs((char *)&buf[0]);								\
     }						       

/* initializes */
#define FDSTATINIT(module,name) fd_ ## module.name = fd_ ## module.name ## _v = 0;

     /* initilizes and starts the time */
#define ISTART(module,name) FDSTATINIT(module,name);START(module,name)
     /* stops the time and prints it */
#define STOPP(module,name) STOP(module,name); PRNAME(module,name)
#define STOPK(module,name) STOP(module,name); KPRNAME(module,name)

#else 

#define START(module,name)
#define STOP(module,name)
#define STOPP(module,name)
#define STOPK(module,name)
#define ISTART(module,name)
#define FDSTATINIT(module,name)
#define PRNAME(module,name)
#define KPRNAME(module,name)
#endif

#endif /* _FD_STAT_H_ */
