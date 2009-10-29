
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <xok/sys_ucall.h>
#include <xok/pmap.h>
#include <xok/mmu.h>
#include <exos/cap.h>
#include <exos/vm.h>
#include <exos/mallocs.h>

int lkexec(char *path, void *loadaddr, int howto);

#define MAX_IMAGESIZE 0xa00000
char image[MAX_IMAGESIZE];
off_t imagesize = MAX_IMAGESIZE;

void tramp(char *c, off_t is) {

  /* copy the kernel to where it will run */
  /* copy: source in esi, destination in edi, size in ecx */
  asm("movl %ecx, %edx\n"
      "\tshrl $2, %ecx\n" /* copy by words */
      "\trep\n"
      "\tmovsl\n"
      "\tmovl %edx, %ecx\n"
      "\tandl $3, %ecx\n" /* any bytes left? */
      "\trep\n"
      "\tmovsb");

  /* disable paging */
  asm("movl %cr0, %eax");
  asm("andl $7FFFFFFF, %eax"); /* zero the paging enabled bit (bit 31) */
  asm("movl %eax, %cr0");

  /* Disable fancy features */
  asm("movl $0, %eax");
  asm(".byte 0xf,0x22,0xe0");  /* (old gcc/as doesn't recognize cr4)
				  asm("movl %eax, %cr4"); */

  /* Clear TLB */
  asm("movl $0, %eax");
  asm("movl %eax, %cr3");

  /* jump to image */
  asm("movl $0x100020, %eax");
  asm("jmpl %eax");
}

void end_tramp() {}

/* also change the last movl instruction in ring0 below */
#define PGVA 4096  

void ring0() {
  /* ensure interrupts are off */
  asm("cli");

  /* copy tramp to physical memory address */
  memcpy((void*)(PGVA+KERNBASE), tramp, (u_int)end_tramp - (u_int)tramp);

  /* prepare call to tramp */
  asm("movl %0, %%esi" :: "g" (image));
  asm("movl %0, %%edi" :: "g" (0x100000+KERNBASE));
  asm("movl %0, %%ecx" :: "g" (imagesize));

  /* jump to the tramp which will disable paging and jump to image */
  asm("movl $4096, %eax");
  asm("jmpl %eax");
}

void loadkern(char *fn) {
  u_int i;

  /* touch all text/data/bss pages so that nothing is missing once we go to
     ring 0.  And make the data pages writeable. */
   {
     volatile char c;
     extern int end;
     extern int etext;
     for (i=PGROUNDDOWN(UTEXT); i < PGROUNDUP(((u_int)&end)-1); i += NBPG) {
       c = *(char*)i;
       if (i >= PGROUNDDOWN((u_int)&etext)) *(volatile char*)i = c;
     }
   }

  /* read in the image */
  if (lkexec(fn, image, 0) == -1) {
    perror("loadkern");
    exit(-1);
  }

  /* make sure the page isn't holding a buffer so that we can 
     map it. */
  assert (__ppages[PGNO(PGVA)].pp_buf == NULL);
  assert (__ppages[PGNO(PGVA)].pp_status == PP_IOMEM);

  /* make sure tramp page is "identity mapped" */
  assert(_exos_self_insert_pte(CAP_ROOT, (PGNO(PGVA) << 12)|PG_P|PG_U|PG_W,
			       PGVA, ESIP_DONTPAGE, NULL) == 0);

  /* need to make sure that none of the pages in the image are located
     near where the kernel loads, or else the source and destination
     could overlap in some pages */
  for (i=PGNO((u_int)image); i < PGNO(PGROUNDUP((u_int)image+imagesize-1));
       i++) {
    u_int copy=0;
    while (PGNO(vpt[i]) < 0x400) { /* less than 4 meg physical page */
      /* then get a different page */
      u_int temppage = (u_int)__malloc(NBPG);
      if (_exos_self_insert_pte(CAP_ROOT, PG_P|PG_U|PG_W, temppage,
				ESIP_DONTPAGE, NULL) != 0) {
	fprintf(stderr, "Out of Pages!");
	return;
      }
      copy = temppage;
    }
    if (copy) bcopy((void*)((u_int)image+i*NBPG), (void*)copy, NBPG);
  }

#ifdef __SMP__
  /* shutdown other processors */
  sys_mp_shutdown();
#endif

  /* gain ring 0 */
  sys_ring0(CAP_ROOT, ring0);
}
