
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#ifndef __EXOS_EXEC_H__
#define __EXOS_EXEC_H__

#include <sys/types.h>

#define __RESERVED_PAGES 5

struct _exos_exec_args {
  int eea_prog_fd; /* The fd corresponding to the image of the executable */
  u_int eea_reserved_pages; /* Mapped but unused pages for the child */
  char *eea_reserved_first; /* The addr of the first reserved page */
};

extern struct _exos_exec_args *__eea;

/* for use in the flags field of __exos_execve */
#define _EXEC_EXECONLY    0x1 /* Don't return to the parent */
#define _EXEC_USE_FD      0x2 /* Use passed in fd instead of path */
#define _EXEC_SHLIB_ONLY  0x4 /* Require parent and child to both be linked
				 against the shared library, and don't check
				 for a new one */
int __exos_execve(const char *path, char *const argv[], char * const envptr[],
		  int fd, u_int flags);

int __load_image (int, int, u_int, u_int);
u_int __load_prog(const char *path, const char *arg0, int _static,
		  u_int envid);
u_int __load_prog_fd(int fd, int _static, u_int envid);

#endif
