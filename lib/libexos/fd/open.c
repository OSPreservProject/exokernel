
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

#if 0
#undef PRINTF_LEVEL
#define PRINTF_LEVEL 99
#endif
#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>

#include "fd/fdstat.h"
#include <stdlib.h>
#include <exos/debug.h>

#include <unistd.h>
#include <exos/callcount.h>

#include <stdarg.h>


//#define PR kprintf("* %3d *",__LINE__)
#define PR

/* to use proven use the old open code in case of bugs */
//#define OLDOPEN

#define lprintf(format, args...)
//#define lprintf(format,args...) printf(format, ## args);


#define nprintf(format,args...) 
//#define nprintf printf

/* from man pages (man stdout)  */
/*      Opening the files /dev/stdin, /dev/stdout and /dev/stderr is equivalent */
/*      to the following calls: */
/*            fd = fcntl(STDIN_FILENO,  F_DUPFD, 0); */
/*            fd = fcntl(STDOUT_FILENO, F_DUPFD, 0); */
/*            fd = fcntl(STDERR_FILENO, F_DUPFD, 0); */


int
open(const char *path, int flags, ...) {
    int error;
    mode_t mode;
    va_list ap;
    struct file * filp, * cwd;
    struct file dir_filp, tmp_filp, cwd_filp;
    int fd, followedlinks = 0;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];
    char path0[PATH_MAX];

    if(path == NULL) {
      errno = EFAULT;
      return -1;
    }

    OSCALLENTER(OSCALL_open);
    DPRINTF(SYS_LEVEL,("open %s 0x%x 0x%x: entering\n",path,flags,mode));

    va_start(ap,flags);
    mode = va_arg(ap,mode_t);
    va_end(ap);

    lprintf("%18s open \"%s\" 0x%x 0x%x: entering\n","##############",path,flags,mode);


    if (path[0] == '/' && path[1] == 'd') {
      if (!strncmp(path,"/dev",4)) {
	int r=0, rtrue=1;
	if (!strcmp(path,"/dev/stdout"))
	  r = dup(1);
#ifdef DEVTTY
	else if (!strcmp(path,"/dev/tty"))
	  r = dup(0);
#endif
	else if (!strcmp(path,"/dev/stdin"))
	  r = dup(0);
	else if (!strcmp(path,"/dev/stderr"))
	  r = dup(2);
	else rtrue = 0;
	if (rtrue) {
	  OSCALLEXIT(OSCALL_open);
	  return r;
	}
      }
    }
    START(fd,open);

    cwd = __current->cwd;		/* set the relative searching point,
				 * this might change is following symlinks */

    /*if (!strcmp(path,"./")) {errno = EISDIR; return -1;}*/
    /*printf("open: %s, flags: %08x, mode: %08x\n",path,flags,mode);*/
    if (*path == (char)0) {path = ".";} /* have to check if this is POSIX
					compliant */
    fd = getfd();
    if (fd == OUTFD) {
      errno = EMFILE;
      OSCALLEXIT(OSCALL_open);
      return -1;
    }
    if (fd == NOFILPS) {
      errno = ENFILE;
      OSCALLEXIT(OSCALL_open);
      return -1;
    }

    filp = __current->fd[fd];

    clear_filp_lock(&dir_filp);

#ifdef OLDOPEN
    error = namei(NULL,path,&dir_filp,1);
    nprintf("First TRY error: %d, path: %s, ino: %d, mode: %x\n",
	    error, path, dir_filp.f_ino, dir_filp.f_mode);
    if (!error) {
      if (S_ISDIR(dir_filp.f_mode)) {
	/* case we are opening a directory */
	error = DOOP(&dir_filp,open,(&dir_filp,filp,".",flags,mode));
	nprintf("First Open error: %d, path: %s, ino: %d, mode: %x\n",
		error, ".", dir_filp.f_ino, dir_filp.f_mode);
	namei_release(&dir_filp,0);
	if (error) {
	  DPRINTF(SYSHELP_LEVEL,
		  ("open: error status = %d, while opening %s, ",error,path));
	  putfd(fd);
	  OSCALLEXIT(OSCALL_open);
	  return -1;
	}
	STOP(fd,open);
	filp_refcount_init(filp);
	filp_refcount_inc(filp);
	lprintf("%18s open return fd: %d ino: %d\n","",fd,filp->f_ino);
	OSCALLEXIT(OSCALL_open);
	return(fd);
      }
      namei_release(&dir_filp,0);
    }




