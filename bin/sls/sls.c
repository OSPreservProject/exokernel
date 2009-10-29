
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
#include <exos/cap.h>
#include <exos/critical.h>
#include <exos/mallocs.h>
#include <exos/ipcdemux.h>
#include <exos/kprintf.h>
#include <exos/nameserv.h>
#include <exos/osdecl.h>
#include <exos/process.h>
#include <exos/sls.h>
#include <exos/ubc.h>
#include <exos/uwk.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <fcntl.h>
#include <fd/nfs/nfs_proc.h>	/* for nfs_bmap_read */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <xok/env.h>
#include <xok/mmu.h>
// #include <xok/pmap.h>
#include <xok/sys_ucall.h>

#define MAX_OPEN 64
#define VPBIN_SIZE 4

#define VERS "v1.2.1"

#define dprintf kprintf
#define d2printf if (0) kprintf


int vpbin[VPBIN_SIZE];

struct sfd {
  int dup;
  char *name;
  int fd;
  int offset;
  int dev;
  int owner;
  int dir;
  DIR *dd;
} openlist[MAX_OPEN];

int lastopened;

int request (int code, int req, int a1, int a2, int a3, int caller);
int sls_read     (int, int, int, int, int);
int sls_lseek     (int, int, int, int, int);
int sls_mmap     (int, int, int, int, int);
int sls_mprotect (int, int, int, int, int);
int sls_munmap   (int, int, int, int, int);
int sls_dup      (int, int, int, int, int);
int sls_dup2     (int, int, int, int, int);
int sls_opendir  (int, int, int, int, int);
int sls_readdir  (int, int, int, int, int);
int sls_closedir (int, int, int, int, int);
int sls_verify   (int, int, int, int, int);
int sls_status   (int, int, int, int, int);
int sls_printf   (int, int, int, int, int, int);
int sls_printd   (int, int, int, int, int);
int clean_up ();


void main () {
  int v, i;
  
  printf("Starting Shared Library Server...\n");

  /* Initialize the open file structure list */
  for (v = 0; v < MAX_OPEN; v++) {
    openlist[v].dup = -1;
    openlist[v].name = NULL;
    openlist[v].owner = 0;
    openlist[v].dir = 0;
  }
  lastopened = MAX_OPEN - 1;
  
  /* Grab VPBIN_SIZE virtual address pages for use in buffer transactions */
  v = (int)__malloc(NBPG * (VPBIN_SIZE + 1));
  for (i = 0; i < VPBIN_SIZE; i++) {
    vpbin[i] = (PGNO(v) + i)<<PGSHIFT;}

  if (ipcdemux_register(IPC_SLS, request, 6) < 0) {
    assert(0);
    /* XXX */
  }
  
  v = nameserver_remove(NS_NM_SLS);
  v = nameserver_register(NS_NM_SLS, __envid);
  v = 0;
  
  while (1) {
    sleep(1);
    v++;
    if (v > 30) {
      v = clean_up(); 
      if (v > 0) {
	kprintf("  sls: removed %d bindings on scheduled clean_up.\n",v);
      }
      v = 0;
    }
  }
  assert (0);
}




/***********************************************************************/
/*                              Open                                   */
/***********************************************************************/


int
sls_open (u_long sname, int flags, mode_t mode, int caller)
{
  int ret = 0;
  int i, j;
  char name[_POSIX_PATH_MAX];
  int lfd;
  struct stat sb;

  if (sys_vcopyin(caller, sname, name, _POSIX_PATH_MAX) < 0) {
    assert(0);
    /* XXX */
  }

  lfd = open(name, flags, mode);
  if (lfd == -1) {
    assert(0);
    /* XXX */
    ret = SLS_INV;
    goto out;
  }

  if (fstat(lfd, &sb) < 0) {
    assert(0);
  }

  for (i = 0; i < MAX_OPEN; i++) {
    j = (i + lastopened + 1) % MAX_OPEN;
    if (openlist[j].name == NULL) {
      openlist[j].name = strdup(name);
      openlist[j].owner = caller; 
      openlist[j].dir = 0;
      openlist[j].fd = lfd;
      openlist[j].offset = 0;
      openlist[j].dev = sb.st_dev;
      ret = j;
      lastopened = j;
      dprintf("  sls: open %d [%s]\n",j,name);
      goto out; 
    }
  } 
  ret = SLS_INV;
  kprintf ("sls: open file table full on open\n");
  close(lfd);
  goto out;
out:
  return (ret);
}


