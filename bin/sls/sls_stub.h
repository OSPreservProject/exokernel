
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

#include "sls.h"
#include <errno.h>

#define _ctype_ S_ctype_
#define _C_ctype_ S_C_ctype_
extern const char *S_ctype_;

#define open S_OPEN
#define read S_READ
#define write S_WRITE
#define fseek S_LSEEK
#define dup  S_DUP
#define dup2 S_DUP2
#define close S_CLOSE
#define mmap S_MMAP
#define mprotect S_MPROTECT
#define munmap S_MUNMAP
#define  perror(A) S_PRINTF(A,-1)
#define slskprintf(A) S_PRINTF(A,0)
#define  slsprintf(A) S_PRINTF(A,1)
#define slskprintd(A) S_PRINTD(A,0)
#define  slsprintd(A) S_PRINTD(A,1)
#define slskprinth(A) S_PRINTD(A,2)
#define  slsprinth(A) S_PRINTD(A,3)
#define opendir S_OPENDIR
#define readdir S_READDIR
#define closedir S_CLOSEDIR

#define malloc S_MALLOC
#define free S_FREE
#define calloc S_CALLOC
#define realloc S_REALLOC
#define bzero   S_BZERO

#define strerror S_strerror
#define strsep S_strsep
#define strdup S_strdup
#define strcpy S_strcpy
#define strcmp S_strcmp
#define strncmp S_strncmp
#define __strerror S___strerror
#define strcat S_strcat
#define strchr S_strchr
#define memcpy S_memcpy
#define strtol S_strtol_10
#define getuid S_getuid
#define geteuid S_geteuid
#define getgid S_getgid
#define getegid S_getegid
#define getenv S_getenv
