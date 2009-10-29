
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

#include <dirent.h>

#define SLS_OPEN 1
#define SLS_READ 2
#define SLS_LSEEK 3
#define SLS_CLOSE 4
#define SLS_MMAP 5
#define SLS_MPROTECT 6
#define SLS_MUNMAP 7
#define SLS_DUP 8
#define SLS_DUP2 9
#define SLS_VERIFY 10
#define SLS_PERROR 11
#define SLS_KPRINTF  12
#define SLS_PRINTF 13
#define SLS_KPRINTD  14
#define SLS_PRINTD 15
#define SLS_WRITE 16
#define SLS_OPENDIR 17
#define SLS_READDIR 18
#define SLS_CLOSEDIR 19
#define SLS_STATUS 20
#define SLS_READINSERT 21
#define SLS_NEWPAGE 22

#define SLS_OK 0
#define SLS_INV -1

//int slseid;
int sls_malloc_ptr;
int wasted_malloc_space;


extern int __sls_asm_sipcout (int, int, int, int, u_int)
     __attribute__ ((regparm (3)));

#define sls_sipcout(env, a1, a2, a3, a4) __sls_asm_sipcout(a1, a2, a3, a4, env)

     /*
int            S_INIT     ();
int            S_OPEN     (char *, int, int);
int            S_LSEEK    (int, int, int);
int            S_READ     (int, char *, int);
int            S_DUP      (int);
int            S_DUP2     (int,int);
void           S_CLOSE    (int);
caddr_t        S_MMAP     (caddr_t, size_t, int, int, int, off_t);
int            S_MPROTECT (caddr_t, size_t, int);
int            S_MUNMAP   (caddr_t, size_t);
void           S_PRINTF   (char *, int );
void           S_PRINTD   (int, int);
int            S_WRITE    (int, char *, int);
DIR *S_OPENDIR  (char *);
struct dirent *S_READDIR  (DIR  *);
void           S_CLOSEDIR (DIR  *);
int            S_STATUS   ();

void          *S_MALLOC   (size_t);
void          *S_REALLOC  (void *, size_t);
void          *S_CALLOC   (size_t, size_t);
void           S_FREE     (void *);
void           S_BZERO    (void *, int);

char          *S_strerror  (int);
char          *S_strsep    (register char **, register const char *);
char          *S_strdup    (const char *);
char          *S_strcpy    (char *,           const char *);
int            S_strcmp    (const char *,     const char *);
int            S_strncmp   (const char *,     const char *,          size_t);
char          *S___strerror(int,              char*);
char          *S_strcat    (char *,           const char *);
char          *S_strchr    (const char *,     int);
void          *S_memcpy    (void *,           const void *,          size_t);
long           S_strtol_10 (const char *,     char**,                int);

uid_t          S_getuid    (void);
uid_t          S_geteuid   (void);
gid_t          S_getgid    (void);
gid_t          S_getegid   (void);
char          *S_getenv    (const char *);
char          *ui2s        (unsigned int);
     */

/* open-only flags */
#define O_RDONLY        0x0000          /* open for reading only */
#define O_WRONLY        0x0001          /* open for writing only */
#define O_RDWR          0x0002          /* open for reading and writing */
#define O_ACCMODE       0x0003          /* mask for above modes */

