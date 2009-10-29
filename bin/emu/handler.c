
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <exos/callcount.h>
#include <exos/oscallstr.h>
#include <exos/uwk.h>
#include <fcntl.h>
#include <fd/proc.h>
#undef PID_MAX
#include <fd/udp/udp_struct.h>
#include <exos/osdecl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xok/kerncallstr.h>
#include <xok/sys_ucall.h>
#include "debug.h"
#include "emubrk.h"
#include "emustat.h"
#include "handler.h"
#include "interim_syscalls.h"
#include "openbsdsyscalls.h"


int	_getlogin __P((char *, size_t));

struct reg_storage r_s;
struct int_storage i_s;
/*int (*handlers[256]());
int (*nyt_handlers[256]());
int (*debug_handlers[256]());
*/
int handlers[256];
int real_handlers[256];
int nyt_handlers[256];
int debug_handlers[256];

extern int dlevel;

extern u_int __os_calls;
extern u_int __os_call_no;
extern u_int __os_call_count_arr[];
#ifdef KERN_CALL_COUNT
extern u_int *__os_kern_call_count_arr[];
static u_int temp_kern_counts[256];
#endif

void count_pre(int call)
{
#ifdef KERN_CALL_COUNT
  int i;
#endif

  __os_calls++;
#ifdef KERN_CALL_COUNT
  for (i=0; i < 256; i++)
    temp_kern_counts[i] =  __kern_call_counts[i];
#endif
  __os_call_count_arr[call]++;
  __os_call_no = call;
}

void count_post()
{
#ifdef KERN_CALL_COUNT
  int i;

  for (i=0; i < 256; i++)
    __os_kern_call_count_arr[__os_call_no][i] +=
      __kern_call_counts[i] - temp_kern_counts[i];
#endif
}

int chflagsx(const char *path, u_long flags)
{
  return 0;
}

int emu_brkx(const char *addr)
{
  int r;

  printf("X");
  r = emu_brk(addr);
  printf("Y");

  return r;
}

extern u_int *dargs;
int compat_43_sys_getkerninfox(int op, char *where, int *size, int arg)
{
  int i;
  for (i=0; i < 20; i++)
    printf("%2d %p: 0x%x\n",i,&dargs[i],dargs[i]);
  exit(0);
}

int emu_fsync(int fd) {
  sys_cputs("EMULATOR: fsync not supported\n");
  return 0;
}

int sigprocmaskx(int how, sigset_t set)
{
  int r;
  sigset_t o;

  r = sigprocmask(how, &set, &o);

  if (!r) r_s.ecx = o;
  return r;    
}

int execvex(const char *path, char *const argv[], char *const envp[])
{
  int r;

  kprintf("[%s %p %p]",path, argv, envp);

  r = execve(path, argv, envp);
  kprintf("[%d %d]", r, errno);

  return r;
}

pid_t forkx(void)
{
  int r;

  if (r = fork()) 
    r_s.edx = 0;
  else
    r_s.edx = 1;

  return r;
}

pid_t vforkx(void)
{
  int r;

  if (r = vfork()) 
    r_s.edx = 0;
  else
    r_s.edx = 1;

  return r;
}

int mprotectx(caddr_t addr, size_t len, int prot)
{
  return mprotect(addr,len,prot & ~PROT_EXEC);
}

int closex(int d)
{
  extern int dfd;
  if (d == 0 || d == 1 || d == dfd) return 0;
  return close(d);
}

int pipex(int *fildes)
{
  int r;

  r = pipe(fildes);

  /* special case pipex */
  r_s.edx = fildes[1];
  return fildes[0];
}

int revokex(const char *path)
{
  return 0;
}

int issetugidx(void)
{
  return issetugid();
}

int ioctlx(int d, unsigned long request, char *argp)
{
  int r;

kprintf("[%d %lx %p]\n",d, request, argp);
if (argp) kprintf("[%x]",*(int*)argp);
  r = ioctl(d, request, argp);
kprintf("[%d]",r);
if (argp) kprintf("[%x]",*(int*)argp);

  return r;
}

int fcntlx(int fd, int cmd, int arg)
{
  int r;

  kprintf("[%d %x %x -",fd, cmd, arg);
  r = fcntl(fd, cmd, arg);
  kprintf(" %d %d]",r,errno);
  return r;
}