/***********************************************************************/
/*                              Close                                  */
/***********************************************************************/

int
sls_close (int fd, int caller)
{
  int ret, i, j, new_parent;

  d2printf ("  sls: close request [%d]\n", fd);
  if (openlist[fd].name == NULL) {
    ret = SLS_INV;
    kprintf ("sls: attempt to close non-existente file\n");
    goto out;}

  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to close a directory fd entry\n");
    ret = SLS_INV;
    goto out;
  }

  if (openlist[fd].dup > -1) {
    if (openlist[openlist[fd].dup].owner != caller) {
      kprintf ("sls: non-owner attempted to close a file\n");
      ret = SLS_INV;
      goto out;
    } else {
      openlist[fd].name = NULL;
      openlist[fd].dup = -1;
      ret = SLS_OK;
      goto out;
    }
  }

  if (openlist[fd].owner != caller) {
    kprintf ("sls: non-owner attempted to close a file\n");
    ret = SLS_INV;
    goto out;
  }

  new_parent = -1;

  if (openlist[fd].dup == -2) {
    /* Other fd's refer to this fd so we need to make sure
       that one of them gets the real data and the rest
       point to it, in addition to not actually closing
       the file */
    for (i = 1; i < MAX_OPEN; i++) {
      j = (i + fd) % MAX_OPEN;
      if (openlist[j].dup == fd) {
	if (new_parent == -1) {
	  /* This is the first dependency, so it becomes the 'new
	     original' (Not the Thamesmen ;) ) */
	  openlist[j].name = openlist[fd].name;
	  openlist[j].fd = openlist[fd].fd;
	  openlist[j].owner = openlist[fd].owner;
	  openlist[j].offset = openlist[fd].offset;
	  openlist[j].dup = -1;
	  new_parent = j;}
	else {
	  openlist[j].dup = new_parent;
	  openlist[new_parent].dup = -2;
	}
      }
    }
  }
  openlist[fd].name = NULL;
  openlist[fd].owner = 0;

  if (new_parent == -1) {   
    /* There are no fd dependencies so we can actually
       close the file and free the filename mem. */
    free(openlist[fd].name);
    close(openlist[fd].fd);
  }

  ret = SLS_OK;

out:
  return (ret);
}

/* sls vcopy destination */
struct vc_info {
  struct {
    int fd;
    u_quad_t blk;
  } to;
  struct {
    int dev;
    u_quad_t blk;
  } from;
};

int
sls_readinsert(u_long vcopyinfo, u_int caller)
{
  struct vc_info fdoff;
  u_quad_t pblock;
  u_int seq;
  struct bc_entry *b;
  int dev, lfd, ret, done=0;

  if (sys_vcopyin(caller, vcopyinfo, &fdoff, sizeof(fdoff)) < 0) {
    assert(0);
  }

  lfd = openlist[fdoff.to.fd].fd;
  dev = openlist[fdoff.to.fd].dev;
  ret = bmap(lfd, &pblock, fdoff.to.blk, &seq);
  assert(ret == 0);
  assert(seq >= 0);
  fdoff.from.dev = dev;
  fdoff.from.blk = pblock;
  seq = 1;

  /* check if the block is in the buffer cache */
  while (!(b = __bc_lookup64(dev, pblock))) {
    if (dev >= 0) {
      /* disk device */
      /* sys_bc_read_and_insert returns -E_EXISTS if *any* of the 
	 requested blocks are in the cache... */
      ret = sys_bc_read_and_insert(dev, (unsigned int)pblock, 1, &done);
      if (ret == 0)
	/* sleep until request is completed... */
	wk_waitfor_value_neq(&done, 0, 0);
      else if (ret < 0 && ret != -E_EXISTS) {
	kprintf ("sys_bc_read_and_insert in mmap returned %d\n", ret);
	panic ("mmap: error reading in block\n");
      }
    } else {
      /* nfs device */
      if (nfs_bmap_read(lfd, pblock) < 0)
	panic ("mmap: error reading block from nfs\n");
    }
  }

  if (sys_vcopyout(&fdoff, caller, vcopyinfo, sizeof(fdoff)) < 0) {
    assert(0);
  }

  return 0;
}

