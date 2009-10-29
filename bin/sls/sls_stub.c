
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

#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <sys/dirent.h>
#include <sys/pmap.h>
#include <sys/sysinfo.h>
#include <xuser.h>
#include <malloc.h>
#include <fd/proc.h>
#include <stdio.h>
#include <ctype.h>


extern proc_info *dyn_ProcInfo;

#undef sipcout
#define sipcout sls_sipcout
#include "sls_stub.h"
#define ProcInfo dyn_ProcInfo
#include <nameserv.h>

extern char **dyn_environ;

#define dprintf if (0) slsprintf
#define d2printf slsprintf
#define dprintd if (0) slsprintd
#define d2printd slskprintd


const char S_C_ctype_[1 + 256] = {
        0,
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C,
        _C,     _C|_S,  _C|_S,  _C|_S,  _C|_S,  _C|_S,  _C,     _C,
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C,
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C,
        _S|_B,  _P,     _P,     _P,     _P,     _P,     _P,     _P,
        _P,     _P,     _P,     _P,     _P,     _P,     _P,     _P,
        _N,     _N,     _N,     _N,     _N,     _N,     _N,     _N,
        _N,     _N,     _P,     _P,     _P,     _P,     _P,     _P,
        _P,     _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U,
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U,
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U,
        _U,     _U,     _U,     _P,     _P,     _P,     _P,     _P,
        _P,     _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L,
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L,
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L,
        _L,     _L,     _L,     _P,     _P,     _P,     _P,     _C
};

const char *S_ctype_ = S_C_ctype_;



int S_INIT () {
  /*kprintf("==> S_INIT\n"); */
  slseid = nameserver_lookup(SID);
  /*kprintf("==> SLS at %d\n",slseid); */
  if (slseid == NS_INVAL) {
    return (0);
  }
  sls_malloc_ptr = BEGIN_TEMP_MALLOC_REGION;
  sys_self_insert_pte(0,(0|PG_U|PG_W|PG_P),PGNO(sls_malloc_ptr)<<PGSHIFT);
 
   wasted_malloc_space = 0;
   return (1);
}

void *S_MALLOC(size_t size) {
  int cvp;
  cvp = PGNO(sls_malloc_ptr);
  if ((size % 4) != 0) {
    size += 4 - (size %4);}
  *((int *)sls_malloc_ptr) = size;
  sls_malloc_ptr += (size + sizeof(int));
  if (sls_malloc_ptr > END_TEMP_MALLOC_REGION) {
    slskprintf("Temp Malloc Failed\n");
    return (0); }
  while (PGNO(sls_malloc_ptr) != cvp) {
    cvp++;
    sys_self_insert_pte(0,(0|PG_U|PG_W|PG_P),cvp<<PGSHIFT);
    slskprintf("::::::::::::::::::Mapped vp 0x");
    slskprinth(PGNO(sls_malloc_ptr));
    slskprintf("to to pp 0x");
    slskprinth(PGNO(vpt[PGNO(sls_malloc_ptr)]));
    slskprintf("\n");

  }
  return ((void *)(sls_malloc_ptr - size));
}

void S_FREE(void *ptr) {
  wasted_malloc_space += *(((int *)ptr) - 1);
}

void *S_REALLOC(void *ptr, size_t size) {
  int cur_size;
  int cvp;
  if ((size % 4) != 0) {
    size += 4 - (size %4);}
  cur_size = *(((int *)ptr) - 1);
  if (cur_size >= size) 
    return (ptr);
  cvp = PGNO(sls_malloc_ptr);
  *((int *)sls_malloc_ptr) = size;
  sls_malloc_ptr += (size + sizeof(int));
  if (sls_malloc_ptr > END_TEMP_MALLOC_REGION) {
     slskprintf("Temp Realloc Failed\n");
    return (0); }
  S_memcpy ((void *)(sls_malloc_ptr - size), ptr, cur_size);
  wasted_malloc_space += cur_size;
  while (PGNO(sls_malloc_ptr) != cvp) {
    cvp++;
    sys_self_insert_pte(0,(0|PG_U|PG_W|PG_P),cvp<<PGSHIFT);
    slskprintf("::::::::::::::::::Mapped vp 0x");
    slskprinth(PGNO(sls_malloc_ptr));
    slskprintf("to to pp 0x");
    slskprinth(PGNO(vpt[PGNO(sls_malloc_ptr)]));
    slskprintf("\n");
  }
  return ((void *)(sls_malloc_ptr - size));
}

void *S_CALLOC(size_t n, size_t size) {
  void *t;
  t = S_MALLOC(n * size);
  S_BZERO(t,(n*size));
  return (t);
}

void S_BZERO(void *ptr, int n) {
  while (n > 4) {
    *((int *)ptr) = (int)(0x00000000);
    ptr += 4;
    n -= 4;}
  while (n > 1) {
    *((char *)(ptr)) = (char)(0x00);
    ptr++;
    n--;
  }
}