int emul_fcntl(int fd, int cmd, int arg)
{
  int op;

  /* XXX - use flock instead of record locking */
  if (cmd == F_SETLK) {
    switch (((struct flock*)arg)->l_type) {
    case F_RDLCK:
      //      op = LOCK_SH | LOCK_NB; XXX
      op = LOCK_EX | LOCK_NB;
      break;
    case F_UNLCK:
      op = LOCK_UN | LOCK_NB;
      break;
    case F_WRLCK:
      op = LOCK_EX | LOCK_NB;
      break;
    default:
      op = 0;
    }
    return flock(fd, op);
  }
  if (cmd == F_SETLKW) {
    switch (((struct flock*)arg)->l_type) {
    case F_RDLCK:
      //      op = LOCK_SH; XXX
      op = LOCK_EX;
      break;
    case F_UNLCK:
      op = LOCK_UN;
      break;
    case F_WRLCK:
      op = LOCK_EX;
      break;
    default:
      op = 0;
    }
    return flock(fd, op);
  }

  return fcntl(fd, cmd, arg);
}

caddr_t mmapx(caddr_t addr, size_t len, int prot, int flags, int fd, int pad,
	      off_t offset)
{
  static int i = 1;
  if (!i) {
    i = 1;
    return addr;
  }

  /* extra padding must be ignored */
  return mmap(addr, len, prot, flags, fd, offset);
}

quad_t readx(int d, void *buf, quad_t nbytes)
{
  quad_t r;

  r = read(d,buf,(int) nbytes);
  kprintf("[%d %d %d %d]",d, (int)nbytes, (int)r, errno);
  if (r < 10)
    {
      int i;
      kprintf("[");
      for (i=0; i < r; i++)
	kprintf("%x ",((char*)buf)[i]);
      kprintf("]");
    }

  return r;
}

quad_t writex(int d, void *buf, quad_t nbytes)
{
  quad_t r;

  kprintf("[%d %d]",d, (int)nbytes);
  r = write(d,buf,(int) nbytes);
  kprintf("[%d %d]",(int)r, errno);
  if (r < 10)
    {
      int i;
      kprintf("[");
      for (i=0; i < r; i++)
	kprintf("%x ",((char*)buf)[i]);
      kprintf("]");
    }

  return r;
}

int accessx(const char *path, int mode)
{
  kprintf("[%s]",path);
  return access(path,mode);
}

int getsocknamex(int s, struct sockaddr *name, int *namelen)
{
  int r;

  r = getsockname(s, name, namelen);
  return r;
}

int getpeernamex(int s, struct sockaddr *name, int *namelen)
{
  int r;

  r = getpeername(s, name, namelen);
  return r;
}

int connectx(int s, struct sockaddr *name, int namelen)
{
  int r;

  kprintf("{c: %x %x}", s, name->sa_family);
  r = connect(s, name, namelen);
  kprintf("{%x %x}", r, name->sa_family);
  return r;
}

/* select stuff */

#define	OBSDNBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	OBSDFD_SETSIZE
#define	OBSDFD_SETSIZE	256
#endif

typedef int32_t	obsd_fd_mask;
#define OBSDNFDBITS	(sizeof(obsd_fd_mask) * OBSDNBBY)	/* bits per mask */

#ifndef obsdhowmany
#define	obsdhowmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

typedef	struct obsd_fd_set {
	fd_mask	fds_bits[howmany(OBSDFD_SETSIZE, OBSDNFDBITS)];
} obsd_fd_set;
int selectx(int nfds, obsd_fd_set *readfds, obsd_fd_set *writefds,
	    obsd_fd_set *exceptfds, struct timeval *timeout)
{
  int r;
  fd_set re, w, e, *rep = NULL, *wp = NULL, *ep = NULL;


  if (readfds)
    {
      rep = &re;
      memset(&re,'\0',sizeof(fd_set));
      memcpy(&re, readfds, MIN(sizeof(fd_set),sizeof(obsd_fd_set)));
    }
  if (writefds)
    {
      wp = &w;
      memset(&w,'\0',sizeof(fd_set));
      memcpy(&w, writefds, MIN(sizeof(fd_set),sizeof(obsd_fd_set)));
    }
  if (exceptfds) 
    {
      ep = &e;
      memset(&e,'\0',sizeof(fd_set));
      memcpy(&e, exceptfds, MIN(sizeof(fd_set),sizeof(obsd_fd_set)));
    }

  r = select(nfds, rep, wp, ep, timeout);

  if (readfds) memcpy(readfds, &re, MIN(sizeof(fd_set),sizeof(obsd_fd_set)));
  if (writefds) memcpy(writefds, &w, MIN(sizeof(fd_set),sizeof(obsd_fd_set)));
  if (exceptfds) memcpy(exceptfds, &e, 
			MIN(sizeof(fd_set),sizeof(obsd_fd_set)));

  return r;
}

