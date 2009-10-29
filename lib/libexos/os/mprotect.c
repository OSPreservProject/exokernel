
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

#include <exos/callcount.h>
#include <exos/conf.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <exos/osdecl.h>
#include <exos/vm.h>
#include <errno.h>

/* used when you want to mprotect memory in another address space */
int __mprotect (void *addr, size_t len, int prot, int k, int envid) {
     u_int add = 0, remove = 0;
     u_int pages;

      if (len < 0) {
	  errno = EINVAL;	/* not sure about this. ENOMEM instead? */
	  return (-1);
     }

     /* make sure page aligned. Hope pagesize never changes... */
     if (((unsigned )addr & 0xfff) != 0) { 
	  errno = EINVAL;
	  return (-1);
     }

     if (prot & PROT_READ) {
       add = PG_P | PG_RO;
       remove = PG_W;
     }
     if (prot & PROT_WRITE) {
       add = (PG_W | PG_P);
       remove = PG_RO;
     }
     if (!prot) {
       add = 0;
       remove = ~0;
     }
     if (prot & PROT_EXEC) {
       errno = EINVAL;
       return (-1);
     }

     pages = len / NBPG;
     if (pages * NBPG < len) {
       pages++;
     }
     
     if (sys_mod_pte_range (0, add, remove, (unsigned)addr, pages, k,
			    envid) < 0) {
       errno = ENOMEM;
       return (-1);
     }

     return (0);
}

/* mprotect system call.

  Set the permissions on a region of memory. 

  XXX -- no PROT_EXEC
*/

int mprotect (void *addr, size_t len, int prot) {
  int r;

  OSCALLENTER(OSCALL_mprotect);
  r = __mprotect (addr, len, prot, 0, __envid);
  OSCALLEXIT(OSCALL_mprotect);
  return r;
}