redo:
    SplitPath(path,suffix,prefix);
#if 1
    DPRINTF(SYS_LEVEL,("open: path: \"%s\", suffix: \"%s\" prefix: \"%s\"\n",
		       path, suffix,prefix));
#endif
    PR;
    error = namei(cwd,prefix,&dir_filp,1);
    nprintf("Second NAMEI error: %d, path: %s, ino: %d, mode: %x\n",
	    error, prefix, dir_filp.f_ino, dir_filp.f_mode);
    if (followedlinks) {PR;namei_release(cwd,0);}

    if (error) goto putfd_and_err;


    if (! S_ISDIR(dir_filp.f_mode)) {
      if (S_ISLNK(dir_filp.f_mode)) {
	fprintf(stderr,"\n#\n#\n# not traversing symlink before file for now\n");
	assert(0);
      }
      namei_release(&dir_filp,0);
      errno = ENOTDIR;
      goto putfd_and_err;
    }

    PR;
    error = namei(&dir_filp,suffix,&tmp_filp,0);
    nprintf("Lookup NAMEI error: %d, path: %s, ino: %d, mode: %x\n",
	    error, suffix, tmp_filp.f_ino, tmp_filp.f_mode);
    if (!error) {
      if (S_ISLNK(tmp_filp.f_mode)) {
	demand(CHECKOP(&tmp_filp,readlink),tmp_filp is a symlink);
	lprintf("S_ISLNK %s,%d is a symlink",suffix,tmp_filp.f_ino);
    PR;
	error = DOOP(&tmp_filp,readlink,(&tmp_filp,path0,PATH_MAX));
	if (error >= 0) path0[error] = 0;
	else demand(0,lnk should have readlink);
	nprintf("First Readlink error: %d, path: %s, ino: %d, mode: %x\n",
		error, path0, tmp_filp.f_ino, tmp_filp.f_mode);
	/* the contents of buf are not null terminated */
    PR;
	namei_release(&tmp_filp,0);
	path = &path0[0];
	lprintf(" pointing to %s\n",path);
	cwd_filp = dir_filp;
	cwd = &cwd_filp;
	followedlinks++;
	lprintf("fl: %d\n",followedlinks);
	
	goto redo;
      } else {
    PR;
	namei_release(&tmp_filp,0);
      }
    }
    

#else
#define ACQUIRE(filp) if (CHECKOP((filp),acquire)) DOOP((filp),acquire,(filp))

    ACQUIRE(cwd);
    SplitPath(path,suffix,prefix);
retry:
    /* cwd has a reference and not dir_filp */
    if (EQDOT(prefix) || prefix[0] == '\0') {
      lprintf("retry on prefix \"%s\" going to lookup\n",prefix);
      dir_filp = *cwd;
      goto lookup;
    }
    if (followedlinks > LINK_MAX) {
      errno = ELOOP;
      goto putfd_and_err;
    }
    lprintf("NAMEI ON: \"%s\" suffix: %s, path: %s\n",prefix,suffix,path);
    error = namei(cwd,prefix,&dir_filp,1);
    lprintf("NAMEI RETURNED: %d, errno: %d    <%d,%d>\n",error,errno,
	    dir_filp.f_dev, dir_filp.f_ino);
    namei_release(cwd,0);
    if (error) {goto putfd_and_err;}

    /* holding reference to dir filp only */
    
    if (S_ISDIR(dir_filp.f_mode)) {
      goto lookup;
    } else if (S_ISLNK(dir_filp.f_mode)) {
      demand(CHECKOP(&dir_filp,readlink),dir_filp is a symlink);
      lprintf("S_ISLNK %s,%d is a symlink",suffix,tmp_filp.f_ino);
      error = DOOP(&dir_filp,readlink,(&dir_filp,path0,PATH_MAX));
      if (error >= 0) path0[error] = 0;	/* NULL terminate */
      else demand(0,lnk should able to readlink);

      path = &path0[0];
      lprintf(" pointing to %s\n",path);
      cwd_filp = dir_filp;
      cwd = &cwd_filp;
      followedlinks++;
      
      SplitPath(path,suffix,prefix);
      goto retry;
    } else {
      errno = ENOTDIR;
      goto release_and_err;
    }