int socketx(int domain, int type, int protocol)
{
  int s;

  kprintf("{t: %x}", type);
  s =  socket(domain, type, protocol);
  kprintf("{s: %x}", s);
  return s;
}

quad_t recvfromx(int s, void *buf, int len, int flags, struct sockaddr *from,
		 int *fromlen)
{
  quad_t r = 0;
  int l = len;

  /* it doesn't make sense, len is supposed to be quad_t!!! */
  r = recvfrom(s, buf, l, flags, from, fromlen);
  return r;
}

quad_t
sendtox(int s, void *msg, quad_t len, int flags,
	struct sockaddr *to, int tolen)
{
  int l = len;
  quad_t q;

  if (!to)
    {
      to = (struct sockaddr*)&((struct socket_data *) &__current->fd[s]->data)->tosockaddr;
      tolen = sizeof(struct sockaddr_in);
    }
  q = sendto(s, msg, l, flags, to, tolen);

  return q;
}

#ifdef KERN_CALL_COUNT
void exitx(int x)
{
  int i, j, sum;

  count_post();
  printf("total number of os calls: %d\n", __os_calls);
  printf("os_call                       num os calls   num kern calls    "
	 "kern/os calls\n");
  for (i=0; i < 256; i++)
    if (__os_call_count_arr[i] > 0)
      {
	sum = 0;
	for (j=0; j < 256; j++)
	  sum += __os_kern_call_count_arr[i][j];
	printf("%-30s%8d        %8d           %8.4f\n", 
	       oscallstr(i), __os_call_count_arr[i], sum, 
	       ((float)sum) / (float)(__os_call_count_arr[i]));
	if (sum)
	  for (j=0; j < 256; j++)
	    {
	      if (__os_kern_call_count_arr[i][j])
		printf("\t[%d] %s\n", __os_kern_call_count_arr[i][j], 
		       kerncallstr(j));
	      free(__os_kern_call_count_arr[i]);
	    }
      }
  exit(x);
}
#endif

int readlinkx(const char *path, char *buf, int bufsiz)
{
  if (path[0] == '/')
    {
      errno = ENOENT;
      return -1;
    }
  return readlink(path, buf, bufsiz);
}

int openx(const char *path, int flags, mode_t mode)
{
  int r;

  /*  if (path[0] == '/')
    {
      errno = ENOENT;
      return -1;
    } */
  if (!strcmp(path,"/dev/tty"))
    {
      int i;

      for (i=0; i < 64; i++)
	{
	  if ((__current->fd[i] != 0) && isatty(i))
	    {
	      r = dup(i);
	      goto good;
	    }
	}
      assert(0);
    good:      
    }
  else
    r = open(path,flags,mode);

  return r;
}

void nys()
{
  char *s = oscallstr((int)r_s.eax);

  kprintf("\nNot yet supported syscall: %d(%s)\n",(int)r_s.eax,
	  s ? s : "UNKNOWN"); 
  exit(0);
}

u_int nyt_scall;
void nyt()
{
  char *s = oscallstr((int)r_s.eax);

  nyt_scall = nyt_handlers[r_s.eax];
  kprintf("\nNot yet tested syscall: %d(%s)\n",(int)r_s.eax,
	  s ? s : "UNKNOWN"); 
  asm("leave\n"
      "\tjmp %0" :: "r" (nyt_scall));
}

void obs()
{
  char *s = oscallstr((int)r_s.eax);

  kprintf("\nObsoleted (and unsupported) syscall: %d(%s)\n",(int)r_s.eax,
	  s ? s : "UNKNOWN"); 
  exit(0);
}

u_int dcount = 0;
u_int dret_addrs[5];
int dfd = -1;
FILE *dfp;
int InIndirect = 0;
int forked_child = 0;
void debug_handler_pre()
{
  if (r_s.eax == SYS___syscall) InIndirect = 2;
  /* open log file */
  if (dlevel == 3)
    {
      if (dfd == -1)
	{
	  char fname[30];

	  if (forked_child)
	    {
	      sprintf(fname,"emulate.log.%d",getpid());
	      forked_child = 0;
	    }
	  else
	    strcpy(fname,"emulate.log");
	  dfd = open(fname,O_CREAT | O_WRONLY | O_APPEND,0600);
	  if (dfd < 0)
	    {
	      printf("Error opening %s: %s(%d)\n",fname,strerror(errno),
		     errno);
	      exit(0);
	    }
	  dfp = fdopen(dfd,"w");
	  if (!dfp)
	    {
	      printf("Error opening %s: %s(%d)\n",fname,strerror(errno),
		     errno);
	      exit(0);
	    }
	  /*	  setvbuf(dfp,malloc(0x1000),_IOFBF,0x1000); */
	  fprintf(dfp,"\n************\n");
	  fflush(dfp);
	}
      detailed_pre(dfp);
    }
  else
    kprintf("(%d)",r_s.eax);
}