int S_OPEN (char *name, int flags, int perms) 
{
  int v, offset;
  int buff = 0;
  dprintf("==> S_OPEN : ");
  dprintf(name);
  dprintf("\n");
  
  offset = (int)name & 0x00000fff;
  if (PGNO(name) != PGNO(name + strlen(name))) {
    /*    printf("Error: Filename wraps across physical pages.\n"); */
    return (-1);
  }
  buff = (PGNO(vpt[PGNO(name)])<<PGSHIFT) + offset;
  /* buff = pointer to page with name in it ******************************/
  
  v = sls_sipcout(slseid,SLS_OPEN,buff, flags, perms);
  
  return (v);
}

int S_DUP (int fd) {
  dprintf("==> S_DUP\n");
  return (sls_sipcout(slseid,SLS_DUP,fd,0,0));
}

int S_DUP2 (int fd, int newfd) {
  dprintf("==> S_DUP2\n");
  return (sls_sipcout(slseid,SLS_DUP2,fd,newfd,0));
}

int S_LSEEK (int fd, int newloc, int mode) {
  int i;
  dprintf("==> S_LSEEK\n");
  i = (sls_sipcout(slseid,SLS_LSEEK,fd, newloc, mode));
  if (i == SLS_INV) {
    return (-1);}
  else {return (i);}
}

int S_READ (int fd, char *buf, int n)
{
  int npp, vpn, offset, v ,ppn, pa;
  int i, j, *k;
  dprintf("==> S_READ\n");
  offset = (u_long)buf & 0x00000fff;
  vpn = PGNO(buf);
  ppn = PGNO(vpt[vpn]);
  pa = (ppn<<PGSHIFT) + offset;
  npp = 1;
  slsprintf("CLIENT READ :Initial page : 0x");
  slsprinth(pa);
  slsprintf("\n");
  if (ppn != PGNO(vpt[PGNO(buf + n)])) {
    i = n - (NBPG - offset);
    while (i > 0) { npp++; i -= NBPG;}
    
    slsprintf("CLIENT READ :number of additional pages : ");
    slsprintd(npp); 
    slsprintf("\n");
    if ((npp * sizeof(int)) > NBPG - offset) {
      perror("S_READ : Cant fit list of ppns in remaining portion of initial pp\n");
      return (-1);}
    for (j = 1; j < npp; j++) {
      *((int *)(buf + (sizeof(int) * j ))) = vpt[vpn + j];}
  }

  k = (int *)buf;
  *k = 0x00000000 + npp;
  d2printf("Making IPC call for read\n");
  v = sls_sipcout(slseid,SLS_READ,fd,pa,n);
  slskprintf("Read ");
  slskprintd(v);
  slskprintf(" out of ");
  slskprintd(n);
  slskprintf("\n");
  return (v);
}

int S_WRITE (int fd, char *buf, int n)
{
  int npp, vpn, offset, v;
  int i, j, *k;
  dprintf("==> S_WRITE\n");
  offset = (int)buf & 0x00000fff;
  vpn = PGNO(buf);
  npp = 1;
  if (PGNO(vpt[vpn]) != PGNO(vpt[PGNO(buf + n)])) {
    i = n - (NBPG - offset);
    while (i > 0) { npp++; i -= NBPG;}
    if ((npp * sizeof(int)) > NBPG - offset) {
      perror("S_WRITE : Cant fit list of ppns in remaining portion of initial pp\n");
      return (-1);}
    for (j = 1; j < npp; j++) {
      *((int *)(buf + (sizeof(int) * j ))) = vpt[vpn + j];}
  }
  k = (int *)buf;
  *k = 0x00000000 + npp;
  v = sls_sipcout(slseid,SLS_WRITE,fd,((PGNO(vpt[vpn])<<PGSHIFT) + offset),n);
  return (v);
}

void S_CLOSE (int fd)
{
  dprintf("==> S_CLOSE\n");
  sls_sipcout(slseid,SLS_CLOSE,fd,0,0);
}

caddr_t S_MMAP (caddr_t addr, size_t len, int prot,
                 int flags, int fd, off_t offset)
{
  int v, pa;
  char *buf;
  caddr_t *ap;
  size_t *lp;
  int *pp;
  off_t *op;
  
  dprintf("==> S_MMAP\n");
  
  /* buf = pointer to page with addr, len, prot and offset **************/
  buf = (char *)malloc(sizeof(caddr_t) +
		       sizeof(size_t) +
		       sizeof(int) +
		       sizeof(off_t));

  /*set up pointers into buffer for data */
  ap = (caddr_t *)(buf);
  lp = (size_t *)(buf + sizeof(caddr_t));
  pp = (int *)(((char *)lp) + sizeof(size_t));
  op = (off_t *)(((char *)pp) + sizeof(int));


  *ap = addr;
  *lp = len;
  *pp = prot;
  *op = offset;
  pa = ((PGNO(vpt[PGNO((int)(buf))]))<<PGSHIFT) + ((int)buf & 0x00000fff);
 
  v = sls_sipcout(slseid,SLS_MMAP,pa,flags,fd);
  if (v == SLS_INV) {
    return ((char *)-1);
  } else {
    return ((caddr_t)v);
  }
}

