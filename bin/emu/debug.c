
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <machine/cpu.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vm/vm_param.h>
#include "handler.h"
#include "interim_syscalls.h"
#include "ioctl_table.h"
#include "openbsdsyscalls.h"
#include "openbsdsysarch.h"

#define SYS_compat_43_sys_getkerninfo 63
#define SYS_ogetpagesize 64
void print_debug_data(FILE *fp, char *cp, int len)
{
  int x = len, count = 0;

  while (x > 0)
    {
      if (count == 0)
	fprintf(fp,"\n\t\t\"");
      else if (count >= 55)
	{
	  fprintf(fp,"\"\n\t\t\"");
	  count = 0;
	}
      if (*cp == '\\')
	{
	  fprintf(fp,"\\\\");
	  count += 2;
	}
      else if (*cp >= ' ' && *cp <= '~')
	{
	  fprintf(fp,"%c",*cp);
	  count++;
	}
      else if (*cp == '\n')
	{
	  fprintf(fp,"\"");
	  count = 0;
	}
      else 
	{
	  fprintf(fp,"\\%02x",*cp & 0xff);
	  count += 3;
	}
      cp++;
      x--;
    }
  if (count != 0) fprintf(fp,"\"");
}

ssize_t calc_vtotal(const struct iovec *iov, int iovcnt)
{
  ssize_t bcount = 0;
  int vcount;

  for (vcount = 0; vcount < iovcnt; vcount++, iov++)
    bcount += iov->iov_len;

  return bcount;
}

void print_debug_datav(FILE *fp, const struct iovec *iov, int iovcnt,
		       int nbytes)
{
  int x, count = 0, i = 0;
  u_int total = nbytes < 0 ? 0xFFFFFFFF : nbytes;
  char *cp;

  while (iovcnt > i && total > 0)
    {
      x = iov[i].iov_len;
      cp = iov[i].iov_base;
      i++;
      while (x > 0 && total > 0)
	{
	  if (count == 0)
	    fprintf(fp,"\n\t\t\"");
	  else if (count >= 55)
	    {
	      fprintf(fp,"\"\n\t\t\"");
	      count = 0;
	    }
	  if (*cp == '\\')
	    {
	      fprintf(fp,"\\\\");
	      count += 2;
	    }
	  else if (*cp >= ' ' && *cp <= '~')
	    {
	      fprintf(fp,"%c",*cp);
	      count++;
	    }
	  else if (*cp == '\n')
	    {
	      fprintf(fp,"\"");
	      count = 0;
	    }
	  else 
	    {
	      fprintf(fp,"\\%02x",*cp & 0xff);
	      count += 3;
	    }
	  cp++;
	  x--;
	  total--;
	}
      if (count != 0) fprintf(fp,"\"");
    }
}

u_int *dargs;
extern struct reg_storage r_s;
extern u_int derrno;
extern int InIndirect;
extern int forked_child;
extern int dfd;
void detailed_pre(FILE *fp)
{
  fprintf(fp,"%5d: ",getpid());
  switch(r_s.eax)
    {      
    case SYS_syscall:
      fprintf(fp,"syscall()");
      break;
    case SYS_exit:
      fprintf(fp,"exit(int status)\n");
      fprintf(fp,"\tstatus = %d",dargs[0]);
      break;
    case SYS_fork:
      fprintf(fp,"fork()");
      break;
    case SYS_read:
      fprintf(fp,"read(int d, void *buf, size_t nbytes)\n");
      fprintf(fp,"\td = %d\n",dargs[0]);
      fprintf(fp,"\tbuf = %p\n",(void*)dargs[1]);
      fprintf(fp,"\tnbytes = %d",dargs[2]);
      break;
    case SYS_write:
      fprintf(fp,"write(int d, const void *buf, size_t nbytes)\n");
      fprintf(fp,"\td = %d\n",dargs[0]);
      fprintf(fp,"\tbuf = %p\n",(void*)dargs[1]);
      fprintf(fp,"\tnbytes = %d",dargs[2]);
      if (dargs[1]) print_debug_data(fp, (char*)dargs[1], dargs[2]);
      break;
    case SYS_open:
      fprintf(fp,"open(const char *path, int flags, mode_t mode)\n");
      fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      fprintf(fp,"\tflags = 0x%x\n",dargs[1]);
      if (dargs[1] & O_ACCMODE == O_RDONLY) fprintf(fp,"\t\tO_RDONLY\n");
      if (dargs[1] & O_ACCMODE == O_WRONLY) fprintf(fp,"\t\tO_WRONLY\n");
      if (dargs[1] & O_ACCMODE == O_RDWR) fprintf(fp,"\t\tO_RDWR\n");
      if (dargs[1] & O_NONBLOCK) fprintf(fp,"\t\tO_NONBLOCK\n");
      if (dargs[1] & O_APPEND) fprintf(fp,"\t\tO_APPEND\n");
      if (dargs[1] & O_CREAT) fprintf(fp,"\t\tO_CREAT\n");
      if (dargs[1] & O_TRUNC) fprintf(fp,"\t\tO_TRUNC\n");
      if (dargs[1] & O_EXCL) fprintf(fp,"\t\tO_EXCL\n");
      if (dargs[1] & O_SHLOCK) fprintf(fp,"\t\tO_SHLOCK\n");
      if (dargs[1] & O_EXLOCK) fprintf(fp,"\t\tO_EXLOCK\n");
      if (dargs[1] & O_ASYNC) fprintf(fp,"\t\tO_ASYNC\n");
      if (dargs[1] & O_FSYNC) fprintf(fp,"\t\tO_FSYNC\n");
      fprintf(fp,"\tmode = 0%o",dargs[2]);
      break;
    case SYS_close:
      fprintf(fp,"close(int d)\n");
      fprintf(fp,"\td = %d",dargs[0]);
      break;
    case SYS_wait4:
      fprintf(fp,"wait4(pid_t wpid, int *status, int options,\n"
	      "\tstruct rusage *rusage)\n");
      fprintf(fp,"\twpid = %d\n",dargs[0]);
      fprintf(fp,"\tstatus = %p\n",(void*)dargs[1]);
      fprintf(fp,"\toptions = 0x%x\n",dargs[2]);
      if (dargs[2] & WNOHANG) fprintf(fp,"\t\tWNOHANG\n");
      if (dargs[2] & WUNTRACED) fprintf(fp,"\t\tWUNTRACED\n");
      fprintf(fp,"\trusage = %p",(void*)dargs[3]);
      break;
    case SYS_link:
      fprintf(fp,"link(const char *name1, const char *name2)");
      if (dargs[0])
	fprintf(fp,"\n\tname1 = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tname1 = NULL");
      if (dargs[1])
	fprintf(fp,"\n\tname2 = %s",(char*)dargs[1]);
      else
	fprintf(fp,"\n\tname2 = NULL");
      break;
    case SYS_unlink:
      fprintf(fp,"unlink(const char *path)");
      if (dargs[0])
	fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tpath = NULL");
      break;
    case SYS_chdir:
      fprintf(fp,"chdir(const char *path)");
      if (dargs[0])
	fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tpath = NULL");
      break;
    case SYS_fchdir:
      fprintf(fp,"fchdir(int fd)\n"
	      "\tfd = %d",dargs[0]);
      break;
    case SYS_mknod:
      fprintf(fp,"mknod(const char *path, mode_t mode, dev_t dev)");
      if (dargs[0])
	fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tpath = NULL");
      fprintf(fp,"\n\tmode = 0%o",dargs[1]);
      fprintf(fp,"\n\tdev = 0x%x",dargs[2]);
      break;
    case SYS_chmod:
      fprintf(fp,"chmod(const char *path, mode_t mode)");
      if (dargs[0])
	fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tpath = NULL");
      fprintf(fp,"\n\tmode = 0%o",dargs[1]);
      break;
    case SYS_chown:
      fprintf(fp,"chown(const char *path, uid_t owner, gid_t group)");
      if (dargs[0])
	fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      else
	fprintf(fp,"\n\tpath = NULL");
      fprintf(fp,"\n\towner = %d",dargs[1]);
      fprintf(fp,"\n\tgroup = %d",dargs[2]);
      break;
    case SYS_break:
      fprintf(fp,"break(const char *addr)\n");
      fprintf(fp,"\taddr = %p",(void*)dargs[0]);
      break;
    case SYS_getfsstat:
      fprintf(fp,"getfsstat(struct statfs *buf, long bufsize, int flags)\n"
	      "\tbuf = %p\n"
	      "\tbufsize = %d\n"
	      "\tflags = 0x%x",(void*)dargs[0],dargs[1],dargs[2]);
      if (dargs[2] == MNT_WAIT) fprintf(fp,"\n\t\tMNT_WAIT");
      else if (dargs[2] == MNT_NOWAIT) fprintf(fp,"\n\t\tMNT_NOWAIT");
      break;
    case SYS_getpid:
      fprintf(fp,"getpid(void)");
      break;
    case SYS_mount:
      fprintf(fp,"mount");
      break;
    case SYS_unmount:
      fprintf(fp,"unmount");
      break;
    case SYS_setuid:
      fprintf(fp,"setuid(uid_t uid)\n"
	      "\tuid = %d",dargs[0]);
      break;
    case SYS_getuid:
      fprintf(fp,"getuid(void)");
      break;
    case SYS_geteuid:
      fprintf(fp,"geteuid(void)");
      break;
    case SYS_ptrace:
      fprintf(fp,"ptrace(int request, pid_t pid, caddr_t addr, int data)\n"
	      "\trequest = %d\n", (int)dargs[0]);
      if (dargs[0] == PT_TRACE_ME) fprintf(fp,"\t\tPT_TRACE_ME\n");
      else if (dargs[0] == PT_READ_I) fprintf(fp,"\t\tPT_READ_I\n");
      else if (dargs[0] == PT_READ_D) fprintf(fp,"\t\tPT_READ_D\n");
      else if (dargs[0] == PT_WRITE_I) fprintf(fp,"\t\tPT_WRITE_I\n");
      else if (dargs[0] == PT_WRITE_D) fprintf(fp,"\t\tPT_WRITE_D\n");
      else if (dargs[0] == PT_CONTINUE) fprintf(fp,"\t\tPT_CONTINUE\n");
      else if (dargs[0] == PT_KILL) fprintf(fp,"\t\tPT_KILL\n");
      else if (dargs[0] == PT_ATTACH) fprintf(fp,"\t\tPT_ATTACH\n");
      else if (dargs[0] == PT_DETACH) fprintf(fp,"\t\tPT_DETACH\n");
      else if (dargs[0] == PT_STEP) fprintf(fp,"\t\tPT_STEP\n");
      else if (dargs[0] == PT_GETREGS) fprintf(fp,"\t\tPT_GETREGS\n");
      else if (dargs[0] == PT_SETREGS) fprintf(fp,"\t\tPT_SETREGS\n");
      else if (dargs[0] == PT_GETFPREGS) fprintf(fp,"\t\tPT_GETFPREGS\n");
      else if (dargs[0] == PT_SETFPREGS) fprintf(fp,"\t\tPT_SETFPREGS\n");
      else fprintf(fp,"\t\tUNKNOWN\n");
      fprintf(fp,"\tpid = %d\n", (pid_t)dargs[1]);
      fprintf(fp,"\taddr = %p\n", (char*)dargs[2]);
      fprintf(fp,"\tdata = %d", (int)dargs[3]);
      break;
    case SYS_recvmsg:
      fprintf(fp,"recvmsg");
      break;
    case SYS_sendmsg:
      fprintf(fp,"sendmsg");
      break;
    case SYS_recvfrom:
      fprintf(fp,"recvfrom(int s, void *buf, size_t len, int flags,\n"
	      "\tstruct sockaddr *from, int *fromlen)\n");
      fprintf(fp,"\ts = %d\n", dargs[0]);
      fprintf(fp,"\tbuf = %p\n", (void*)dargs[1]);
      fprintf(fp,"\tlen = %d\n", dargs[2]);
      fprintf(fp,"\tflags = %x\n", dargs[3]);
      if (dargs[3] & MSG_OOB) fprintf(fp,"\t\tMSG_OOB\n");
      if (dargs[3] & MSG_PEEK) fprintf(fp,"\t\tMSG_PEEK\n");
#define MSG_WAITALL 0x40 /* wait for full request or error */
      if (dargs[3] & MSG_WAITALL) fprintf(fp,"\t\tMSG_WAITALL\n");
      fprintf(fp,"\tfrom = %p\n", (void*)dargs[4]);
      fprintf(fp,"\tfromlen = %p", (void*)dargs[5]);
      if (dargs[5]) fprintf(fp,"\n\t*fromlen = %d", *(int*)dargs[5]);
      break;
    case SYS_accept:
      fprintf(fp,"accept");
      break;
    case SYS_getpeername:
      fprintf(fp,"getpeername(int s, struct sockaddr *name, int *namelen)\n");
      fprintf(fp,"\ts = %d\n",dargs[0]);
      fprintf(fp,"\tname = %p\n",(void*)dargs[1]);
      fprintf(fp,"\tnamelen = %p",(void*)dargs[2]);
      if (dargs[2]) fprintf(fp,"\n\t*namelen = %d",*(int*)dargs[2]);
      break;
    case SYS_getsockname:
      fprintf(fp,"getsockname(int s, struct sockaddr *name, int *namelen)\n");
      fprintf(fp,"\ts = %d\n",dargs[0]);
      fprintf(fp,"\tname = %p\n",(void*)dargs[1]);
      fprintf(fp,"\tnamelen = %p",(void*)dargs[2]);
      if (dargs[2]) fprintf(fp,"\n\t*namelen = %d",*(int*)dargs[2]);
      break;
    case SYS_access:
      fprintf(fp,"access(const char *path, int mode)\n");
      if (dargs[0])
	fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      else
	fprintf(fp,"\tpath = NULL\n");
      fprintf(fp,"\tmode = 0x%x",dargs[1]);
      break;
    case SYS_chflags:
      fprintf(fp,"chflags");
      break;
    case SYS_fchflags:
      fprintf(fp,"fchflags");
      break;
    case SYS_sync:
      fprintf(fp,"sync");
      break;
    case SYS_kill:
      fprintf(fp,"kill(pid_t pid, int sig)\n");
      fprintf(fp,"\tpid = %d\n",dargs[0]);
      fprintf(fp,"\tsig = (%d)%s",dargs[1],strsignal(dargs[1]));
      break;
    case SYS_getppid:
      fprintf(fp,"getppid(void)");
      break;
    case SYS_dup:
      fprintf(fp,"dup(int oldd)"
	      "\n\toldd = %d",dargs[0]);
      break;
    case SYS_pipe:
      fprintf(fp,"pipe(int *fildes)");
      fprintf(fp,"\n\tfiledes = %p",(void*)dargs[0]);
      break;
    case SYS_getegid:
      fprintf(fp,"getegid(void)");
      break;
    case ob_SYS_profil:
      fprintf(fp,"profil");
      break;
    case SYS_ktrace:
      fprintf(fp,"ktrace");
      break;
    case SYS_sigaction:
      fprintf(fp,"sigaction(int sig, const struct sigaction *act, struct "
	      "sigaction *oact)");
      fprintf(fp,"\n\tsig = (%d)%s",dargs[0],strsignal(dargs[0])); 
      fprintf(fp,"\n\tact = %p", (void*)dargs[1]);
      if (dargs[1]) {
	fprintf(fp,"\n\tact->sa_handler = %p",
		((struct sigaction*)dargs[1])->sa_handler);
	fprintf(fp,"\n\tact->sa_mask = ");
	{
	  int i, j=0;

	  for (i=1; i < 32; i++)
	    if (sigismember(&((struct sigaction*)dargs[1])->sa_mask, i)) {
	      fprintf(fp,"\n\t\t(%d)%s", i, strsignal(i));
	      j = 1;
	    }
	  if (!j) fprintf(fp,"(empty)");
	}
	fprintf(fp,"\n\tact->sa_flags = 0x%x",
		((struct sigaction*)dargs[1])->sa_flags);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_ONSTACK)
	  fprintf(fp,"\n\t\t(0x%x)SA_ONSTACK", SA_ONSTACK);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_RESTART)
	  fprintf(fp,"\n\t\t(0x%x)SA_RESTART", SA_RESTART);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_RESETHAND)
	  fprintf(fp,"\n\t\t(0x%x)SA_RESETHAND", SA_RESETHAND);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_NOCLDSTOP)
	  fprintf(fp,"\n\t\t(0x%x)SA_NOCLDSTOP", SA_NOCLDSTOP);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_NODEFER)
	  fprintf(fp,"\n\t\t(0x%x)SA_NODEFER", SA_NODEFER);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_NOCLDWAIT)
	  fprintf(fp,"\n\t\t(0x%x)SA_NOCLDWAIT", SA_NOCLDWAIT);
	if (((struct sigaction*)dargs[1])->sa_flags & SA_SIGINFO)
	  fprintf(fp,"\n\t\t(0x%x)SA_SIGINFO", SA_SIGINFO);
	if (((struct sigaction*)dargs[1])->sa_flags & 0x100) /* SA_USERTRAMP */
	  fprintf(fp,"\n\t\t(0x%x)SA_USERTRAMP", 0x100);
      }
      fprintf(fp,"\n\toact = %p", (void*)dargs[1]);
      break;
    case SYS_getgid:
      fprintf(fp,"getgid(void)");
      break;
    case SYS_sigprocmask:
      fprintf(fp,"sigprocmask(int how, const sigset_t *set, sigset_t *oset)");
      /* the 'how' defines are different between XOK & OpenBSD */
      fprintf(fp,"\n\thow = (%d)%s", dargs[0],
	      dargs[0] == 1 ? "SIG_BLOCK" :
	      dargs[0] == 2 ? "SIG_UNBLOCK" :
	      dargs[0] == 3 ? "SIG_SETMASK" : "UNKNOWN!");
      {
	int i;

	fprintf(fp,"\n\t*set = 0x%x", dargs[1]);
	for (i=1; i < 32; i++)
	  if (sigismember(&dargs[1], i))
	    fprintf(fp,"\n\t\t(%d)%s", i, strsignal(i));
      }
      break;
    case SYS_getlogin:
      fprintf(fp,"getlogin(void)");
      break;
    case SYS_setlogin:
      fprintf(fp,"setlogin(const char *name)\n");
      fprintf(fp,"\tname = %s", dargs[0] ? (char*)dargs[0] : "(NULL)");
      break;
    case SYS_acct:
      fprintf(fp,"acct");
      break;
    case SYS_sigpending:
      fprintf(fp,"sigpending");
      break;
    case SYS_sigaltstack:
      fprintf(fp,"sigaltstack");
      break;
    case SYS_ioctl:
      fprintf(fp,"ioctl(int d, unsigned long request, char *argp)\n");
      fprintf(fp,"\td = %d\n",dargs[0]);
      {
	char *s = get_ioctl_req(dargs[1]);

	fprintf(fp,"\trequest = (0x%x)%s\n", dargs[1], s ? s : "UNKNOWN");
      }
      fprintf(fp,"\targp = %p",(void*)dargs[2]);
      break;
    case ob_SYS_reboot:
      fprintf(fp,"reboot(int howto)\n"
	      "\thowto = %d", dargs[0]);
      break;
    case SYS_revoke:
      fprintf(fp,"revoke");
      break;
    case SYS_symlink:
      fprintf(fp,"symlink(const char *name1, const char *name2)\n");
      fprintf(fp,"\tname1 = %s\n"
	      "\tname2 = %s", (char*)dargs[0], (char*)dargs[1]);
      break;
    case SYS_readlink:
      fprintf(fp,"readlink(const char *path, char *buf, int bufsiz)\n");
      fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      fprintf(fp,"\tbuf = %p\n",(void*)dargs[1]);
      fprintf(fp,"\tbufsiz = %d",dargs[2]);
      break;
    case SYS_execve:
      fprintf(fp,"execve(const char *path, char *const argv[], "
	      "char *const envp[])");
      if (dargs[0]) fprintf(fp,"\n\tpath = %s",(char*)dargs[0]);
      if (dargs[1])
	{
	  int i = 0;
	  char **argv = (char**)dargs[1];
	  fprintf(fp,"\n\targv = %p",(void*)dargs[1]);
	  while (argv[i])
	    fprintf(fp,"\n\t\t%s",argv[i++]);
	}
      if (dargs[2])
	{
	  int i = 0;
	  char **envp = (char**)dargs[2];
	  fprintf(fp,"\n\tenvp = %p",(void*)dargs[2]);
	  while (envp[i])
	    fprintf(fp,"\n\t\t%s",envp[i++]);
	}
      break;
    case SYS_umask:
      fprintf(fp,"umask(mode_t numask)\n"
	      "\tnumask = 0%o", (u_int)dargs[0]);
      break;
    case SYS_chroot:
      fprintf(fp,"chroot(const char *dirname)\n");
      fprintf(fp,"\tdirname = %s", (char*)dargs[0]);
      break;
    case SYS_compat_43_sys_getkerninfo:
