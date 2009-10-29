
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
#define PRINTF_LEVEL 1
#endif

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>
#include <exos/callcount.h>
#include <fcntl.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

/* unistd stuff */
#define SEEK_CUR 1
extern int close(int);

#include <exos/debug.h>
#include "fd/fdstat.h"



static long pathconf0(int name) {
  switch(name) {
  case _PC_LINK_MAX:
    return LINK_MAX;
  case _PC_MAX_CANON:
    break;			/* implemented, but in another file */
  case _PC_MAX_INPUT:
    break;			/* implemented, but in another file */
  case _PC_NAME_MAX:
    return NAME_MAX;
  case _PC_PATH_MAX:
    return PATH_MAX;
  case _PC_PIPE_BUF:
    return 512;			/* real value is in pipe.c */
  case _PC_CHOWN_RESTRICTED:
    return 1;
  case _PC_NO_TRUNC:
    return 0;
  case _PC_VDISABLE:
    break;			/* implemented, but in another file */
  }
  errno = EINVAL;
  return -1;
}  

long
pathconf(const char *path, int name) {
    OSCALLENTER(OSCALL_pathconf);
    return pathconf0(name);
    OSCALLEXIT(OSCALL_pathconf);
}

long
fpathconf(int fd, int name) {
    OSCALLENTER(OSCALL_fpathconf);
    return pathconf0(name);
    OSCALLEXIT(OSCALL_fpathconf);
}