int S_MPROTECT (caddr_t addr, size_t len, int prot)
{
  int v;
  dprintf("==> S_MPROTECT\n");
  v = sls_sipcout(slseid,SLS_MPROTECT,(int)addr, (int)len, prot);
  return(v);
}


int S_MUNMAP (caddr_t addr, size_t len)
{
  int v;
  dprintf("==> S_MUNMAP\n");
  v = sls_sipcout(slseid,SLS_MUNMAP,(int)addr,(int)len,0);
  return (v);
}

void S_PRINTD(int d, int i) {
  int v;
  v = sls_sipcout(slseid, SLS_PRINTD, d, i, 0);
}

void S_PRINTF(char *msg, int i) {
  int v      = (int)msg;
  int offset = v & 0x00000fff;
  int pp    = (PGNO(vpt[PGNO(v)])<<PGSHIFT) + offset;
  int pp1   = PGNO(vpt[PGNO(v) + 1])<<PGSHIFT;
  
  if      (i == -1) i = SLS_PERROR;
  else if (i ==  0) i = SLS_KPRINTF;
  else if (i ==  1) i = SLS_PRINTF;
  else              i = SLS_KPRINTF;
  if (strlen(msg) + (v % NBPG) > NBPG) { v = sls_sipcout(slseid, i, 1, pp, pp1);}
  
  else                                 { v = sls_sipcout(slseid, i, 0, pp, 0);   }
}



DIR *S_OPENDIR  (char *dname) {
  int v,ppn, offset;
  int buff = 0;
  struct dirent *dp;
  dprintf("==> S_OPENDIR : ");
  dprintf(dname);
  dprintf("\n");
  dp = (struct dirent *)malloc(2 * (sizeof(struct dirent)));
  offset = (int)dname & 0x00000fff;
  if (PGNO(dname) != PGNO(dname + strlen(dname))) {
    perror("Error: Directory name wraps across physical pages.\n");
    return (NULL);
  }
  ppn = PGNO(vpt[PGNO(dname)]);
  buff = (ppn<<PGSHIFT) + offset;

  /* buff = pointer to page with name in it ******************************/
  v = sls_sipcout(slseid,SLS_OPENDIR,buff,0,0);
  if (v < 0) {
    dprintf("===> Bad directory : ");
    dprintf(dname);
    dprintf("\n");
    return (NULL);}
  else {
    dp->d_fileno = v;
    return ((DIR *)dp);
  }
}

struct dirent *S_READDIR  (DIR *dp) {
  int vpn1, vpn2, vpn3, offset, v, ppn1, ppn2, ppn3;
  int i, n;
  struct dirent *buf1, *buf2;

  if (dp == NULL) {
    perror("S_READDIR : Attempt to read from non existent DIR\n");
    return (NULL);}
  buf1 = (struct dirent *)dp;
  buf2 = buf1 + 1;
  n = (sizeof(struct dirent));
  vpn1 =  PGNO(buf1);
  vpn2 =  PGNO(buf2);
  vpn3 =  PGNO(buf2 + 1);
  ppn1 = PGNO(vpt[vpn1]);
  ppn2 = PGNO(vpt[vpn2]);
  ppn3 = PGNO(vpt[vpn3]);
  i = ((struct dirent *)dp)->d_fileno;
  if (ppn2 == ppn3) {
    /* There is no page boundary in the second dirent so use it. */
    offset = (int)buf2 & 0x00000fff;
    v = sls_sipcout(slseid,SLS_READDIR,i,((ppn2<<PGSHIFT)+offset),n);
  }
  else {
    /* There is a page boundary in the second dirent.  So we use the first
       one and then move it to the second.  But we need to replace the
       d_fileno when we are done. */
    
    i = ((struct dirent *)dp)->d_fileno;
    offset = (int)buf1 & 0x00000fff;
    v = sls_sipcout(slseid,SLS_READDIR,i,((ppn1<<PGSHIFT)+offset),n);
    S_memcpy(buf2,buf1,sizeof(struct dirent));
    ((struct dirent *)dp)->d_fileno = i;
    
  }
  if ((v == 0) || (v == SLS_INV)) {
    dprintf("===> S_READDIR BAD ");
    dprintd(v);
    dprintf("\n");
    return (NULL);}
  else {
    return ((struct dirent *)dp + 1);
  }
}

void S_CLOSEDIR (DIR  *dp){
  int v;

  dprintf("==> S_CLOSEDIR\n");
  v = ((struct dirent *)dp)->d_fileno;
  v = sls_sipcout(slseid,SLS_CLOSEDIR,v,0,0);
  free(dp);
}

int S_STATUS (){
  int v;
  v= sls_sipcout(slseid,SLS_STATUS,0,0,0);
  d2printf("SLS_STATUS   : ");
  d2printd(v);
  d2printf("\n");
  return (v);
}

#include "d_string.c"
#include "d_env.c"

#undef dprintf
#undef d2printf
#undef dprintd
#undef d2printd
