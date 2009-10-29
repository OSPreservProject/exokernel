
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

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>
#include <fcntl.h>
#include <exos/callcount.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <utime.h>
#include <exos/debug.h>
int
utimes(const char *path, const struct timeval *times) {
    int error;
    struct timeval t[2];
    struct file filp;
    
    OSCALLENTER(OSCALL_utimes);
    DPRINTF(SYS_LEVEL,("utime %s %p: entering\n",path,times));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_utimes);
      return -1;
    }
    
    error = namei(NULL,path,&filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_utimes);
      return -1;
    }
    
    if (CHECKOP(&filp,utimes) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_utimes);
	return -1;
    }

    if (!times) {
      gettimeofday(&t[0],(struct timezone *)0);
      t[1] = t[0];
      error = DOOP(&filp,utimes,(&filp,&t[0]));
    } else {
      error = DOOP(&filp,utimes,(&filp,times));
    }
    
    namei_release(&filp,0);
  
    OSCALLEXIT(OSCALL_utimes);
    return error;
}

int 
futimes(int fd, const struct timeval *times) {
    struct file *filp;
    struct timeval t[2];
    int error;

    OSCALLENTER(OSCALL_futimes);
    DPRINTF(SYS_LEVEL,("futimes %d %p: entering\n",fd,times));
    CHECKFD(fd, OSCALL_futimes);
    filp = __current->fd[fd];

    if (CHECKOP(filp,utimes) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_futimes);
	return -1;
    }
    if (!times) {
      gettimeofday(&t[0],(struct timezone *)0);
      t[1] = t[0];
      error = DOOP(filp,utimes,(filp,&t[0]));
    } else {
      error = DOOP(filp,utimes,(filp,times));
    }
    
    OSCALLEXIT(OSCALL_futimes);
    return error;
}