u_int derrno;
void debug_handler_post(u_int regs)
{ 
  derrno = errno;
  if (dlevel == 3)
    detailed_post(dfp,(struct reg_storage*)&regs);
  else
    kprintf("(%d)",r_s.eax);
  errno = derrno;
  if (InIndirect) InIndirect--;
}

void unknown()
{
  kprintf("\nUnknown syscall: %d\n",(int)r_s.eax);
  exit(0);
}

u_int scallx_pad, scallx_retaddr, scallx_scnum;

int emu_sysarch(int number, char *args)
{
  return 0;
}

int emu_getpriority(int which, int who)
{
  return 0;
}

int emu_setpriority(int which, int who, int prio)
{
  return 0;
}

/* can't read more than 1024 from stdin at a time */
ssize_t readxx(int d, void *buf, size_t nbytes)
{
  if (d == 0 && nbytes > 1024)
    return read(d, buf, 1024);
  else
    return read(d, buf, nbytes);
}

extern void (asm_debug_handler)();
extern void (__syscallx)();
void init_handlers()
{
  int i;

  DEF_SYM(r_s_edi,&r_s.edi);
  DEF_SYM(r_s_esi,&r_s.esi);
  DEF_SYM(r_s_ebp,&r_s.ebp);
  DEF_SYM(r_s_esp,&r_s.esp);
  DEF_SYM(r_s_ebx,&r_s.ebx);
  DEF_SYM(r_s_edx,&r_s.edx);
  DEF_SYM(r_s_ecx,&r_s.ecx);
  DEF_SYM(r_s_eax,&r_s.eax);
  DEF_SYM(i_s_iip,&i_s.iip);
  DEF_SYM(i_s_ics,&i_s.ics);
  DEF_SYM(i_s_flags,&i_s.flags);
  DEF_SYM(i_s_rta,&i_s.rta); 

  for (i=0; i < 256; i++)
    {
      nyt_handlers[i] = real_handlers[i] = (int)unknown;
      debug_handlers[i] = (int)asm_debug_handler;
    }
  real_handlers[0] = (int)nys; /* syscall */
  real_handlers[1] = (int)exit;
  real_handlers[2] = (int)forkx;
  real_handlers[3] = (int)readxx;
  real_handlers[4] = (int)write;
  real_handlers[5] = (int)openx;
  real_handlers[6] = (int)closex;
  real_handlers[7] = (int)wait4;

  real_handlers[9] = (int)link;
  real_handlers[10] = (int)unlink;

  real_handlers[12] = (int)chdir;
  real_handlers[13] = (int)fchdir;
  real_handlers[14] = (int)mknod;
  real_handlers[15] = (int)chmod;
  real_handlers[16] = (int)chown;
  real_handlers[17] = (int)emu_brk;
  real_handlers[18] = (int)getfsstat;

  real_handlers[20] = (int)getpid;
  real_handlers[21] = (int)nys; /* mount */
  real_handlers[22] = (int)nys; /* unmount */
  real_handlers[23] = (int)setuid;
  real_handlers[24] = (int)getuid;
  real_handlers[25] = (int)geteuid;
  real_handlers[26] = (int)ptrace;
  real_handlers[27] = (int)nys; /* recvmsg */
  real_handlers[28] = (int)nys; /* sendmsg */
  real_handlers[29] = (int)recvfrom;
  real_handlers[30] = (int)accept;
  real_handlers[31] = (int)getpeername;
  real_handlers[32] = (int)getsockname;
  real_handlers[33] = (int)access;
  real_handlers[34] = (int)chflagsx; /* chflags */
  real_handlers[35] = (int)fchflags;
  real_handlers[36] = (int)nys; /* sync */
  real_handlers[37] = (int)kill;

  real_handlers[39] = (int)getppid;

  real_handlers[41] = (int)dup;
  real_handlers[42] = (int)pipex;
  real_handlers[43] = (int)getegid;
  real_handlers[44] = (int)nys; /* profil */
  real_handlers[45] = (int)nys; /* ktrace */
  real_handlers[46] = (int)sigaction;
  real_handlers[47] = (int)getgid;
  real_handlers[48] = (int)sigprocmaskx;
  real_handlers[49] = (int)_getlogin;
  real_handlers[50] = (int)setlogin;
  real_handlers[51] = (int)nys; /* acct */
  real_handlers[52] = (int)sigpending;
  real_handlers[53] = (int)sigaltstack;
  real_handlers[54] = (int)ioctl;
  real_handlers[55] = (int)nys; /* reboot */
  real_handlers[56] = (int)revokex;
  real_handlers[57] = (int)symlink;
  real_handlers[58] = (int)readlink;
  real_handlers[59] = (int)execve;
  real_handlers[60] = (int)umask;
  real_handlers[61] = (int)chroot;

  real_handlers[63] = (int)compat_43_sys_getkerninfox;
  real_handlers[64] = (int)getpagesize; /* compat_43 */
  real_handlers[65] = (int)msync;
  real_handlers[66] = (int)vforkx;


  real_handlers[69] = (int)emu_sbrk;
  real_handlers[70] = (int)nys; /* sstk */

  real_handlers[72] = (int)nys; /* vadvise */
  real_handlers[73] = (int)munmap;
  real_handlers[74] = (int)mprotectx;
  real_handlers[75] = (int)nys; /* madvise */


  real_handlers[78] = (int)nys; /* mincore */
  real_handlers[79] = (int)getgroups;
  real_handlers[80] = (int)setgroups;
  real_handlers[81] = (int)getpgrp;
  real_handlers[82] = (int)setpgid;
  real_handlers[83] = (int)interim_setitimer;

  real_handlers[85] = (int)nys; /* swapon */
  real_handlers[86] = (int)nys; /* getitimer */


  real_handlers[89] = (int)getdtablesize; /* compat_43 */
  real_handlers[90] = (int)dup2;

  real_handlers[92] = (int)emul_fcntl;
  real_handlers[93] = (int)selectx;

  real_handlers[95] = (int)emu_fsync;
  real_handlers[96] = (int)emu_setpriority; /* setpriority */
  real_handlers[97] = (int)socket;
  real_handlers[98] = (int)connect;

  real_handlers[100] = (int)emu_getpriority; /* getpriority */


  real_handlers[103] = (int)sigreturn;
  real_handlers[104] = (int)bind;
  real_handlers[105] = (int)setsockopt;
  real_handlers[106] = (int)listen;




  real_handlers[111] = (int)sigsuspend;



  real_handlers[115] = (int)obs; /* vtrace */
  real_handlers[116] = (int)gettimeofday;
  real_handlers[117] = (int)getrusage;
  real_handlers[118] = (int)getsockopt;

  real_handlers[120] = (int)readv;
  real_handlers[121] = (int)writev;
  real_handlers[122] = (int)settimeofday;
  real_handlers[123] = (int)fchown;
  real_handlers[124] = (int)fchmod;



  real_handlers[128] = (int)rename;


  real_handlers[131] = (int)flock;
  real_handlers[132] = (int)nys; /* mkfifo */
  real_handlers[133] = (int)sendto;
  real_handlers[134] = (int)shutdown;
  real_handlers[135] = (int)nys; /* socketpair */
  real_handlers[136] = (int)mkdir;
  real_handlers[137] = (int)rmdir;
  real_handlers[138] = (int)utimes;

  real_handlers[140] = (int)nys; /* adjtime */






  real_handlers[147] = (int)setsid;
  real_handlers[148] = (int)nys; /* quotactl */






  real_handlers[155] = (int)nys; /* nfssvc */

  real_handlers[157] = (int)statfs;
  real_handlers[158] = (int)fstatfs;


  real_handlers[161] = (int)obs; /* getfh */



  real_handlers[165] = (int)emu_sysarch; /* sysarch */









  real_handlers[175] = (int)obs; /* ntp_gettime */
  real_handlers[176] = (int)obs; /* ntp_adjtime */




  real_handlers[181] = (int)setgid;
  real_handlers[182] = (int)setegid;
  real_handlers[183] = (int)seteuid;
  real_handlers[184] = (int)obs; /* lfs_bmapv */
  real_handlers[185] = (int)obs; /* lfs_markv */
  real_handlers[186] = (int)obs; /* lfs_segclean */
  real_handlers[187] = (int)obs; /* lfs_segwait */
  real_handlers[188] = (int)stat;
  real_handlers[189] = (int)fstat;
  real_handlers[190] = (int)lstat;
  real_handlers[191] = (int)pathconf;
  real_handlers[192] = (int)fpathconf;

  real_handlers[194] = (int)getrlimit;
  real_handlers[195] = (int)setrlimit;
  real_handlers[196] = (int)getdirentries;
  real_handlers[197] = (int)mmapx;
  real_handlers[198] = (int)__syscallx;
  real_handlers[199] = (int)interim_lseek;
  real_handlers[200] = (int)truncate;
  real_handlers[201] = (int)ftruncate;
  real_handlers[202] = (int)sysctl;
  real_handlers[203] = (int)nys; /* mlock */
  real_handlers[204] = (int)nys; /* munlock */
  real_handlers[205] = (int)nys; /* undelete */
  real_handlers[206] = (int)futimes;













  real_handlers[220] = (int)nys; /* __semctl */
  real_handlers[221] = (int)nys; /* semget */
  real_handlers[222] = (int)nys; /* semop */
  real_handlers[223] = (int)nys; /* semconfig */
  real_handlers[224] = (int)nys; /* msgctl */
  real_handlers[225] = (int)nys; /* msgget */
  real_handlers[226] = (int)nys; /* msgsnd */
  real_handlers[227] = (int)nys; /* msgrcv */
  real_handlers[228] = (int)shmat;
  real_handlers[229] = (int)shmctl;
  real_handlers[230] = (int)shmdt;
  real_handlers[231] = (int)shmget;







  real_handlers[240] = (int)nanosleep;











  real_handlers[250] = (int)nys; /* minherit */
  real_handlers[251] = (int)nys; /* rfork */
  real_handlers[252] = (int)interim_poll; /* poll */
  real_handlers[253] = (int)issetugid;

  if (dlevel == 2 || dlevel == 3)
    for (i=0; i < 256; i++)
      handlers[i] = debug_handlers[i];
  else
    for (i=0; i < 256; i++)
      handlers[i] = real_handlers[i];
}