#define KINFO_PROC              (0<<8)
#define KINFO_RT                (1<<8)
#define KINFO_VNODE             (2<<8)
#define KINFO_FILE              (3<<8)
#define KINFO_METER             (4<<8)
#define KINFO_LOADAVG           (5<<8)
#define KINFO_CLOCKRATE         (6<<8)
#define KINFO_BSDI_SYSINFO      (101<<8)
      fprintf(fp,"compat_43_sys_getkerninfo(int op, char *where, int *size,\n"
	      "\tint arg)\n"
	      "\top = 0x%x\n"
	      "\twhere = %p\n"
	      "\tsize = %p\n"
	      "\targ = %d",dargs[0],(void*)dargs[1],(void*)dargs[2],dargs[3]);
      if (dargs[0] & 0xff00 == KINFO_PROC) fprintf(fp,"\n\t\tKINFO_PROC");
      if (dargs[0] & 0xff00 == KINFO_RT) fprintf(fp,"\n\t\tKINFO_RT");
      if (dargs[0] & 0xff00 == KINFO_VNODE) fprintf(fp,"\n\t\tKINFO_VNODE");
      if (dargs[0] & 0xff00 == KINFO_FILE) fprintf(fp,"\n\t\tKINFO_FILE");
      if (dargs[0] & 0xff00 == KINFO_METER) fprintf(fp,"\n\t\tKINFO_METER");
      if (dargs[0] & 0xff00 == KINFO_LOADAVG)
	fprintf(fp,"\n\t\tKINFO_LOADAVG");
      if (dargs[0] & 0xff00 == KINFO_CLOCKRATE)
	fprintf(fp,"\n\t\tKINFO_CLOCKRATE");
      if (dargs[0] & 0xff00 == KINFO_BSDI_SYSINFO) 
	fprintf(fp,"\n\t\tKINFO_BSDI_SYSINFO");
      break;
    case SYS_ogetpagesize:
      fprintf(fp,"ogetpagesize()");
      break;
    case SYS_msync:
      fprintf(fp,"msync(caddr_t addr, size_t len)\n"
	      "\taddr = %p\n"
	      "\tlen = %d", (void*)dargs[0], dargs[1]);
      break;
    case SYS_vfork:
      fprintf(fp,"vfork()");
      break;
    case SYS_sbrk:
      fprintf(fp,"sbrk(int incr)\n"
	      "\tincr = %d",dargs[0]);
      break;
    case SYS_sstk:
      fprintf(fp,"sstk");
      break;
    case SYS_vadvise:
      fprintf(fp,"vadvise");
      break;
    case SYS_munmap:
      fprintf(fp,"munmap(caddr_t addr, size_t len)\n");
      fprintf(fp,"\taddr = %p\n"
	      "\tlen = %d",(void*)dargs[0],dargs[1]);
      break;
    case SYS_mprotect:
      fprintf(fp,"mprotect(caddr_t addr, size_t len, int prot)\n");
      fprintf(fp,"\taddr = %p\n"
	      "\tlen = %d\n"
	      "\tprot = 0x%x",(void*)dargs[0],dargs[1],dargs[2]);
      if (dargs[2] & PROT_READ) fprintf(fp,"\n\t\tPROT_READ");
      if (dargs[2] & PROT_WRITE) fprintf(fp,"\n\t\tPROT_WRITE");
      if (dargs[2] & PROT_EXEC) fprintf(fp,"\n\t\tPROT_EXEC");
      if (dargs[2] == PROT_NONE) fprintf(fp,"\n\t\tPROT_NONE");
      break;
    case SYS_madvise:
      fprintf(fp,"madvise");
      break;
    case SYS_mincore:
      fprintf(fp,"mincore");
      break;
    case SYS_getgroups:
      fprintf(fp,"getgroups");
      break;
    case SYS_setgroups:
      fprintf(fp,"setgroups");
      break;
    case SYS_getpgrp:
      fprintf(fp,"getpgrp(void)");
      break;
    case SYS_setpgid:
      fprintf(fp,"setpgid(pid_t pid, pid_t pgrp)");
      break;
    case SYS_setitimer:
      fprintf(fp,"setitimer(int which, const struct itimerval *value,\n"
	      "\tstruct itimerval *ovalue)\n");
      fprintf(fp,"\twhich = %d\n"
	      "\tvalue = %p\n", dargs[0], (void*)dargs[1]);
      if (dargs[1])
	fprintf(fp,"\t\tit_interval.tv_sec = %ld\n"
		"\t\tit_interval.tv_usec = %ld\n"
		"\t\tit_value.tv_sec = %ld\n"
		"\t\tit_value.tv_usec = %ld\n",
		((struct itimerval*)dargs[1])->it_interval.tv_sec,
		((struct itimerval*)dargs[1])->it_interval.tv_usec,
		((struct itimerval*)dargs[1])->it_value.tv_sec,
		((struct itimerval*)dargs[1])->it_value.tv_usec);
      fprintf(fp,"\tovalue = %p", (void*)dargs[2]);
      if (dargs[2])
	fprintf(fp,"\n\t\tit_interval.tv_sec = %ld\n"
		"\t\tit_interval.tv_usec = %ld\n"
		"\t\tit_value.tv_sec = %ld\n"
		"\t\tit_value.tv_usec = %ld",
		((struct itimerval*)dargs[2])->it_interval.tv_sec,
		((struct itimerval*)dargs[2])->it_interval.tv_usec,
		((struct itimerval*)dargs[2])->it_value.tv_sec,
		((struct itimerval*)dargs[2])->it_value.tv_usec);
      break;
    case SYS_swapon:
      fprintf(fp,"swapon");
      break;
    case SYS_getitimer:
      fprintf(fp,"getitimer");
      break;
    case SYS_dup2:
      fprintf(fp,"dup2(int oldd, int newd)"
	      "\n\toldd = %d"
	      "\n\tnewd = %d",dargs[0],dargs[1]);
      break;
    case SYS_fcntl:
      fprintf(fp,"fcntl(int fd, int cmd, int arg)\n");
      fprintf(fp,"\tfd = %d\n"
	      "\tcmd = %d\n"
	      "\t\t%s\n"
	      "\targ = %d",dargs[0],dargs[1],
	      dargs[1] == F_DUPFD ? "F_DUPFD" :
	      dargs[1] == F_GETFD ? "F_GETFD" :
	      dargs[1] == F_SETFD ? "F_SETFD" :
	      dargs[1] == F_GETFL ? "F_GETFL" :
	      dargs[1] == F_SETFL ? "F_SETFL" :
	      dargs[1] == F_GETOWN ? "F_GETOWN" :
	      dargs[1] == F_SETOWN ? "F_SETOWN" :
	      dargs[1] == F_GETLK ? "F_GETLK" :
	      dargs[1] == F_SETLK ? "F_SETLK" :
	      dargs[1] == F_SETLKW ? "F_SETLKW" :
	      "UNKNOWN FLAG",
	      dargs[2]);
      if ((dargs[1] == F_GETLK ||
	   dargs[1] == F_SETLK ||
	   dargs[1] == F_SETLKW) && dargs[2])
	fprintf(fp,"\n\t((struct flock*)arg)->l_start = %qd\n"
		"\t((struct flock*)arg)->l_len = %qd\n"
		"\t((struct flock*)arg)->l_pid = %d\n"
		"\t((struct flock*)arg)->l_type = %hd\n"
		"\t\t%s\n"
		"\t((struct flock*)arg)->l_whence = %hd\n"
		"\t\t%s",
		((struct flock*)dargs[2])->l_start,
		((struct flock*)dargs[2])->l_len,
		((struct flock*)dargs[2])->l_pid,
		((struct flock*)dargs[2])->l_type,
		((struct flock*)dargs[2])->l_type == F_RDLCK ? "F_RDLCK" :
		((struct flock*)dargs[2])->l_type == F_UNLCK ? "F_UNLCK" :
		((struct flock*)dargs[2])->l_type == F_WRLCK ? "F_WRLCK" :
		"UNKNOWN",
		((struct flock*)dargs[2])->l_whence,
		((struct flock*)dargs[2])->l_whence == SEEK_SET ? "SEEK_SET" :
		((struct flock*)dargs[2])->l_whence == SEEK_CUR ? "SEEK_CUR" :
		((struct flock*)dargs[2])->l_whence == SEEK_END ? "SEEK_END" :
		"UNKNOWN");
      break;
    case SYS_select:
      fprintf(fp,"select(int nfds, fd_set *readfds, fd_set *writefds, "
	      "fd_set *exceptfds, \n\tstruct timeval *timeout)\n");
      fprintf(fp,"\tnfds = %d",dargs[0]);
      if (dargs[1])
	{
	  int x;

	  fprintf(fp,"\n\treadfds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[1]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\treadfds = NULL");
      if (dargs[2])
	{
	  int x;

	  fprintf(fp,"\n\twritefds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[2]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\twritefds = NULL");
      if (dargs[3])
	{
	  int x;

	  fprintf(fp,"\n\texceptfds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[3]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\texceptfds = NULL");
      if (dargs[4])
	{
	  fprintf(fp,"\n\ttimeout->tv_sec = %ld\n",
		  ((struct timeval*)dargs[4])->tv_sec);
	  fprintf(fp,"\ttimeout->tv_usec = %ld",
		  ((struct timeval*)dargs[4])->tv_usec);
	}
      else
	  fprintf(fp,"\n\ttimeout = NULL");
      break;
    case SYS_fsync:
      fprintf(fp,"fsync(int fd)\n"
	      "\tfd = %d", (int)dargs[0]);
      break;
    case SYS_setpriority:
      fprintf(fp,"setpriority(int which, int who, int prio)\n"
	      "\twhich = %d\n"
	      "\twho = %d\n"
	      "\tprio = %d", (int)dargs[0], (int)dargs[1], (int)dargs[2]);
      break;
    case SYS_socket:
      fprintf(fp,"socket(int domain, int type, int protocol)\n");
      fprintf(fp,"\tdomain = %d\n",dargs[0]);
      fprintf(fp,"\ttype = %d\n",dargs[1]);
      fprintf(fp,"\tprotocol = %d",dargs[2]);
      break;
    case SYS_connect:
      fprintf(fp,"connect(int s, const struct sockaddr *name, int namelen)\n");
      fprintf(fp,"\ts = %d\n",dargs[0]);
      fprintf(fp,"\tname = %p\n",(void*)dargs[1]);
      if (dargs[1])
	{
	  char *s;

	  fprintf(fp,"\t\tsa_len = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_len));
	  fprintf(fp,"\t\tsa_family = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_family));
	  fprintf(fp,"\t\tsin_port = %d\n",
		  (int)(((struct sockaddr_in*)dargs[1])->sin_port));
	  s = inet_ntoa(((struct sockaddr_in*)dargs[1])->sin_addr);
	  fprintf(fp,"\t\tsin_addr = %s\n", s ? s : "0?");
	}
      fprintf(fp,"\tnamelen = %d",dargs[2]);
      break;
    case SYS_getpriority:
      fprintf(fp,"getpriority(int which, int who)\n"
	      "\twhich = %d\n"
	      "\twho = %d", (int)dargs[0], (int)dargs[1]);
      break;
    case SYS_sigreturn:
      fprintf(fp,"sigreturn");
      break;
    case SYS_bind:
      fprintf(fp,"bind");
      break;
    case SYS_setsockopt:
      fprintf(fp,"setsockopt(int s, int level, int optname, const void "
	      "*optval, int optlen)\n");
      fprintf(fp,"\ts = %d\n",dargs[0]);
      fprintf(fp,"\tlevel = %d\n",dargs[1]);
      fprintf(fp,"\toptname = %d\n",dargs[2]);
      fprintf(fp,"\toptval = %p\n",(void*)dargs[3]);
      fprintf(fp,"\toptlen = %d",dargs[4]);
      break;
    case SYS_listen:
      fprintf(fp,"listen");
      break;
    case SYS_sigsuspend:
      fprintf(fp,"sigsuspend");
      break;
    case SYS_vtrace:
      fprintf(fp,"vtrace");
      break;
    case SYS_gettimeofday:
      fprintf(fp,"gettimeofday(struct timeval *tp, struct timezone *tzp)\n");
      fprintf(fp,"\ttp = %p\n",(void*)dargs[0]);
      fprintf(fp,"\ttzp = %p",(void*)dargs[1]);
      break;
    case SYS_getrusage:
      fprintf(fp,"getrusage");
      break;
    case SYS_getsockopt:
      fprintf(fp,"getsockopt");
      break;
    case SYS_readv:
      fprintf(fp,"readv(int d, const struct iovec *iov, int iovcnt)\n");
      fprintf(fp,"\td = %d\n", dargs[0]);
      fprintf(fp,"\tiov = %p\n", (void*)dargs[1]);
      fprintf(fp,"\tiovcnt = %d", dargs[2]);
      if (dargs[1]) fprintf(fp,"\n\t\t(total) = %d",
			    calc_vtotal((void*)dargs[1], dargs[2]));
      break;
    case SYS_writev:
      fprintf(fp,"writev(int d, const struct iovec *iov, int iovcnt)\n");
      fprintf(fp,"\td = %d\n", dargs[0]);
      fprintf(fp,"\tiov = %p\n", (void*)dargs[1]);
      fprintf(fp,"\tiovcnt = %d", dargs[2]);
      if (dargs[1]) fprintf(fp,"\n\t\t(total) = %d",
			    calc_vtotal((void*)dargs[1], dargs[2]));
      if (dargs[1]) print_debug_datav(fp, (void*)dargs[1], dargs[2], -1);
      break;
    case SYS_settimeofday:
      fprintf(fp,"settimeofday");
      break;
    case SYS_fchown:
      fprintf(fp,"fchown");
      break;
    case SYS_fchmod:
      fprintf(fp,"fchmod");
      break;
    case SYS_rename:
      fprintf(fp,"rename");
      break;
    case SYS_flock:
      fprintf(fp,"flock");
      break;
    case SYS_mkfifo:
      fprintf(fp,"mkfifo");
      break;
    case SYS_sendto:
      fprintf(fp,"sendto(int s, const void *msg, size_t len, int flags,\n"
	      "\tconst struct sockaddr *to, int tolen)\n");
      fprintf(fp,"\ts = %d\n", dargs[0]);
      fprintf(fp,"\tmsg = %p\n", (void*)dargs[1]);
      fprintf(fp,"\tlen = %d\n", dargs[2]);
      fprintf(fp,"\tflags = %x\n", dargs[3]);
      if (dargs[3] & MSG_OOB) fprintf(fp,"\t\tMSG_OOB\n");
      if (dargs[3] & MSG_DONTROUTE) fprintf(fp,"\t\tMSG_DONTROUTE\n");
      fprintf(fp,"\tto = %p", (void*)dargs[4]);
      if (dargs[4])
	{
	  char *s;

	  fprintf(fp,"\n\t\tsa_len = %d\n",
		  (int)(((struct sockaddr*)dargs[4])->sa_len));
	  fprintf(fp,"\t\tsa_family = %d\n",
		  (int)(((struct sockaddr*)dargs[4])->sa_family));
	  fprintf(fp,"\t\tsin_port = %d\n",
		  (int)(((struct sockaddr_in*)dargs[4])->sin_port));
	  s = inet_ntoa(((struct sockaddr_in*)dargs[4])->sin_addr);
	  fprintf(fp,"\t\tsin_addr = %s", s ? s : "0?");
	}
      fprintf(fp,"\n\ttolen = %d", dargs[5]);
      if (dargs[1]) print_debug_data(fp, (char*)dargs[1], dargs[2]);
      break;
    case SYS_shutdown:
      fprintf(fp,"shutdown(int s, int how)\n");
      fprintf(fp,"\ts = %d\n", dargs[0]);
      fprintf(fp,"\thow = %d", dargs[1]);
      break;
    case SYS_socketpair:
      fprintf(fp,"socketpair");
      break;
    case SYS_mkdir:
      fprintf(fp,"mkdir(const char *path, mode_t mode)\n");
      fprintf(fp,"\tpath = %s\n"
	      "\tmode = 0%o", (char*)dargs[0], dargs[1]);
      break;
    case SYS_rmdir:
      fprintf(fp,"rmdir");
      break;
    case SYS_utimes:
      fprintf(fp,"utimes");
      break;
    case SYS_adjtime:
      fprintf(fp,"adjtime");
      break;
    case SYS_setsid:
      fprintf(fp,"setsid");
      break;
    case SYS_quotactl:
      fprintf(fp,"quotactl");
      break;
    case SYS_nfssvc:
      fprintf(fp,"nfssvc");
      break;
    case SYS_statfs:
      fprintf(fp,"statfs(const char *path, struct statfs *buf)\n");
      if (dargs[0])
	fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      else
	fprintf(fp,"\tpath = NULL\n");
      fprintf(fp,"\tbuf = %p",(void*)dargs[1]);
      break;
    case SYS_fstatfs:
      fprintf(fp,"fstatfs(int fd, struct statfs *buf)\n"
	      "\tfd = %d\n"
	      "\tbuf = %p",dargs[0],(void*)dargs[1]);
      break;
    case SYS_getfh:
      fprintf(fp,"getfh");
      break;
    case SYS_sysarch:
      fprintf(fp,"sysarch(int number, char *args)\n"
	      "\tnumber = %d\n"
	      "\targs = %p", (int)dargs[0], (void*)dargs[1]);
      if (dargs[1])
	switch (dargs[0]) {
	case I386_GET_LDT:
	  fprintf(fp,"\n\tI386_GET_LDT\n"
		  "\targs->start = %d\n"
		  "\targs->desc = %p\n"
		  "\targs->num = %d",
		  ((struct i386_get_ldt_args*)dargs[1])->start,
		  ((struct i386_get_ldt_args*)dargs[1])->desc,
		  ((struct i386_get_ldt_args*)dargs[1])->num);
	  break;
	case I386_SET_LDT:
	  fprintf(fp,"\n\tI386_SET_LDT\n"
		  "\targs->start = %d\n"
		  "\targs->desc = %p\n"
		  "\targs->num = %d",
		  ((struct i386_set_ldt_args*)dargs[1])->start,
		  ((struct i386_set_ldt_args*)dargs[1])->desc,
		  ((struct i386_set_ldt_args*)dargs[1])->num);
	  break;
	case I386_IOPL:
	  fprintf(fp,"\n\tI386_IOPL\n"
		  "\targs->iopl = %d",
		  ((struct i386_iopl_args*)dargs[1])->iopl);
	  break;
	case I386_GET_IOPERM:
	  fprintf(fp,"\n\tI386_GET_IOPERM\n"
		  "\targs->iomap = %p",
		  ((struct i386_get_ioperm_args*)dargs[1])->iomap);
	  break;
	case I386_SET_IOPERM:
	  fprintf(fp,"\n\tI386_SET_IOPERM\n"
		  "\targs->iomap = %p",
		  ((struct i386_set_ioperm_args*)dargs[1])->iomap);
	  break;
	case I386_VM86:
	  fprintf(fp,"\n\tI386_VM86");
	  break;
	}
      break;
    case SYS_ntp_gettime:
      fprintf(fp,"ntp_gettime");
      break;
    case SYS_ntp_adjtime:
      fprintf(fp,"ntp_adjtime");
      break;
    case SYS_setgid:
      fprintf(fp,"setgid(gid_t gid)\n"
	      "\tgid = %d",dargs[0]);
      break;
    case SYS_setegid:
      fprintf(fp,"setegid(gid_t egid)\n"
	      "\tegid = %d",dargs[0]);
      break;
    case SYS_seteuid:
      fprintf(fp,"seteuid(uid_t euid)\n"
	      "\teuid = %d",dargs[0]);
      break;
    case SYS_lfs_bmapv:
      fprintf(fp,"lfs_bmapv");
      break;
    case SYS_lfs_markv:
      fprintf(fp,"lfs_markv");
      break;
    case SYS_lfs_segclean:
      fprintf(fp,"lfs_segclean");
      break;
    case SYS_lfs_segwait:
      fprintf(fp,"lfs_segwait");
      break;
    case SYS_stat:
      fprintf(fp,"stat(const char *path, struct stat *sb)\n");
      if (dargs[0])
	fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      else
	fprintf(fp,"\tpath = NULL\n");
      fprintf(fp,"\tsb = %p",(void*)dargs[1]);
      break;
    case SYS_fstat:
      fprintf(fp,"fstat(int fd, struct stat *sb)\n");
      fprintf(fp,"\tfd = %d\n",dargs[0]);
      fprintf(fp,"\tsb = %p",(void*)dargs[1]);
      break;
    case SYS_lstat:
      fprintf(fp,"lstat(const char *path, struct stat *sb)\n");
      if (dargs[0])
	fprintf(fp,"\tpath = %s\n",(char*)dargs[0]);
      else
	fprintf(fp,"\tpath = NULL\n");
      fprintf(fp,"\tsb = %p",(void*)dargs[1]);
      break;
    case SYS_pathconf:
      fprintf(fp,"pathconf");
      break;
    case SYS_fpathconf:
      fprintf(fp,"fpathconf");
      break;
    case SYS_getrlimit:
      fprintf(fp,"getrlimit(int resource, struct rlimit *rlp)\n"
	      "\tresource = %d\n", (int)dargs[0]);
      switch ((int)dargs[0]) {
      case RLIMIT_CORE: fprintf(fp,"\t\tRLIMIT_CORE\n"); break;
      case RLIMIT_CPU: fprintf(fp,"\t\tRLIMIT_CPU\n"); break;
      case RLIMIT_DATA: fprintf(fp,"\t\tRLIMIT_DATA\n"); break;
      case RLIMIT_FSIZE: fprintf(fp,"\t\tRLIMIT_FSIZE\n"); break;
      case RLIMIT_MEMLOCK: fprintf(fp,"\t\tRLIMIT_MEMLOCK\n"); break;
      case RLIMIT_NOFILE: fprintf(fp,"\t\tRLIMIT_NOFILE\n"); break;
      case RLIMIT_NPROC: fprintf(fp,"\t\tRLIMIT_NPROC\n"); break;
      case RLIMIT_RSS: fprintf(fp,"\t\tRLIMIT_RSS\n"); break;
      case RLIMIT_STACK: fprintf(fp,"\t\tRLIMIT_STACK\n"); break;
      default: fprintf(fp,"\t\tUNKNOWN\n"); break;
      }
      fprintf(fp,"\trlp = %p", (void*)dargs[1]);
      break;
    case SYS_setrlimit:
      fprintf(fp,"setrlimit(int resource, const struct rlimit *rlp)\n"
	      "\tresource = %d\n", (int)dargs[0]);
      switch ((int)dargs[0]) {
      case RLIMIT_CORE: fprintf(fp,"\t\tRLIMIT_CORE\n"); break;
      case RLIMIT_CPU: fprintf(fp,"\t\tRLIMIT_CPU\n"); break;
      case RLIMIT_DATA: fprintf(fp,"\t\tRLIMIT_DATA\n"); break;
      case RLIMIT_FSIZE: fprintf(fp,"\t\tRLIMIT_FSIZE\n"); break;
      case RLIMIT_MEMLOCK: fprintf(fp,"\t\tRLIMIT_MEMLOCK\n"); break;
      case RLIMIT_NOFILE: fprintf(fp,"\t\tRLIMIT_NOFILE\n"); break;
      case RLIMIT_NPROC: fprintf(fp,"\t\tRLIMIT_NPROC\n"); break;
      case RLIMIT_RSS: fprintf(fp,"\t\tRLIMIT_RSS\n"); break;
      case RLIMIT_STACK: fprintf(fp,"\t\tRLIMIT_STACK\n"); break;
      default: fprintf(fp,"\t\tUNKNOWN\n"); break;
      }
      fprintf(fp,"\trlp = %p", (void*)dargs[1]);
      if (dargs[1]) {
	if (((struct rlimit *)dargs[1])->rlim_cur == RLIM_INFINITY)
	  fprintf(fp,"\n\trlp->rlim_cur = RLIM_INFINITY");
	else
	  fprintf(fp,"\n\trlp->rlim_cur = %qd",
		  ((struct rlimit *)dargs[1])->rlim_cur);
	if (((struct rlimit *)dargs[1])->rlim_max == RLIM_INFINITY)
	  fprintf(fp,"\n\trlp->rlim_max = RLIM_INFINITY");
	else
	  fprintf(fp,"\n\trlp->rlim_max = %qd",
		  ((struct rlimit *)dargs[1])->rlim_max);
      }
      break;
    case SYS_getdirentries:
      fprintf(fp,"getdirentries");
      break;
    case SYS_mmap:
      fprintf(fp,"mmap(caddr_t addr, size_t len, int prot, int flags, int fd, "
	      "off_t offset)\n");
      fprintf(fp,"\taddr = %p\n",(void*)dargs[0]);
      fprintf(fp,"\tlen = %d\n",dargs[1]);
      fprintf(fp,"\tprot = %d",dargs[2]);
      if (dargs[2] & PROT_READ) fprintf(fp,"\n\t\tPROT_READ");
      if (dargs[2] & PROT_WRITE) fprintf(fp,"\n\t\tPROT_WRITE");
      if (dargs[2] & PROT_EXEC) fprintf(fp,"\n\t\tPROT_EXEC");
      if (dargs[2] == PROT_NONE) fprintf(fp,"\n\t\tPROT_NONE");
      fprintf(fp,"\n\tflags = %d",dargs[3]);
      if (dargs[3] & MAP_SHARED) fprintf(fp,"\n\t\tMAP_SHARED");
      if (dargs[3] & MAP_PRIVATE) fprintf(fp,"\n\t\tMAP_PRIVATE");
      if (dargs[3] & MAP_COPY) fprintf(fp,"\n\t\tMAP_COPY");
      if (dargs[3] & MAP_FIXED) fprintf(fp,"\n\t\tMAP_FIXED");
      if (dargs[3] & MAP_RENAME) fprintf(fp,"\n\t\tMAP_RENAME");
      if (dargs[3] & MAP_NORESERVE) fprintf(fp,"\n\t\tMAP_NORESERVE");
      if (dargs[3] & MAP_INHERIT) fprintf(fp,"\n\t\tMAP_INHERIT");
      if (dargs[3] & MAP_NOEXTEND) fprintf(fp,"\n\t\tMAP_NOEXTEND");
      if (dargs[3] & MAP_HASSEMAPHORE) fprintf(fp,"\n\t\tMAP_HASSEMAPHORE");
      if (dargs[3] & MAP_ANON) fprintf(fp,"\n\t\tMAP_ANON");
      if (dargs[3] == MAP_FILE) fprintf(fp,"\n\t\tMAP_FILE");
      fprintf(fp,"\n\tfd = %d\n",dargs[4]); /* dargs[5] = padding */
      fprintf(fp,"\toffset = %qd",(quad_t)dargs[6]);
      break;
    case SYS___syscall:
      fprintf(fp,"__syscall (indirect syscall = %d)",dargs[0]);
      break;
    case SYS_lseek:
      fprintf(fp,"lseek(int fildes, off_t offset, int whence)\n");
      fprintf(fp,"\tfildes = %d\n"
	      "\toffset = %qd\n"
	      "\twhence = 0x%x",
	      dargs[0],*(quad_t*)&dargs[2],dargs[4]); /* dargs[1] = padding */
      if (dargs[4] == SEEK_SET) fprintf(fp,"\n\t\tSEEK_SET");
      if (dargs[4] == SEEK_CUR) fprintf(fp,"\n\t\tSEEK_CUR");
      if (dargs[4] == SEEK_END) fprintf(fp,"\n\t\tSEEK_END");
      break;
    case SYS_truncate:
      fprintf(fp,"truncate(const char *path, off_t length)\n");
      fprintf(fp,"\tpath = %s\n"
	      "\tlength = %qd", (char*)dargs[0], *(quad_t*)&dargs[1]);
      break;
    case SYS_ftruncate:
      fprintf(fp,"ftruncate(int fd, off_t length)\n");
      fprintf(fp,"\fd = %d\n"
	      "\tlength = %qd", dargs[0], *(quad_t*)&dargs[1]);
      break;
    case SYS___sysctl:
      fprintf(fp,"__sysctl(int *name, u_int namelen, void *oldp, size_t "
	      "*oldlenp, \n\tvoid *newp, size_t newlen)\n");
      fprintf(fp,"\tname = %p\n",(void*)dargs[0]);
      fprintf(fp,"\tnamelen = %u\n",dargs[1]);
      fprintf(fp,"\toldp = %p\n",(void*)dargs[2]);
      if (dargs[2]) fprintf(fp,"\t*(u_int*)oldp = 0x%x\n", *(u_int*)dargs[2]);
      fprintf(fp,"\toldlenp = %p\n",(size_t*)dargs[3]);
      if (dargs[3]) fprintf(fp,"\t*(u_int*)oldlenp = 0x%x\n", *(u_int*)dargs[3]);
      fprintf(fp,"\tnewp = %p\n",(void*)dargs[4]);
      if (dargs[4]) fprintf(fp,"\t*(u_int*)newp = 0x%x\n", *(u_int*)dargs[4]);
      fprintf(fp,"\tnewlen = %u",dargs[5]);
      {
	int sysctli;

	for (sysctli=0; sysctli < dargs[1]; sysctli++) {
	  if (sysctli > 7) break;
	  fprintf(fp,"\n\tname[%u] = %u", sysctli, ((int*)dargs[0])[sysctli]);
	}
      }
      if (dargs[1] > 0) {
	fprintf(fp,"\n\tname[0] = ");
	switch (((int*)dargs[0])[0]) {
	case CTL_UNSPEC: fprintf(fp,"CTL_UNSPEC");
	  break;
	case CTL_KERN: fprintf(fp,"CTL_KERN");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case KERN_OSTYPE: fprintf(fp,"KERN_OSTYPE"); break;
	    case KERN_OSRELEASE: fprintf(fp,"KERN_OSRELEASE"); break;
	    case KERN_OSREV: fprintf(fp,"KERN_OSREV"); break;
	    case KERN_VERSION: fprintf(fp,"KERN_VERSION"); break;
	    case KERN_MAXVNODES: fprintf(fp,"KERN_MAXVNODES"); break;
	    case KERN_MAXPROC: fprintf(fp,"KERN_MAXPROC"); break;
	    case KERN_MAXFILES: fprintf(fp,"KERN_MAXFILES"); break;
	    case KERN_ARGMAX: fprintf(fp,"KERN_ARGMAX"); break;
	    case KERN_SECURELVL: fprintf(fp,"KERN_SECURELVL"); break;
	    case KERN_HOSTNAME: fprintf(fp,"KERN_HOSTNAME"); break;
	    case KERN_HOSTID: fprintf(fp,"KERN_HOSTID"); break;
	    case KERN_CLOCKRATE: fprintf(fp,"KERN_CLOCKRATE"); break;
	    case KERN_VNODE: fprintf(fp,"KERN_VNODE"); break;
	    case KERN_PROC:
	      fprintf(fp,"KERN_PROC");
	      if (dargs[1] > 2) {
		fprintf(fp,"\n\tname[2] = ");
		switch (((int*)dargs[0])[2]) {
		case KERN_PROC_ALL: fprintf(fp,"KERN_PROC_ALL"); break;
		case KERN_PROC_PID: fprintf(fp,"KERN_PROC_PID"); break;
		case KERN_PROC_PGRP: fprintf(fp,"KERN_PROC_PGRP"); break;
		case KERN_PROC_SESSION: fprintf(fp,"KERN_PROC_SESSION"); break;
		case KERN_PROC_TTY: fprintf(fp,"KERN_PROC_TTY"); break;
		case KERN_PROC_UID: fprintf(fp,"KERN_PROC_UID"); break;
		case KERN_PROC_RUID: fprintf(fp,"KERN_PROC_RUID"); break;
		default: fprintf(fp,"UNKNOWN"); break;
		}
	      }
	      break;
	    case KERN_FILE: fprintf(fp,"KERN_FILE"); break;
	    case KERN_PROF: fprintf(fp,"KERN_PROF"); break;
	    case KERN_POSIX1: fprintf(fp,"KERN_POSIX1"); break;
	    case KERN_NGROUPS: fprintf(fp,"KERN_NGROUPS"); break;
	    case KERN_JOB_CONTROL: fprintf(fp,"KERN_JOB_CONTROL"); break;
	    case KERN_SAVED_IDS: fprintf(fp,"KERN_SAVED_IDS"); break;
	    case KERN_BOOTTIME: fprintf(fp,"KERN_BOOTTIME"); break;
	    case KERN_DOMAINNAME: fprintf(fp,"KERN_DOMAINNAME"); break;
	    case KERN_MAXPARTITIONS: fprintf(fp,"KERN_MAXPARTITIONS"); break;
	    case KERN_RAWPARTITION: fprintf(fp,"KERN_RAWPARTITION"); break;
	    case KERN_NTPTIME: fprintf(fp,"KERN_NTPTIME"); break;
	    case KERN_TIMEX: fprintf(fp,"KERN_TIMEX"); break;
	    case KERN_OSVERSION: fprintf(fp,"KERN_OSVERSION"); break;
	    case KERN_SOMAXCONN: fprintf(fp,"KERN_SOMAXCONN"); break;
	    case KERN_SOMINCONN: fprintf(fp,"KERN_SOMINCONN"); break;
	    case KERN_USERMOUNT: fprintf(fp,"KERN_USERMOUNT"); break;
	    case KERN_RND: fprintf(fp,"KERN_RND"); break;
	    case KERN_NOSUIDCOREDUMP: fprintf(fp,"KERN_NOSUIDCOREDUMP"); break;
	    case KERN_FSYNC: fprintf(fp,"KERN_FSYNC"); break;
	    case KERN_SYSVMSG: fprintf(fp,"KERN_SYSVMSG"); break;
	    case KERN_SYSVSEM: fprintf(fp,"KERN_SYSVSEM"); break;
	    case KERN_SYSVSHM: fprintf(fp,"KERN_SYSVSHM"); break;
	    case KERN_MAXID: fprintf(fp,"KERN_MAXID"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_VM: fprintf(fp,"CTL_VM");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case VM_LOADAVG: fprintf(fp,"VM_LOADAVG"); break;
	    case VM_METER: fprintf(fp,"VM_METER"); break;
	    case VM_PSSTRINGS: fprintf(fp,"VM_PSSTRINGS"); break;
#ifdef VM_UVMEXP
	    case VM_UVMEXP: fprintf(fp,"VM_UVMEXP"); break;
#endif
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_FS: fprintf(fp,"CTL_FS");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case FS_POSIX:
	      fprintf(fp,"FS_POSIX");
	      if (dargs[1] > 2) {
		fprintf(fp,"\n\tname[2] = ");
		switch (((int*)dargs[0])[2]) {
		case FS_POSIX_SETUID: fprintf(fp,"FS_POSIX_SETUID"); break;
		default: fprintf(fp,"UNKNOWN"); break;
		}
	      }
	      break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_NET: fprintf(fp,"CTL_NET");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case PF_ROUTE: fprintf(fp,"PF_ROUTE"); break;
	    case PF_INET: fprintf(fp,"PF_INET"); break;
	    case PF_ENCAP: fprintf(fp,"PF_ENCAP"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_DEBUG: fprintf(fp,"CTL_DEBUG");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case CTL_DEBUG_NAME: fprintf(fp,"CTL_DEBUG_NAME"); break;
	    case CTL_DEBUG_VALUE: fprintf(fp,"CTL_DEBUG_VALUE"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	  break;
	case CTL_HW: fprintf(fp,"CTL_HW");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case HW_MACHINE: fprintf(fp,"HW_MACHINE"); break;
	    case HW_MODEL: fprintf(fp,"HW_MODEL"); break;
	    case HW_NCPU: fprintf(fp,"HW_NCPU"); break;
	    case HW_BYTEORDER: fprintf(fp,"HW_BYTEORDER"); break;
	    case HW_PHYSMEM: fprintf(fp,"HW_PHYSMEM"); break;
	    case HW_USERMEM: fprintf(fp,"HW_USERMEM"); break;
	    case HW_PAGESIZE: fprintf(fp,"HW_PAGESIZE"); break;
	    case HW_DISKNAMES: fprintf(fp,"HW_DISKNAMES"); break;
	    case HW_DISKSTATS: fprintf(fp,"HW_DISKSTATS"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_MACHDEP: fprintf(fp,"CTL_MACHDEP");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case CPU_CONSDEV: fprintf(fp,"CPU_CONSDEV"); break;
	    case CPU_BIOS: fprintf(fp,"CPU_BIOS"); break;
	    case CPU_BLK2CHR: fprintf(fp,"CPU_BLK2CHR"); break;
	    case CPU_CHR2BLK: fprintf(fp,"CPU_CHR2BLK"); break;
	    case CPU_ALLOWAPERTURE: fprintf(fp,"CPU_ALLOWAPERTURE"); break;
	    case CPU_CPUVENDOR: fprintf(fp,"CPU_CPUVENDOR"); break;
	    case CPU_CPUID: fprintf(fp,"CPU_CPUID"); break;
	    case CPU_CPUFEATURE: fprintf(fp,"CPU_CPUFEATURE"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_USER: fprintf(fp,"CTL_USER");
	  if (dargs[1] > 1) {
	    fprintf(fp,"\n\tname[1] = ");
	    switch (((int*)dargs[0])[1]) {
	    case USER_CS_PATH: fprintf(fp,"USER_CS_PATH"); break;
	    case USER_BC_BASE_MAX: fprintf(fp,"USER_BC_BASE_MAX"); break;
	    case USER_BC_DIM_MAX: fprintf(fp,"USER_BC_DIM_MAX"); break;
	    case USER_BC_SCALE_MAX: fprintf(fp,"USER_BC_SCALE_MAX"); break;
	    case USER_BC_STRING_MAX: fprintf(fp,"USER_BC_STRING_MAX"); break;
	    case USER_COLL_WEIGHTS_MAX: fprintf(fp,"USER_COLL_WEIGHTS_MAX"); break;
	    case USER_EXPR_NEST_MAX: fprintf(fp,"USER_EXPR_NEST_MAX"); break;
	    case USER_LINE_MAX: fprintf(fp,"USER_LINE_MAX"); break;
	    case USER_RE_DUP_MAX: fprintf(fp,"USER_RE_DUP_MAX"); break;
	    case USER_POSIX2_VERSION: fprintf(fp,"USER_POSIX2_VERSION"); break;
	    case USER_POSIX2_C_BIND: fprintf(fp,"USER_POSIX2_C_BIND"); break;
	    case USER_POSIX2_C_DEV: fprintf(fp,"USER_POSIX2_C_DEV"); break;
	    case USER_POSIX2_CHAR_TERM: fprintf(fp,"USER_POSIX2_CHAR_TERM"); break;
	    case USER_POSIX2_FORT_DEV: fprintf(fp,"USER_POSIX2_FORT_DEV"); break;
	    case USER_POSIX2_FORT_RUN: fprintf(fp,"USER_POSIX2_FORT_RUN"); break;
	    case USER_POSIX2_LOCALEDEF: fprintf(fp,"USER_POSIX2_LOCALEDEF"); break;
	    case USER_POSIX2_SW_DEV: fprintf(fp,"USER_POSIX2_SW_DEV"); break;
	    case USER_POSIX2_UPE: fprintf(fp,"USER_POSIX2_UPE"); break;
	    case USER_STREAM_MAX: fprintf(fp,"USER_STREAM_MAX"); break;
	    case USER_TZNAME_MAX: fprintf(fp,"USER_TZNAME_MAX"); break;
	    default: fprintf(fp,"UNKNOWN"); break;
	    }
	  }
	  break;
	case CTL_DDB: fprintf(fp,"CTL_DDB");
	  break;
	case CTL_VFS: fprintf(fp,"CTL_VFS");
	  break;
	default: fprintf(fp,"UNKNOWN"); break;
	}
      }
      break;
    case SYS_mlock:
      fprintf(fp,"mlock");
      break;
    case SYS_munlock:
      fprintf(fp,"munlock");
      break;
    case SYS_undelete:
      fprintf(fp,"undelete");
      break;
    case SYS_futimes:
      fprintf(fp,"futimes");
      break;
    case SYS___semctl:
      fprintf(fp,"__semctl");
      break;
    case SYS_semget:
      fprintf(fp,"semget");
      break;
    case SYS_semop:
      fprintf(fp,"semop");
      break;
    case SYS_semconfig:
      fprintf(fp,"semconfig");
      break;
    case SYS_msgctl:
      fprintf(fp,"msgctl");
      break;
    case SYS_msgget:
      fprintf(fp,"msgget");
      break;
    case SYS_msgsnd:
      fprintf(fp,"msgsnd");
      break;
    case SYS_msgrcv:
      fprintf(fp,"msgrcv");
      break;
    case SYS_shmat:
      fprintf(fp,"shmat");
      break;
    case SYS_shmctl:
      fprintf(fp,"shmctl");
      break;
    case SYS_shmdt:
      fprintf(fp,"shmdt");
      break;
    case SYS_shmget:
      fprintf(fp,"shmget");
      break;
    case SYS_nanosleep:
      fprintf(fp,"nanosleep(const struct timespec *rqtp, "
	      "struct timespec *rmtp)\n");
      fprintf(fp, "\trqtp = %p\n"
	      "\trmtp = %p", (void*)dargs[0], (void*)dargs[1]);
      if (dargs[0])
	{
	  struct timespec *t = (void*)dargs[0];
	  fprintf(fp,"\n\trqtp->tv_sec = %ld\n", (long)t->tv_sec);
	  fprintf(fp,"\trqtp->tv_nsec = %ld", t->tv_nsec);
	}
      if (dargs[1])
	{
	  struct timespec *t = (void*)dargs[1];
	  fprintf(fp,"\n\trmtp->tv_sec = %ld\n", (long)t->tv_sec);
	  fprintf(fp,"\trmtp->tv_nsec = %ld", t->tv_nsec);
	}
      break;
    case SYS_minherit:
      fprintf(fp,"minherit");
      break;
    case SYS_rfork:
      fprintf(fp,"rfork");
      break;
    case SYS_poll:
      fprintf(fp,"poll(struct pollfd *fds, int nfds, int timeout)\n");
      fprintf(fp,"\tfds = %p", (void*)dargs[0]);
      {
	int i;
	for (i=0; i < dargs[1]; i++)
	  fprintf(fp,"\n\tfds[%d].fd = %d\n"
		  "\t      .events = %x", i, ((struct pollfd*)dargs[0])->fd,
		  ((struct pollfd*)dargs[0])->events);
      }
      fprintf(fp,"\n\tnfds = %d"
	      "\n\ttimeout = %d", dargs[1], dargs[2]);
      break;
    case SYS_issetugid:
      fprintf(fp,"issetugid(void)");
      break;
    case SYS_MAXSYSCALL:

    default:
      fprintf(fp,"UNKNOWN SYSCALL: %d",r_s.eax);
    }
  fprintf(fp,"\n");
  fflush(fp);
}

void detailed_post(FILE *fp, struct reg_storage *r)
{
  if ((r_s.eax != SYS_fork && r_s.eax != SYS_vfork) || r->eax)
    {
      fprintf(fp,"%5d: ",getpid());
      if (InIndirect == 1)
	{
	  fprintf(fp,"End of indirect syscall\n");
	  goto post_switch;
	}
    }
  switch(r_s.eax)
    {      
    case SYS_syscall:
      fprintf(fp,"syscall");
      break;
    case SYS_exit:
      fprintf(fp,"exit => THIS SHOULD NEVER HAPPEN");
      break;
    case SYS_fork:
      if (!r->eax)
	{
	  forked_child = 1;
	  dfd = -1;
	  fclose(fp);
	}
      else
	fprintf(fp,"fork => (pid_t)%d",r->eax);
      break;
    case SYS_read:
      fprintf(fp,"read => (ssize_t)%d",r->eax);
      if (dargs[1]) print_debug_data(fp, (char*)dargs[1], r->eax);
      break;
    case SYS_write:
      fprintf(fp,"write => (ssize_t)%d",r->eax);
      break;
    case SYS_open:
      fprintf(fp,"open => (int)%d",r->eax);
      break;
    case SYS_close:
      fprintf(fp,"close => (int)%d",r->eax);
      break;
    case SYS_wait4:
      fprintf(fp,"wait4 => (pid_t)%d",r->eax);
      if (dargs[1]) {
	if (WIFEXITED(*(int*)dargs[1]))
	  fprintf(fp,"\n\tWIFEXITED(*status) = TRUE"
		  "\n\t\tWEXITSTATUS(*status) = %d",
		  WEXITSTATUS(*(int*)dargs[1]));
	else fprintf(fp,"\n\tWIFEXITED(*status) = FALSE");
	if (WIFSIGNALED(*(int*)dargs[1]))
	  fprintf(fp,"\n\tWIFSIGNALED(*status) = TRUE"
		  "\n\t\tWTERMSIG(*status) = %d"
		  "\n\t\tWCOREDUMP(*status) = %s",
		  WTERMSIG(*(int*)dargs[1]),
		  WCOREDUMP(*(int*)dargs[1]) ? "TRUE" : "FALSE");
	else fprintf(fp,"\n\tWIFSIGNALED(*status) = FALSE");
	if (WIFSTOPPED(*(int*)dargs[1]))
	  fprintf(fp,"\n\tWIFSTOPPED(*status) = TRUE"
		  "\n\t\tWSTOPSIG(*status) = (%d)%s",
		  WSTOPSIG(*(int*)dargs[1]),
		  strsignal(WSTOPSIG(*(int*)dargs[1])));
	else fprintf(fp,"\n\tWIFSTOPPED(*status) = FALSE");
      }
      break;
    case SYS_link:
      fprintf(fp,"link => (int)%d",r->eax);
      break;
    case SYS_unlink:
      fprintf(fp,"unlink => (int)%d",r->eax);
      break;
    case SYS_chdir:
      fprintf(fp,"chdir => (int)%d",r->eax);
      break;
    case SYS_fchdir:
      fprintf(fp,"fchdir => (int)%d",r->eax);
      break;
    case SYS_mknod:
      fprintf(fp,"mknod => (int)%d",r->eax);
      break;
    case SYS_chmod:
      fprintf(fp,"chmod => (int)%d",r->eax);
      break;
    case SYS_chown:
      fprintf(fp,"chown => (int)%d",r->eax);
      break;
    case SYS_break:
      fprintf(fp,"break => (int)0x%x",r->eax);
      break;
    case SYS_getfsstat:
      fprintf(fp,"getfsstat => (int)%d",r->eax);
      if (r->eax != -1 && dargs[0])
	{
	  int i = 0, bytes = sizeof(struct statfs);
	  while (i < r->eax && bytes < dargs[1])
	    {
	      fprintf(fp,"\n\tbuf[%d]->f_type = %d",i,
		      ((struct statfs*)dargs[0])->f_type);
	      fprintf(fp,"\n\tbuf[%d]->f_flags = 0x%x",i,
		      ((struct statfs*)dargs[0])->f_flags);
	      fprintf(fp,"\n\tbuf[%d]->f_bsize = %ld",i,
		      ((struct statfs*)dargs[0])->f_bsize);
	      fprintf(fp,"\n\tbuf[%d]->f_iosize = %ld",i,
		      ((struct statfs*)dargs[0])->f_iosize);
	      fprintf(fp,"\n\tbuf[%d]->f_blocks = %ld",i,
		      ((struct statfs*)dargs[0])->f_blocks);
	      fprintf(fp,"\n\tbuf[%d]->f_bfree = %ld",i,
		      ((struct statfs*)dargs[0])->f_bfree);
	      fprintf(fp,"\n\tbuf[%d]->f_bavail = %ld",i,
		      ((struct statfs*)dargs[0])->f_bavail);
	      fprintf(fp,"\n\tbuf[%d]->f_files = %ld",i,
		      ((struct statfs*)dargs[0])->f_files);
	      fprintf(fp,"\n\tbuf[%d]->f_ffree = %ld",i,
		      ((struct statfs*)dargs[0])->f_ffree);
	      fprintf(fp,"\n\tbuf[%d]->f_fsid.val[0] = %d",i,
		      (int)((struct statfs*)dargs[0])->f_fsid);
	      fprintf(fp,"\n\tbuf[%d]->f_fsid.val[1] = %d",i,
		      *(((int*)&((struct statfs*)dargs[0])->f_fsid)+1));
	      fprintf(fp,"\n\tbuf[%d]->f_owner = %d",i,
		      ((struct statfs*)dargs[0])->f_owner);
	      fprintf(fp,"\n\tbuf[%d]->f_fstypename = %s",i,
		      ((struct statfs*)dargs[0])->f_fstypename);
	      fprintf(fp,"\n\tbuf[%d]->f_mntonname = %s",i,
		      ((struct statfs*)dargs[0])->f_mntonname);
	      fprintf(fp,"\n\tbuf[%d]->f_mntfromname = %s",i,
		      ((struct statfs*)dargs[0])->f_mntfromname);
	      i++;
	      bytes += sizeof(struct statfs);
	    }
	}
      break;
    case SYS_getpid:
      fprintf(fp,"getpid => (pid_t)%d",r->eax);
      break;
    case SYS_mount:
      fprintf(fp,"mount");
      break;
    case SYS_unmount:
      fprintf(fp,"unmount");
      break;
    case SYS_setuid:
      fprintf(fp,"setuid => (int)%d",r->eax);
      break;
    case SYS_getuid:
      fprintf(fp,"getuid => (uid_t)%d",r->eax);
      break;
    case SYS_geteuid:
      fprintf(fp,"geteuid => (uid_t)%d",r->eax);
      break;
    case SYS_ptrace:
      fprintf(fp,"ptrace => (int)%d",r->eax);
      break;
    case SYS_recvmsg:
      fprintf(fp,"recvmsg");
      break;
    case SYS_sendmsg:
      fprintf(fp,"sendmsg");
      break;
    case SYS_recvfrom:
      fprintf(fp,"recvfrom => (ssize_t)%d",r->eax);
      if (dargs[5]) fprintf(fp,"\n\t*fromlen = %d", *(int*)dargs[5]);
      fprintf(fp,"\n\tfrom = %p",(void*)dargs[4]);
      if (dargs[4])
	{
	  char *s;

	  fprintf(fp,"\n\t\tsa_len = %d\n",
		  (int)(((struct sockaddr*)dargs[4])->sa_len));
	  fprintf(fp,"\t\tsa_family = %d\n",
		  (int)(((struct sockaddr*)dargs[4])->sa_family));
	  fprintf(fp,"\t\tsin_port = %d\n",
		  (int)(((struct sockaddr_in*)dargs[4])->sin_port));
	  s = inet_ntoa(((struct sockaddr_in*)dargs[4])->sin_addr);
	  fprintf(fp,"\t\tsin_addr = %s", s ? s : "0?");
	}
      if (dargs[1]) print_debug_data(fp, (char*)dargs[1], r->eax);
      break;
    case SYS_accept:
      fprintf(fp,"accept");
      break;
    case SYS_getpeername:
      fprintf(fp,"getpeername => (int)%d",r->eax);
      fprintf(fp,"\n\tname = %p",(void*)dargs[1]);
      if (dargs[1])
	{
	  char *s;

	  fprintf(fp,"\n\t\tsa_len = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_len));
	  fprintf(fp,"\t\tsa_family = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_family));
	  fprintf(fp,"\t\tsin_port = %d\n",
		  (int)(((struct sockaddr_in*)dargs[1])->sin_port));
	  s = inet_ntoa(((struct sockaddr_in*)dargs[1])->sin_addr);
	  fprintf(fp,"\t\tsin_addr = %s", s ? s : "0?");
	}
      if (dargs[2]) fprintf(fp,"\n\t*namelen = %d",*(int*)dargs[2]);
      break;
    case SYS_getsockname:
      fprintf(fp,"getsockname => (int)%d",r->eax);
      fprintf(fp,"\n\tname = %p",(void*)dargs[1]);
      if (dargs[1])
	{
	  char *s;

	  fprintf(fp,"\n\t\tsa_len = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_len));
	  fprintf(fp,"\t\tsa_family = %d\n",
		  (int)(((struct sockaddr*)dargs[1])->sa_family));
	  fprintf(fp,"\t\tsin_port = %d\n",
		  (int)(((struct sockaddr_in*)dargs[1])->sin_port));
	  s = inet_ntoa(((struct sockaddr_in*)dargs[1])->sin_addr);
	  fprintf(fp,"\t\tsin_addr = %s", s ? s : "0?");
	}
      if (dargs[2]) fprintf(fp,"\n\t*namelen = %d",*(int*)dargs[2]);
      break;
    case SYS_access:
      fprintf(fp,"access => (int)%d",r->eax);
      break;
    case SYS_chflags:
      fprintf(fp,"chflags");
      break;
    case SYS_fchflags:
      fprintf(fp,"fchflags");
      break;
    case SYS_sync:
      fprintf(fp,"sync");
      break;
    case SYS_kill:
      fprintf(fp,"kill => (int)%d",r->eax);
      break;
    case SYS_getppid:
      fprintf(fp,"getppid => (pid_t)%d",r->eax);
      break;
    case SYS_dup:
      fprintf(fp,"dup => (int)%d",r->eax);
      break;
    case SYS_pipe:
      fprintf(fp,"pipe => (int)%d",r->eax);
      if (dargs[0])
	fprintf(fp,"\n\tfiledes[0] = %d"
		"\n\tfiledes[1] = %d",
		*(int*)dargs[0],*((int*)dargs[0]+1));
      break;
    case SYS_getegid:
      fprintf(fp,"getegid => (gid_t)%d",r->eax);
      break;
    case ob_SYS_profil:
      fprintf(fp,"profil");
      break;
    case SYS_ktrace:
      fprintf(fp,"ktrace");
      break;
    case SYS_sigaction:
      fprintf(fp,"sigaction => (int)%d",r->eax);
      if (dargs[2]) {
	fprintf(fp,"\n\toact->sa_handler = %p",
		((struct sigaction*)dargs[2])->sa_handler);
	fprintf(fp,"\n\toact->sa_mask = ");
	{
	  int i, j=0;

	  for (i=1; i < 32; i++)
	    if (sigismember(&((struct sigaction*)dargs[2])->sa_mask, i)) {
	      fprintf(fp,"\n\t\t(%d)%s", i, strsignal(i));
	      j = 1;
	    }
	  if (!j) fprintf(fp,"(empty)");
	}
	fprintf(fp,"\n\toact->sa_flags = 0x%x",
		((struct sigaction*)dargs[2])->sa_flags);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_ONSTACK)
	  fprintf(fp,"\n\t\t(0x%x)SA_ONSTACK", SA_ONSTACK);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_RESTART)
	  fprintf(fp,"\n\t\t(0x%x)SA_RESTART", SA_RESTART);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_RESETHAND)
	  fprintf(fp,"\n\t\t(0x%x)SA_RESETHAND", SA_RESETHAND);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_NOCLDSTOP)
	  fprintf(fp,"\n\t\t(0x%x)SA_NOCLDSTOP", SA_NOCLDSTOP);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_NODEFER)
	  fprintf(fp,"\n\t\t(0x%x)SA_NODEFER", SA_NODEFER);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_NOCLDWAIT)
	  fprintf(fp,"\n\t\t(0x%x)SA_NOCLDWAIT", SA_NOCLDWAIT);
	if (((struct sigaction*)dargs[2])->sa_flags & SA_SIGINFO)
	  fprintf(fp,"\n\t\t(0x%x)SA_SIGINFO", SA_SIGINFO);
	if (((struct sigaction*)dargs[2])->sa_flags & 0x100) /* SA_USERTRAMP */
	  fprintf(fp,"\n\t\t(0x%x)SA_USERTRAMP", 0x100);
      }
      break;
    case SYS_getgid:
      fprintf(fp,"getgid => (gid_t)%d",r->eax);
      break;
    case SYS_sigprocmask:
      fprintf(fp,"sigprocmask => (int)%d",r->eax);
      {
	int i;

	fprintf(fp,"\n\t*oset = 0x%x", r->ecx);
	for (i=1; i < 32; i++)
	  if (sigismember(&r->ecx, i))
	    fprintf(fp,"\n\t\t(%d)%s", i, strsignal(i));
      }
      break;
    case SYS_getlogin:
      fprintf(fp,"getlogin => (char*)%s", r->eax ? (char*)r->eax : "(NULL)");
      break;
    case SYS_setlogin:
      fprintf(fp,"setlogin => (int)%d",r->eax);
      break;
    case SYS_acct:
      fprintf(fp,"acct");
      break;
    case SYS_sigpending:
      fprintf(fp,"sigpending");
      break;
    case SYS_sigaltstack:
      fprintf(fp,"sigaltstack");
      break;
    case SYS_ioctl:
      fprintf(fp,"ioctl => (int)%d",r->eax);
      if (dargs[1] == FIONREAD && dargs[2])
	fprintf(fp,"\n\t%d bytes ready", *(int*)dargs[2]);
      break;
    case ob_SYS_reboot:
      fprintf(fp,"reboot => (int)%d",r->eax);
      break;
    case SYS_revoke:
      fprintf(fp,"revoke");
      break;
    case SYS_symlink:
      fprintf(fp,"symlink => (int)%d",r->eax);
      break;
    case SYS_readlink:
      fprintf(fp,"readlink => (int)%d",r->eax);
      break;
    case SYS_execve:
      fprintf(fp,"execve => (int)%d",r->eax);
      break;
    case SYS_umask:
      fprintf(fp,"umask => (mode_t)0%o",r->eax);
      break;
    case SYS_chroot:
      fprintf(fp,"chroot => (int)%d",r->eax);
      break;
    case SYS_compat_43_sys_getkerninfo:
      fprintf(fp,"compat_43_sys_getkerninfo => (int)%d",r->eax);
      break;
    case SYS_ogetpagesize:
      fprintf(fp,"ogetpagesize => (int)%d",r->eax);
      break;
    case SYS_msync:
      fprintf(fp,"msync => (int)%d",r->eax);
      break;
    case SYS_vfork:
      if (!r->eax)
	{
	  forked_child = 1;
	  dfd = -1;
	  fclose(fp);
	}
      else
	fprintf(fp,"vfork => (pid_t)%d",r->eax);
      break;
      break;
    case SYS_sbrk:
      fprintf(fp,"sbrk => (char*)%p",(void*)r->eax);
      break;
    case SYS_sstk:
      fprintf(fp,"sstk");
      break;
    case SYS_vadvise:
      fprintf(fp,"vadvise");
      break;
    case SYS_munmap:
      fprintf(fp,"munmap => (int)%d",r->eax);
      break;
    case SYS_mprotect:
      fprintf(fp,"mprotect => (int)%d",r->eax);
      break;
    case SYS_madvise:
      fprintf(fp,"madvise");
      break;
    case SYS_mincore:
      fprintf(fp,"mincore");
      break;
    case SYS_getgroups:
      fprintf(fp,"getgroups");
      break;
    case SYS_setgroups:
      fprintf(fp,"setgroups");
      break;
    case SYS_getpgrp:
      fprintf(fp,"getpgrp => (pid_t)%d",r->eax);
      break;
    case SYS_setpgid:
      fprintf(fp,"setpgid => (int)%d",r->eax);
      break;
    case SYS_setitimer:
      fprintf(fp,"setitimer => (int)%d",r->eax);
      if (dargs[2])
	fprintf(fp,"\n\tovalue->it_interval.tv_sec = %ld\n"
		"\t\tit_interval.tv_usec = %ld\n"
		"\t\tit_value.tv_sec = %ld\n"
		"\t\tit_value.tv_usec = %ld",
		((struct itimerval*)dargs[2])->it_interval.tv_sec,
		((struct itimerval*)dargs[2])->it_interval.tv_usec,
		((struct itimerval*)dargs[2])->it_value.tv_sec,
		((struct itimerval*)dargs[2])->it_value.tv_usec);
      break;
    case SYS_swapon:
      fprintf(fp,"swapon");
      break;
    case SYS_getitimer:
      fprintf(fp,"getitimer");
      break;
    case SYS_dup2:
      fprintf(fp,"dup2 => (int)%d",r->eax);
      break;
    case SYS_fcntl:
      fprintf(fp,"fcntl => (int)%d",r->eax);
      break;
    case SYS_select:
      fprintf(fp,"select => (int)%d",r->eax);
      if (dargs[1])
	{
	  int x;

	  fprintf(fp,"\n\treadfds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[1]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\treadfds = NULL");
      if (dargs[2])
	{
	  int x;

	  fprintf(fp,"\n\twritefds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[2]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\twritefds = NULL");
      if (dargs[3])
	{
	  int x;

	  fprintf(fp,"\n\texceptfds =");

	  for (x = 0; x < dargs[0]; x++)
	    if (FD_ISSET(x,(fd_set*)dargs[3]))
	      fprintf(fp," %d",x);
	}
      else
	fprintf(fp,"\n\texceptfds = NULL");
      break;
    case SYS_fsync:
      fprintf(fp,"fsync => (int)%d",r->eax);
      break;
    case SYS_setpriority:
      fprintf(fp,"setpriority => (int)%d",r->eax);
      break;
    case SYS_socket:
      fprintf(fp,"socket => (int)%d",r->eax);
      break;
    case SYS_connect:
      fprintf(fp,"connect => (int)%d",r->eax);
      break;
    case SYS_getpriority:
      fprintf(fp,"getpriority => (int)%d",r->eax);
      break;
    case SYS_sigreturn:
      fprintf(fp,"sigreturn");
      break;
    case SYS_bind:
      fprintf(fp,"bind");
      break;
    case SYS_setsockopt:
      fprintf(fp,"setsockopt => (int)%d",r->eax);
      break;
    case SYS_listen:
      fprintf(fp,"listen");
      break;
    case SYS_sigsuspend:
      fprintf(fp,"sigsuspend");
      break;
    case SYS_vtrace:
      fprintf(fp,"vtrace");
      break;
    case SYS_gettimeofday:
      fprintf(fp,"gettimeofday => (int)%d",r->eax);
      if (r->eax == 0)
	{
	  if (dargs[0])
	    {
	      fprintf(fp,"\n\ttp->tv_sec = %ld\n",
		      ((struct timeval*)dargs[0])->tv_sec);
	      fprintf(fp,"\ttp->tv_usec = %ld",
		      ((struct timeval*)dargs[0])->tv_usec);
	    }
	  if (dargs[1])
	    {
	      fprintf(fp,"\n\ttzp->tz_minuteswest = %d\n",
		      ((struct timezone*)dargs[1])->tz_minuteswest);
	      fprintf(fp,"\ttzp->tz_dsttime = %d",
		      ((struct timezone*)dargs[1])->tz_dsttime);
	    }
	}
      break;
    case SYS_getrusage:
      fprintf(fp,"getrusage");
      break;
    case SYS_getsockopt:
      fprintf(fp,"getsockopt");
      break;
    case SYS_readv:
      fprintf(fp,"readv => (ssize_t)%d",r->eax);
      if (dargs[1]) print_debug_datav(fp, (void*)dargs[1], dargs[2], r->eax);
      break;
    case SYS_writev:
      fprintf(fp,"writev => (ssize_t)%d",r->eax);
      break;
    case SYS_settimeofday:
      fprintf(fp,"settimeofday");
      break;
    case SYS_fchown:
      fprintf(fp,"fchown");
      break;
    case SYS_fchmod:
      fprintf(fp,"fchmod");
      break;
    case SYS_rename:
      fprintf(fp,"rename");
      break;
    case SYS_flock:
      fprintf(fp,"flock");
      break;
    case SYS_mkfifo:
      fprintf(fp,"mkfifo");
      break;
    case SYS_sendto:
      fprintf(fp,"sendto => (ssize_t)%d",r->eax);
      break;
    case SYS_shutdown:
      fprintf(fp,"shutdown => (int)%d",r->eax);
      break;
    case SYS_socketpair:
      fprintf(fp,"socketpair");
      break;
    case SYS_mkdir:
      fprintf(fp,"mkdir => (int)%d",r->eax);
      break;
    case SYS_rmdir:
      fprintf(fp,"rmdir");
      break;
    case SYS_utimes:
      fprintf(fp,"utimes");
      break;
    case SYS_adjtime:
      fprintf(fp,"adjtime");
      break;
    case SYS_setsid:
      fprintf(fp,"setsid");
      break;
    case SYS_quotactl:
      fprintf(fp,"quotactl");
      break;
    case SYS_nfssvc:
      fprintf(fp,"nfssvc");
      break;
    case SYS_statfs:
      fprintf(fp,"statfs => (int)%d",r->eax);
      if (r->eax != -1 && dargs[1])
	{
	  fprintf(fp,"\n\tbuf->f_type = %d",
		  ((struct statfs*)dargs[1])->f_type);
	  fprintf(fp,"\n\tbuf->f_flags = 0x%x",
		  ((struct statfs*)dargs[1])->f_flags);
	  fprintf(fp,"\n\tbuf->f_bsize = %ld",
		  ((struct statfs*)dargs[1])->f_bsize);
	  fprintf(fp,"\n\tbuf->f_iosize = %ld",
		  ((struct statfs*)dargs[1])->f_iosize);
	  fprintf(fp,"\n\tbuf->f_blocks = %ld",
		  ((struct statfs*)dargs[1])->f_blocks);
	  fprintf(fp,"\n\tbuf->f_bfree = %ld",
		  ((struct statfs*)dargs[1])->f_bfree);
	  fprintf(fp,"\n\tbuf->f_bavail = %ld",
		  ((struct statfs*)dargs[1])->f_bavail);
	  fprintf(fp,"\n\tbuf->f_files = %ld",
		  ((struct statfs*)dargs[1])->f_files);
	  fprintf(fp,"\n\tbuf->f_ffree = %ld",
		  ((struct statfs*)dargs[1])->f_ffree);
	  fprintf(fp,"\n\tbuf->f_fsid.val[0] = %d",
		  (int)((struct statfs*)dargs[1])->f_fsid);
	  fprintf(fp,"\n\tbuf->f_fsid.val[1] = %d",
		  *(((int*)&((struct statfs*)dargs[1])->f_fsid)+1));
	  fprintf(fp,"\n\tbuf->f_owner = %d",
		  ((struct statfs*)dargs[1])->f_owner);
	  fprintf(fp,"\n\tbuf->f_fstypename = %s",
		  ((struct statfs*)dargs[1])->f_fstypename);
	  fprintf(fp,"\n\tbuf->f_mntonname = %s",
		  ((struct statfs*)dargs[1])->f_mntonname);
	  fprintf(fp,"\n\tbuf->f_mntfromname = %s",
		  ((struct statfs*)dargs[1])->f_mntfromname);
	}
      break;
    case SYS_fstatfs:
      fprintf(fp,"fstatfs => (int)%d",r->eax);
      if (r->eax != -1 && dargs[1])
	{
	  fprintf(fp,"\n\tbuf->f_type = %d",
		  ((struct statfs*)dargs[1])->f_type);
	  fprintf(fp,"\n\tbuf->f_flags = 0x%x",
		  ((struct statfs*)dargs[1])->f_flags);
	  fprintf(fp,"\n\tbuf->f_bsize = %ld",
		  ((struct statfs*)dargs[1])->f_bsize);
	  fprintf(fp,"\n\tbuf->f_iosize = %ld",
		  ((struct statfs*)dargs[1])->f_iosize);
	  fprintf(fp,"\n\tbuf->f_blocks = %ld",
		  ((struct statfs*)dargs[1])->f_blocks);
	  fprintf(fp,"\n\tbuf->f_bfree = %ld",
		  ((struct statfs*)dargs[1])->f_bfree);
	  fprintf(fp,"\n\tbuf->f_bavail = %ld",
		  ((struct statfs*)dargs[1])->f_bavail);
	  fprintf(fp,"\n\tbuf->f_files = %ld",
		  ((struct statfs*)dargs[1])->f_files);
	  fprintf(fp,"\n\tbuf->f_ffree = %ld",
		  ((struct statfs*)dargs[1])->f_ffree);
	  fprintf(fp,"\n\tbuf->f_fsid.val[0] = %d",
		  (int)((struct statfs*)dargs[1])->f_fsid);
	  fprintf(fp,"\n\tbuf->f_fsid.val[1] = %d",
		  *(((int*)&((struct statfs*)dargs[1])->f_fsid)+1));
	  fprintf(fp,"\n\tbuf->f_owner = %d",
		  ((struct statfs*)dargs[1])->f_owner);
	  fprintf(fp,"\n\tbuf->f_fstypename = %s",
		  ((struct statfs*)dargs[1])->f_fstypename);
	  fprintf(fp,"\n\tbuf->f_mntonname = %s",
		  ((struct statfs*)dargs[1])->f_mntonname);
	  fprintf(fp,"\n\tbuf->f_mntfromname = %s",
		  ((struct statfs*)dargs[1])->f_mntfromname);
	}
      break;
    case SYS_getfh:
      fprintf(fp,"getfh");
      break;
    case SYS_sysarch:
      fprintf(fp,"sysarch => (int)%d",r->eax);
      break;
    case SYS_ntp_gettime:
      fprintf(fp,"ntp_gettime");
      break;
    case SYS_ntp_adjtime:
      fprintf(fp,"ntp_adjtime");
      break;
    case SYS_setgid:
      fprintf(fp,"setgid => (int)%d",r->eax);
      break;
    case SYS_setegid:
      fprintf(fp,"setegid => (int)%d",r->eax);
      break;
    case SYS_seteuid:
      fprintf(fp,"seteuid => (int)%d",r->eax);
      break;
    case SYS_lfs_bmapv:
      fprintf(fp,"lfs_bmapv");
      break;
    case SYS_lfs_markv:
      fprintf(fp,"lfs_markv");
      break;
    case SYS_lfs_segclean:
      fprintf(fp,"lfs_segclean");
      break;
    case SYS_lfs_segwait:
      fprintf(fp,"lfs_segwait");
      break;
    case SYS_stat:
      fprintf(fp,"stat => (int)%d",r->eax);
      if (r->eax == 0)
	{
	  fprintf(fp,"\n\tsb->st_dev = %d\n",((struct stat*)dargs[1])->st_dev);
	  fprintf(fp,"\tsb->st_ino = %d\n",((struct stat*)dargs[1])->st_ino);
	  fprintf(fp,"\tsb->st_mode = 0x%x\n",
		  ((struct stat*)dargs[1])->st_mode);
	  fprintf(fp,"\tsb->st_nlink = %d\n",
		  ((struct stat*)dargs[1])->st_nlink);
	  fprintf(fp,"\tsb->st_uid = %d\n",((struct stat*)dargs[1])->st_uid);
	  fprintf(fp,"\tsb->st_gid = %d\n",((struct stat*)dargs[1])->st_gid);
	  fprintf(fp,"\tsb->st_rdev = %d\n",((struct stat*)dargs[1])->st_rdev);
	  fprintf(fp,"\tsb->st_atimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_sec);
	  fprintf(fp,"\tsb->st_atimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_mtimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_sec);
	  fprintf(fp,"\tsb->st_mtimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_ctimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_sec);
	  fprintf(fp,"\tsb->st_ctimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_size = %qd\n",
		  ((struct stat*)dargs[1])->st_size);
	  fprintf(fp,"\tsb->st_blocks = %qd\n",
		  ((struct stat*)dargs[1])->st_blocks);
	  fprintf(fp,"\tsb->st_blksize = %d\n",
		  ((struct stat*)dargs[1])->st_blksize);
	  fprintf(fp,"\tsb->st_flags = 0x%x\n",
		  ((struct stat*)dargs[1])->st_flags);
	  fprintf(fp,"\tsb->st_gen = %d",((struct stat*)dargs[1])->st_gen);
	}
      break;
    case SYS_fstat:
      fprintf(fp,"fstat => (int)%d",r->eax);
      if (r->eax == 0)
	{
	  fprintf(fp,"\n\tsb->st_dev = %d\n",((struct stat*)dargs[1])->st_dev);
	  fprintf(fp,"\tsb->st_ino = %d\n",((struct stat*)dargs[1])->st_ino);
	  fprintf(fp,"\tsb->st_mode = 0x%x\n",
		  ((struct stat*)dargs[1])->st_mode);
	  fprintf(fp,"\tsb->st_nlink = %d\n",
		  ((struct stat*)dargs[1])->st_nlink);
	  fprintf(fp,"\tsb->st_uid = %d\n",((struct stat*)dargs[1])->st_uid);
	  fprintf(fp,"\tsb->st_gid = %d\n",((struct stat*)dargs[1])->st_gid);
	  fprintf(fp,"\tsb->st_rdev = %d\n",((struct stat*)dargs[1])->st_rdev);
	  fprintf(fp,"\tsb->st_atimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_sec);
	  fprintf(fp,"\tsb->st_atimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_mtimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_sec);
	  fprintf(fp,"\tsb->st_mtimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_ctimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_sec);
	  fprintf(fp,"\tsb->st_ctimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_size = %qd\n",
		  ((struct stat*)dargs[1])->st_size);
	  fprintf(fp,"\tsb->st_blocks = %qd\n",
		  ((struct stat*)dargs[1])->st_blocks);
	  fprintf(fp,"\tsb->st_blksize = %d\n",
		  ((struct stat*)dargs[1])->st_blksize);
	  fprintf(fp,"\tsb->st_flags = 0x%x\n",
		  ((struct stat*)dargs[1])->st_flags);
	  fprintf(fp,"\tsb->st_gen = %d",((struct stat*)dargs[1])->st_gen);
	}
      break;
    case SYS_lstat:
      fprintf(fp,"lstat => (int)%d",r->eax);
      if (r->eax == 0)
	{
	  fprintf(fp,"\n\tsb->st_dev = %d\n",((struct stat*)dargs[1])->st_dev);
	  fprintf(fp,"\tsb->st_ino = %d\n",((struct stat*)dargs[1])->st_ino);
	  fprintf(fp,"\tsb->st_mode = 0x%x\n",
		  ((struct stat*)dargs[1])->st_mode);
	  fprintf(fp,"\tsb->st_nlink = %d\n",
		  ((struct stat*)dargs[1])->st_nlink);
	  fprintf(fp,"\tsb->st_uid = %d\n",((struct stat*)dargs[1])->st_uid);
	  fprintf(fp,"\tsb->st_gid = %d\n",((struct stat*)dargs[1])->st_gid);
	  fprintf(fp,"\tsb->st_rdev = %d\n",((struct stat*)dargs[1])->st_rdev);
	  fprintf(fp,"\tsb->st_atimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_sec);
	  fprintf(fp,"\tsb->st_atimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_atimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_mtimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_sec);
	  fprintf(fp,"\tsb->st_mtimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_mtimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_ctimespec->tv_sec = %d\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_sec);
	  fprintf(fp,"\tsb->st_ctimespec->tvnsec = %ld\n",
		  ((struct stat*)dargs[1])->st_ctimespec.tv_nsec);
	  fprintf(fp,"\tsb->st_size = %qd\n",
		  ((struct stat*)dargs[1])->st_size);
	  fprintf(fp,"\tsb->st_blocks = %qd\n",
		  ((struct stat*)dargs[1])->st_blocks);
	  fprintf(fp,"\tsb->st_blksize = %d\n",
		  ((struct stat*)dargs[1])->st_blksize);
	  fprintf(fp,"\tsb->st_flags = 0x%x\n",
		  ((struct stat*)dargs[1])->st_flags);
	  fprintf(fp,"\tsb->st_gen = %d",((struct stat*)dargs[1])->st_gen);
	}
      break;
    case SYS_pathconf:
      fprintf(fp,"pathconf");
      break;
    case SYS_fpathconf:
      fprintf(fp,"fpathconf");
      break;
    case SYS_getrlimit:
      fprintf(fp,"getrlimit => (int)%d",r->eax);
      if (dargs[1]) {
	if (((struct rlimit *)dargs[1])->rlim_cur == RLIM_INFINITY)
	  fprintf(fp,"\n\trlp->rlim_cur = RLIM_INFINITY");
	else
	  fprintf(fp,"\n\trlp->rlim_cur = %qd",
		  ((struct rlimit *)dargs[1])->rlim_cur);
	if (((struct rlimit *)dargs[1])->rlim_max == RLIM_INFINITY)
	  fprintf(fp,"\n\trlp->rlim_max = RLIM_INFINITY");
	else
	  fprintf(fp,"\n\trlp->rlim_max = %qd",
		  ((struct rlimit *)dargs[1])->rlim_max);
      }
      break;
    case SYS_setrlimit:
      fprintf(fp,"setrlimit => (int)%d",r->eax);
      break;
    case SYS_getdirentries:
      fprintf(fp,"getdirentries");
      break;
    case SYS_mmap:
      fprintf(fp,"mmap => (caddr_t)%p",(void*)r->eax);
      break;
    case SYS___syscall:
      fprintf(fp,"__syscall");
      break;
    case SYS_lseek:
      {
	quad_t q;
	((int*)&q)[0] = r->eax;
	((int*)&q)[1] = r_s.edx;
	fprintf(fp,"lseek => (off_t)0x%qx",q);
      }
      break;
    case SYS_truncate:
      fprintf(fp,"truncate => (int)%d",r->eax);
      break;
    case SYS_ftruncate:
      fprintf(fp,"ftruncate => (int)%d",r->eax);
      break;
    case SYS___sysctl:
      fprintf(fp,"__sysctl => (int)%d",r->eax);
      break;
    case SYS_mlock:
      fprintf(fp,"mlock");
      break;
    case SYS_munlock:
      fprintf(fp,"munlock");
      break;
    case SYS_undelete:
      fprintf(fp,"undelete");
      break;
    case SYS_futimes:
      fprintf(fp,"futimes");
      break;
    case SYS___semctl:
      fprintf(fp,"__semctl");
      break;
    case SYS_semget:
      fprintf(fp,"semget");
      break;
    case SYS_semop:
      fprintf(fp,"semop");
      break;
    case SYS_semconfig:
      fprintf(fp,"semconfig");
      break;
    case SYS_msgctl:
      fprintf(fp,"msgctl");
      break;
    case SYS_msgget:
      fprintf(fp,"msgget");
      break;
    case SYS_msgsnd:
      fprintf(fp,"msgsnd");
      break;
    case SYS_msgrcv:
      fprintf(fp,"msgrcv");
      break;
    case SYS_shmat:
      fprintf(fp,"shmat");
      break;
    case SYS_shmctl:
      fprintf(fp,"shmctl");
      break;
    case SYS_shmdt:
      fprintf(fp,"shmdt");
      break;
    case SYS_shmget:
      fprintf(fp,"shmget");
      break;
    case SYS_nanosleep:
      fprintf(fp,"nanosleep => (int)%d",r->eax);
      if (dargs[1])
	{
	  struct timespec *t = (void*)dargs[1];
	  fprintf(fp,"\n\trmtp->tv_sec = %ld\n", (long)t->tv_sec);
	  fprintf(fp,"\trmtp->tv_nsec = %ld", t->tv_nsec);
	}
      break;
    case SYS_minherit:
      fprintf(fp,"minherit");
      break;
    case SYS_rfork:
      fprintf(fp,"rfork");
      break;
    case SYS_poll:
      fprintf(fp,"poll => (int)%d",r->eax);
      {
	int i;
	for (i=0; i < dargs[1]; i++)
	  fprintf(fp,"\n\tfds[%d].revents = %x", i,
		  ((struct pollfd*)dargs[0])->revents);
      }
      break;
    case SYS_issetugid:
      fprintf(fp,"issetugid => (int)%d",r->eax);
      break;
    case SYS_MAXSYSCALL:

    default:
      fprintf(fp,"UNKNOWN SYSCALL => THIS SHOULD NEVER HAPPEN");
    }
  if (!forked_child) fprintf(fp,"\n");
  if (r->eax == -1)
    fprintf(fp,"\terrno = %d => %s\n",derrno,strerror(derrno));
post_switch:
  if (!forked_child) fflush(fp);
}