int
sls_newpage(u_int vpte, u_int caller)
{
  if (sys_insert_pte(CAP_ROOT, vpte & PGMASK, PGROUNDDOWN(vpte), CAP_ROOT,
		     caller) < 0) {
    assert(0);
  }
  return 0;
}

/***********************************************************************/
/*                      Dup, Dup2 and Fseek                            */
/***********************************************************************/

int
sls_dup (int req, int a1, int a2, int a3, int caller)
{
  int i,j, ret;

  int fd;

  fd = a1;
  if (openlist[a1].dup > -1) { fd = openlist[a1].dup;}


  if (openlist[fd].name == NULL) {
    kprintf("sls: attempt to dup a non-existent file descriptor\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[fd].owner != caller) {
    kprintf("sls: non-owner request to dup file descriptor\n");
    ret = SLS_INV;
    goto out;}
  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to dup a directory fd entry\n");
    ret = SLS_INV;
    goto out;}

  for (i = 0; i < MAX_OPEN; i++) {
    j = (i + lastopened + 1) % MAX_OPEN;
    if (openlist[j].name == NULL) {
      openlist[j].name = openlist[a1].name;
      openlist[j].dup = fd;
      openlist[j].dir = 0;
      /* We don't want to have to deal with chaining fd duplications */
      if (openlist[fd].dup > -1) {
	openlist[j].dup = openlist[fd].dup;}
      /* Mark the referenced fd as referenced to speed closing */
      openlist[openlist[j].dup].dup = -2;
      ret = j;

      dprintf("  sls: dup on fd %d\n",fd);

      goto out;
    }
  }
  ret = SLS_INV;
  kprintf("sls: open file table full on dup\n");
out:
  return (ret);
}