/*

  syscall - not supported
  forkx - requires edx to be set on return
  readxx - libexos can't read more than 1024 from stdin at a time...
  openx - special attention to /dev/tty
  closex - won't close stdin/out or the emulator debug file
  emu_brk - maintain break separate from emulator's libexos break
  mount - not supported - different interface in libexos
  unmount - not supported - different interface in libexos
  recvmsg - not supported on libexos
  sendmsg - not supported on libexos
  chflagsx - programs require this to return 0, libexos returns -1
  sync - not supported on libexos
  pipex - requires edx to be set on return
  profil - not supported on libexos
  ktrace - not supported on libexos
  sigprocmaskx - requires ecx to be set on return
  acct - not supported on libexos
  reboot - not supported on libexos
  revokex - not supported on libexos, returns 0
  compat_43_sys_getkerninfox - shoudn't be getting called
  vforkx - requires edx to be set on return
  emu_sbrk - maintain break separate from emulator's libexos break
  sstk - not supported on libexos
  vadvise - not supported on libexos
  mprotectx - libexos can't have PROT_EXEC
  madvise - not supported on libexos
  mincore - not supported on libexos
  interim_setitimer - stub since not supported on libexos
  swapon - not supported on libexos
  getitimer - not supported on libexos
  selectx - different way/size of dealing with FDSET's
  fsync - not supported on libexos
  emu_setpriority - return 0, not supported on libexos
  emu_getpriority - return 0, not supported on libexos
  flock - not supported on libexos
  mkfifo - not supported on libexos
  socketpair - not supported on libexos
  adjtime - not supported on libexos
  quotactl - not supported on libexos
  nfssvc - not supported on libexos
  sysarch - not supported on libexos
  pathconf - not supported on libexos
  fpathconf - not supported on libexos
  mmapx - ignore extra padding
  __syscallx - special to call the emulated syscall and not the libos one
  interim_lseek - padding and edx
  mlock - not supported on libexos
  munlock - not supported on libexos
  undelete - not supported on libexos
  __semctl - not supported on libexos
  semget - not supported on libexos
  semop - not supported on libexos
  semconfig - not supported on libexos
  msgctl - not supported on libexos
  msgget - not supported on libexos
  msgsnd - not supported on libexos
  msgrcv - not supported on libexos
  minherit - not supported on libexos
  rfork - not supported on libexos
  interim_poll - not in libexos, emulated poll with select

 */