lookup:
    /* holding reference to dir_filp and not cwd */
#if 0
    /* can not skip here since there might be a mount point traversal
       left in the chain */
    if (EQDOT(suffix) || suffix[0] == '\0') {
      lprintf("retry on suffix \"%s\" going to open\n",prefix);
      goto open;
    }
#endif
    lprintf("LOOKUP ON \"%s\"\n",suffix);
    error = namei(&dir_filp,suffix,&tmp_filp,0);
    lprintf("LOOKUP RETURNED %d  errno: %d mode: %x lnk: %d  <%d,%d>\n",
	    error,errno,tmp_filp.f_mode, S_ISLNK(tmp_filp.f_mode),
	    tmp_filp.f_dev, tmp_filp.f_ino);
    if (error) {
PR;
      if (errno == ENOENT) goto open;
      else goto release_and_err;
    } else {
PR;
      /* holding reference to dir_filp and tmp_filp */
      if (S_ISLNK(tmp_filp.f_mode)) {      
PR;
	demand(CHECKOP(&tmp_filp,readlink),tmp_filp is a symlink);
	lprintf("S_ISLNK %s,%d is a symlink",suffix,tmp_filp.f_ino);
	error = DOOP(&tmp_filp,readlink,(&tmp_filp,path0,PATH_MAX));
	if (error >= 0) path0[error] = 0; /* NULL terminate */
	else demand(0,lnk should be able readlink);
	namei_release(&tmp_filp,0);
	/* holding reference to dir_filp only */
	path = &path0[0];

	cwd_filp = dir_filp;
	cwd = &cwd_filp;
	/* holding reference to cwd only */
	followedlinks++;

	SplitPath(path,suffix,prefix);
	goto retry;
      }
      if (S_ISDIR(tmp_filp.f_mode)) {
	namei_release(&dir_filp,0);
	
	lprintf("OPEN ON \"%s\"\n",suffix);
	error = DOOP(&tmp_filp,open,(&tmp_filp,filp,".",flags,mode));
	lprintf("OPEN RETURNED %d  errno: %d <%d,%d>\n",error,errno,
		tmp_filp.f_dev, tmp_filp.f_ino);
	
	namei_release(&tmp_filp,0);
	
	goto check_error;
      } else {
	/* if not dir or symlink, then must be file, socket or device or directory */
	namei_release(&tmp_filp,0);
      }
    }
    PR;
open:
      /* holding reference to dir_filp only */
#endif /* OLDOPEN */
      
    lprintf("OPEN ON \"%s\"\n",suffix);
    error = DOOP(&dir_filp,open,(&dir_filp,filp,suffix,flags,mode));
    lprintf("OPEN RETURNED %d  errno: %d <%d,%d>\n",error,errno,
	    dir_filp.f_dev, dir_filp.f_ino);

    namei_release(&dir_filp,0);
check_error:
    /* if (error) then holding no reference else holding reference to new filp only */
    if (error) {
      DPRINTF(SYSHELP_LEVEL,
	      ("open: error status = %d, while opening %s, ",error,path));
      goto putfd_and_err;
    }
    STOP(fd,open);
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    lprintf("%18s open return fd: %d <%d.%d>\n","##############",fd,filp->f_dev,filp->f_ino);
    OSCALLEXIT(OSCALL_open);
    return(fd);
#ifndef OLDOPEN
release_and_err:
    namei_release(&dir_filp,0);
#endif
putfd_and_err:
    putfd(fd); 
    STOP(fd,open);
    OSCALLEXIT(OSCALL_open);
    return -1;
}