int
sls_dup2 (int req, int a1, int a2, int a3, int caller)
{
  int ret;
  int fd;

  fd = a1;
  if (openlist[a1].dup > -1) { fd = openlist[a1].dup;}

  if (openlist[fd].name == NULL) {
    kprintf("sls: attempt to dup a non-existent file descriptor\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to dup a directory fd entry\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[fd].owner != caller) {
    kprintf("sls: non-owner request to dup file descriptor\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[a2].name != NULL) {
    ret = sls_close(a2,caller);
    if (ret == SLS_INV) {
      kprintf("sls: dup2 couldn't close existing fd\n");
      goto out;}
  }
  openlist[a2].name = openlist[a1].name;
  openlist[a2].dup = fd;
  /* We don't want to have to deal with chaining fd duplications */
  if (openlist[fd].dup > -1) {
    openlist[a2].dup = openlist[fd].dup;}
  /* Mark the referenced fd as referenced to speed closing */
  openlist[openlist[a1].dup].dup = -2;
  ret = a2;
  goto out;

  dprintf("  sls: dup2 on fd %d to %d\n",fd,a2);

out:
  return (ret);
}


int sls_lseek (int req, int a1, int a2, int a3, int caller) {
  int fd,ret;

  fd = a1;
  if (openlist[a1].dup > -1) { fd = openlist[a1].dup;}

  if (openlist[fd].name == NULL) {
    kprintf("sls: attempt to fseek a non-existent file descriptor\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to fseek a directory fd entry\n");
    ret = SLS_INV;
    goto out;}

  if (openlist[fd].owner != caller) {
    kprintf("sls: non-owner request to fseek file descriptor\n");
    ret = SLS_INV;
    goto out;}
  ret = lseek(openlist[fd].fd,a2,a3);
out:
  return (ret);
}

/***********************************************************************/
/*                              Read                                  */
/***********************************************************************/

int
sls_read (int req, int a1, int a2, int a3, int caller)
{
  int i,j, jj, ret, *k;
  int ppn, *pplist, offset;
  int bytes_left, bytes_consumed,gobble, yum;
  int fd;

  fd = a1;

  
  if (openlist[fd].name == NULL) {
    ret = SLS_INV;
    kprintf ("sls: attempt to read from a non-existent file\n");
    goto out;}

  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to read from a directory fd entry\n");
    ret = SLS_INV;
    goto out;}

  /* If the fd is a duplicate, find the original */
  if (openlist[fd].dup > -1) {
    fd = openlist[fd].dup;}


  if (openlist[fd].owner != caller) {
    ret = SLS_INV;
    kprintf("sls: non-owner attempted to read from file\n");
    goto out;}
  
  
  /* Map the first physical page of the buffer into the server's
     virtual address space */
  
  offset = a2 & 0x00000fff;
  ppn = PGNO(a2);
  jj = sys_self_insert_pte(0,((ppn<<PGSHIFT)|PG_W|PG_U|PG_P),vpbin[0]);
  k = (int *)(vpbin[0] + offset);
  i = *k;
  pplist = NULL;
  
  /*If the value at the head of the buffer is > 1 then the buffer
    is larger than one page, so we need to pull out the id's of
    the other physical pages before we munge them by reading
    into the buffer. */
   if (i > 1) {
    pplist = malloc(i * sizeof(int));
    for (j = 1; j < i; j++) {
      pplist[j] = *(int *)(vpbin[0] + offset + (j * sizeof(int)));
      printf("SLS READ :Additional page : 0x%x\n",(int)pplist[j]);

    }
  }
  
  /* Read in the first chunk of the file.  The size is the minimum
     of the total size of the read and the space left in the first
     physical page. */
  
  bytes_consumed = NBPG - offset;
  if (bytes_consumed > a3) { bytes_consumed = a3;}
  ret = read(openlist[fd].fd,k,bytes_consumed);
  if (ret != bytes_consumed) {
    goto out;}
  
  
  /*If the buffer is big enough that it lies on multiple pages,
    we need to read in from the file for each one.  Since we
    don't allocate a contiguous block of virtual memory for this
    we do it in a loop of reads, one read per page. */
  
  if (i > 1) {
    bytes_left = a3 - bytes_consumed;
    j = 1;
    while (bytes_left > 0) {
      ppn = PGNO(pplist[j]);
      sys_self_insert_pte(0,((ppn<<PGSHIFT)|PG_W|PG_U|PG_P),vpbin[0]);
      if (bytes_left > NBPG) {
	gobble =  NBPG;}
      else {
	gobble = bytes_left;}
      yum = read(openlist[fd].fd,(int *)vpbin[0],gobble);
      if (yum < gobble) {
	ret = bytes_consumed + yum;
	goto out;}
      bytes_consumed += yum;
      bytes_left -= yum;
      j++;}
    free (pplist);
  }
  ret = bytes_consumed;
  goto out;

out:
    return (ret);
}

/***********************************************************************/
/*                   mmap, mprotect and munmap                         */
/***********************************************************************/

int sls_mmap (int req, int pa, int flags, int fd, int caller) {
  int ret;
  char *buf;
  caddr_t *ap;
  size_t *lp;
  int *pp;
  off_t *op;
  int self;



  dprintf ("  sls: mmap request\n");

  self = __envid;

  if (openlist[fd].dir != 0) {
    kprintf("sls: attempt to mmap from a directory fd entry\n");
    ret = SLS_INV;
    goto out;}
  


  sys_self_insert_pte(0,((PGNO(pa)<<PGSHIFT)|PG_W|PG_U|PG_P),vpbin[0]);
  buf = (char *)(vpbin[0] + (pa & 0x00000fff));


  /*set up pointers into buffer for data */
  ap = (caddr_t *)(buf);
  lp = (size_t *)(buf + sizeof(caddr_t));
  pp = (int *)(((char *)lp) + sizeof(size_t));
  op = (off_t *)(((char *)pp) + sizeof(int));

  ret = (int)(__mmap((*ap), *lp, *pp, flags, openlist[fd].fd, *op, 0, caller));
  if (ret == -1) { ret = SLS_INV;}

out:
  return (ret);
}

int sls_mprotect (int req, int addr, int len, int prot, int caller) {
  int ret;
  printf("Mprotecting 0x%x to 0x%x with %x for caller %d\n",
	 addr, addr + len, prot, caller);
  ret = __mprotect((caddr_t)addr, (size_t)len, prot, 0, caller);
  if (ret == -1) 
    printf("FAILED : ERRNO = %d\n",errno);
  dprintf ("sls: mprotect request\n");

  return (ret);
}

int sls_munmap (int req, int a1, int a2, int a3, int caller) {
  int ret;
  ret = SLS_OK;
  kprintf ("sls: munmap request - ignored\n");
  return (ret);
}



/***********************************************************************/
/*                 opendir, readdir and closedir                       */
/***********************************************************************/

int sls_opendir (int req, int a1, int a2, int a3, int caller) {
  int i,j, ret,pp;
  int ppn, offset;
  char *name;

  offset = a1 & 0x00000fff;
  ppn = PGNO(a1);

  sys_self_insert_pte(0,((ppn<<PGSHIFT)|PG_U|PG_P),vpbin[0]);
  pp = PGNO(vpt[PGNO(vpbin[0])]);

  name = (char *)(vpbin[0] + offset);

  for (i = 0; i < MAX_OPEN; i++) {
    j = (i + lastopened + 1) % MAX_OPEN;
    if (openlist[j].name == NULL) {
      openlist[j].dd = opendir(name);
      if (openlist[j].dd == NULL) {
	ret = SLS_INV;
	goto out;}
      openlist[j].name = malloc(sizeof(char) * strlen(name) + 1);
      strcpy(openlist[j].name,name);
      openlist[j].owner = caller;
      openlist[j].dir = 1;
      openlist[j].offset = 0;
      ret = j;
      dprintf ("  sls: opendir request : %d [%s]\n",j,name);
      lastopened = j;
      goto out;
    }
  }
  ret = SLS_INV;
  kprintf ("sls : open file table full on open\n");
  goto out;
out:
  return (ret);
}



int sls_readdir (int req, int a1, int a2, int a3, int caller) {
  int ret, *k;
  int ppn, offset;
  int fd;
  struct dirent *de;

  fd = a1;
  
  if (openlist[fd].name == NULL) {
    ret = SLS_INV;
    kprintf ("sls: attempt to read from a non-existent directory\n");
    goto out;}

  if (openlist[fd].dir == 0) {
    kprintf("sls: attempt to readdir from a file fd entry\n");
    ret = SLS_INV;
    goto out;}


  if (openlist[fd].owner != caller) {
    ret = SLS_INV;
    kprintf("sls: non-owner attempted to read from directory\n");
    goto out;}
  

  
  /* Map the first physical page of the buffer into the server's
     virtual address space */
  
  offset = a2 & 0x00000fff;
  ppn = PGNO(a2);
  sys_self_insert_pte(0,((ppn<<PGSHIFT)|PG_W|PG_U|PG_P),vpbin[0]);
  k = (int *)(vpbin[0] + offset);
  de = readdir(openlist[fd].dd);
  if (de == NULL) { return (SLS_INV);}
  *((struct dirent *)k) = *(de);
out:
  return (SLS_OK);
}

int sls_closedir (int req, int a1, int a2, int a3, int caller) {
    int ret;
    

  d2printf("  sls : closedir : %d [%s]\n",a1,openlist[a1].name);
  if (openlist[a1].name == NULL) {
    ret = SLS_INV;
    kprintf ("sls: attempt to close non-existent DIR\n");
    goto out;}

  if (openlist[a1].dir == 0) {
    kprintf("sls: attempt to closedir a file fd entry\n");
    ret = SLS_INV;
    goto out;}

  openlist[a1].name = NULL;
  openlist[a1].owner = 0;

  free(openlist[a1].name);

  closedir (openlist[a1].dd);

  ret = SLS_OK;
  
out:
  return (ret);
}


/***********************************************************************/
/*                 Cleanup the list of open files                      */
/***********************************************************************/

int sls_verify (int req, int a1, int a2, int a3, int caller) {
  int ret;
  printf ("sls: verify request\n");
  ret = clean_up();
  return (ret);
}


int clean_up () {
  int i, j, r;
  j = 0;
  for (i = 0; i < MAX_OPEN; i++) {
    if (openlist[i].owner) {
      if (__envs[envidx(openlist[i].owner)].env_status != ENV_OK) {
	r = sls_close(i, openlist[i].owner);
	if (r == SLS_INV) {
	  kprintf("sls: internal error, couldn't close dead file %d\n",i);
	}
	else {
	  j++;}
      }
    }
  }
  return j;
}

int sls_printf (int req, int a1, int a2, int a3, int caller, int notk) {
  char *msg;
  sys_self_insert_pte(0,((PGNO(a2)<<PGSHIFT)|PG_U|PG_P),vpbin[0]);
  if (a1 == 1) {
    sys_self_insert_pte(0,((PGNO(a3)<<PGSHIFT)|PG_U|PG_P),vpbin[1]);
  }
  msg = (char *)(vpbin[0] + (a2 & 0x00000fff));
  if (notk == 1) {
    printf("%s",msg);}
  else if (notk == -1) {
    printf("SLS_STDERR : %s\n",msg);}
  else {
    kprintf("%s",msg);}
  return(1);
}

int sls_printd (int req, int a1, int a2, int a3, int caller) {
  if (a2 == 0) {
    kprintf("%d",a1);}
  else if (a2 == 1) {
    printf("%d",a1);}
  else if (a2 == 2) {
    kprintf("%x",a1);}
  else if (a2 == 3) {
    printf("%x",a1);}
  else printf("*bad_mode*\n");

  return(1);
}

int sls_status (int req, int a1, int a2, int a3, int caller) {
  int i,j;
  j = 0;
  kprintf("Current status:\n");
  for (i = 0; i < MAX_OPEN; i++) {
    if (openlist[i].name != NULL) {
      j++;
      if (openlist[i].dup < 0) {
	kprintf ("  %d\t%s\t%d\n",i,openlist[i].name,openlist[i].fd);}
      else {
	kprintf("  %d\t%s\t(%d <- %d)\n",i,openlist[i].name,openlist[openlist[i].dup].fd,openlist[i].dup);}
    }
  }
  return (j);
}

int request (int code, int req, int a1, int a2, int a3, int caller) {
  int ret;

  switch (req) {
  case SLS_OPEN:
    ret = sls_open((u_long)a1, (int)a2, (mode_t)a3, caller);
    break;
  case SLS_CLOSE:
    ret = sls_close((int)a1, caller);
    break;
  case SLS_READINSERT:
    ret = sls_readinsert(a1, caller);
    break;
  case SLS_NEWPAGE:
    ret = sls_newpage((u_int)a1, caller);
    break;
  case SLS_READ:      ret = sls_read     (req,a1,a2,a3,caller); break;
  case SLS_LSEEK:     ret = sls_lseek    (req,a1,a2,a3,caller); break;
  case SLS_MMAP:      ret = sls_mmap     (req,a1,a2,a3,caller); break;
  case SLS_MPROTECT:  ret = sls_mprotect (req,a1,a2,a3,caller); break;
  case SLS_MUNMAP:    ret = sls_munmap   (req,a1,a2,a3,caller); break;
  case SLS_DUP:       ret = sls_dup      (req,a1,a2,a3,caller); break;
  case SLS_DUP2:      ret = sls_dup2     (req,a1,a2,a3,caller); break;
  case SLS_OPENDIR:   ret = sls_opendir  (req,a1,a2,a3,caller); break;
  case SLS_READDIR:   ret = sls_readdir  (req,a1,a2,a3,caller); break;
  case SLS_CLOSEDIR:  ret = sls_closedir (req,a1,a2,a3,caller); break;
  case SLS_VERIFY:    ret = sls_verify   (req,a1,a2,a3,caller); break;
  case SLS_PERROR:    ret = sls_printf   (req,a1,a2,a3,caller,-1); break;
  case SLS_PRINTF:    ret = sls_printf   (req,a1,a2,a3,caller,1); break;
  case SLS_KPRINTF:   ret = sls_printf   (req,a1,a2,a3,caller,0); break;
  case SLS_PRINTD:    ret = sls_printd   (req,a1,a2,a3,caller); break;
  case SLS_STATUS:    ret = sls_status   (req,a1,a2,a2,caller); break;
  default: 
    kprintf ("sls: received bogus request (%d) from env %d\n",req, caller);
    ret = SLS_INV;
  }
  
  return (ret);
}
