
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

#include <errno.h>
#include <exos/brk.h>
#include <exos/callcount.h>
#include <exos/magic.h>
#include <exos/osdecl.h>
#include <exos/uidt.h>
#include <fcntl.h>
#include <machine/vmparam.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <xok/env.h>
#include <xok/mmu.h>
#include <xok/wk.h>
#include "emubrk.h"
#include "handler.h"

u_int __os_calls = 0;
u_int __os_call_no = 0;
u_int __os_call_count_arr[256];
#ifdef KERN_CALL_COUNT
u_int *__os_kern_call_count_arr[256];
#endif

int dlevel = 0;

/* From /usr/src/lib/csu/i386/crt0.c */
struct kframe {
  int     kargc;
  char    **kargv;      /* size depends on kargc */
  char    *kargstr;     /* size varies */
  char    *kenvstr;     /* size varies */
};

extern int asm_start_386bsd;
extern int asm_start;
extern int asm_stop;
extern int asm_start2;
extern int asm_stop2;

void
itest(void)
{
  init_handlers();

  uidt_register_sfunc(0x80, (u_int)&asm_start);
  uidt_register_sfunc(13, (u_int)&asm_start_386bsd);
}

void
itest2(void)
{
  uidt_register_sfunc(0x80, (u_int)&asm_start2);
}

void
move_ash_region(void)
{
  u_int old_epilogue = UAREA.u_entepilogue;
  //  u_int NewAshLoc = 0x3000000;
  // u_int envid = geteid();
  //  u_int k = 0;

  UAREA.u_entepilogue = (u_int) &&ash_fork_epilogue;
  //  sys_env_ashuva(k,envid,NewAshLoc);

  UAREA.u_entepilogue = old_epilogue;


  return;

ash_fork_epilogue:
  //  sys_env_ashuva(k,envid,NewAshLoc);

  UAREA.u_entepilogue = old_epilogue;
  asm ("jmp %0" :: "r" (old_epilogue));
}

u_int gloc, gargc, genvc, gi;
char **genviron, **gargv;

int
main(int argc, char **argv, char **environ)
{
  int f, envc = 0,aesize;
  struct exec hdr;
  caddr_t base, base2;
  long unsigned entry=0;
  char **pc;
  int showusage = 0;
  u_int magic;
  int res, bin_format, bin_os;
  char *fnamep;
  u_int daddr, baddr, bsize;

  {
    int i;

    for (i=0; i < 256; i++)
      {
#ifdef KERN_CALL_COUNT
	int j;

	__os_kern_call_count_arr[i] = malloc(sizeof(int) * 256);
	for (j=0; j < 256; j++)
	  __os_kern_call_count_arr[i][j] = 0;
#endif
	__os_call_count_arr[i] = 0;
      }
  }

//  move_ash_region();

  if (argc <= 1)
    showusage = 1;
  else if (argc == 2)
    dlevel = 0;
  else if (!strcmp("-d1",argv[1]))
    dlevel = 1;
  else if (!strcmp("-d2",argv[1]))
    dlevel = 2;
  else if (!strcmp("-d3",argv[1]))
    dlevel = 3;
  else if (!strcmp("-d4",argv[1]))
    dlevel = 4;
  if (showusage)
    {
      printf("Usage: %s [options] <binary filename> "
	     "[<command line arguments for binary>]\n",argv[0]);
      printf("\noptions: (one max)\n"
	     "       -d1 - loading msgs stdout\n"
	     "       -d2 - syscall #'s to console\n"
	     "       -d3 - syscall #'s to log file: emulate.log\n"
	     "       -d4 - more\n\n");
      exit(0);
    }
  if (dlevel > 0)
    {
      fnamep = argv[2];
      argc--;
      argv = &argv[1];
    }
  else
    fnamep = argv[1];

  itest();
  /*  itest2(); */

  f = open(fnamep, O_RDONLY, 0);
  if (!f) {
    printf("Error opening file: %s\n",fnamep);
    exit(0);
  }

  if (dlevel > 0) printf("Reading magic number.\n");
  res = read(f, &magic, sizeof (magic));

  if (res == -1) {
    printf("Error reading from file %s: (%d)%s\n", fnamep, errno,
	   strerror(errno));
    exit(0);
  }

  if (lseek(f,0,SEEK_SET) != 0) {
      printf("Error lseeking in file %s: (%d)%s\n", fnamep, errno,
	     strerror(errno));
      exit(0);
    }

  if (res != sizeof(magic)) {
    printf("File too short to determine type: %s\n", fnamep);
    exit(0);
  }

  if (dlevel > 0) printf("MAGIC: %d 0x%x 0%o\n", magic, magic, magic);

  bin_os = check_magic(magic, &bin_format);

  switch (bin_os) {
  case BF_UNKNOWN:
    printf("File %s has unknown magic number 0%o\n", fnamep, magic);
    exit(0);

  case BF_AOUT_386BSD:
    if (dlevel > 0) printf("binary format = 386BSD\n");
    res = read(f, &hdr, sizeof (hdr));
    if (res == -1) {
      printf("Error reading from file %s: (%d)%s\n", fnamep, errno,
	     strerror(errno));
      exit(0);
    } else if (res != sizeof(hdr)) {
      printf("Error reading from file %s\n", fnamep);
      exit(0);
    }

    switch (bin_format) {
    case BF_OMAGIC:
      if (dlevel > 0) printf("mmaping OMAGIC file\n");
      exit(-1);

    case BF_NMAGIC:
      if (dlevel > 0) printf("mmaping NMAGIC file\n");
      exit(-1);

    case BF_QMAGIC:
      if (dlevel > 0) printf("mmaping QMAGIC file\n");

      if (dlevel > 0) printf("mmaping text segment\n");
      base = mmap((caddr_t)0x1000, hdr.a_text, PROT_READ | PROT_EXEC, 
		  MAP_FIXED | MAP_PRIVATE, f, 0);
      if ((int)base == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      if (dlevel > 0) printf("mmaping data segment\n");
      base2 = mmap((caddr_t)(base+hdr.a_text), hdr.a_data, PROT_READ | 
		   PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE, f, 
		   hdr.a_text);
      if ((int)base2 == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      if (dlevel > 0) printf("mmapping bss %x %x\n",
			     (u_int)(base + hdr.a_text + hdr.a_data),
			     (u_int)hdr.a_bss);
      if (hdr.a_bss && (int)mmap(base + hdr.a_text + hdr.a_data, hdr.a_bss,
				 PROT_READ | PROT_WRITE | PROT_EXEC,
				 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0)
	  == -1) {
	printf("Mapping error(b): %d\n", errno);
	exit(0);
      }

      emu_brkpt = (u_int)base + hdr.a_text + hdr.a_data + hdr.a_bss;
      entry = hdr.a_entry;
      break;

    case BF_ZMAGIC:
      if (dlevel > 0) printf("mmaping ZMAGIC file\n");

      if (dlevel > 0) printf("mmaping text segment\n");
      base = mmap((caddr_t)0x0, hdr.a_text, PROT_READ | PROT_EXEC, 
		  MAP_FIXED | MAP_PRIVATE, f, NBPG);
      if ((int)base == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      if (dlevel > 0) printf("mmaping data segment\n");
      base2 = mmap((caddr_t)(base+hdr.a_text), hdr.a_data, PROT_READ | 
		   PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE, f, 
		   hdr.a_text + NBPG);
      if ((int)base2 == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      if (dlevel > 0) printf("mmaping bss %x %x\n",
			     (u_int)(base + hdr.a_text + hdr.a_data),
			     (u_int)hdr.a_bss);
      if (hdr.a_bss && (int)mmap(base + hdr.a_text + hdr.a_data, hdr.a_bss,
				 PROT_READ | PROT_WRITE | PROT_EXEC,
				 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0)
	  == -1) {
	printf("Mapping error(b): %d\n", errno);
	exit(0);
      }

      emu_brkpt = (u_int)base + hdr.a_text + hdr.a_data + hdr.a_bss;
      entry = hdr.a_entry;
      break;
    }
    break;

  case BF_AOUT_OPENBSD:
    if (dlevel > 0) printf("binary format = OPENBSD\n");
    res = read(f, &hdr, sizeof (hdr));

    if (res == -1) {
      printf("Error reading from file %s: (%d)%s\n", fnamep, errno,
	     strerror(errno));
      exit(0);
    } else if (res != sizeof(hdr)) {
      printf("Error reading from file %s\n", fnamep);
      exit(0);
    }

    switch (bin_format) {
    case BF_OMAGIC:
      if (dlevel > 0) printf("mmaping OMAGIC file\n");
      base = mmap((caddr_t)0x1000, hdr.a_text + hdr.a_data, PROT_READ |
		  PROT_EXEC, MAP_FIXED | MAP_PRIVATE, f, sizeof(struct exec));
      if ((int)base == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      daddr = 0x1000 + hdr.a_text;
      baddr = roundup(daddr+hdr.a_data,NBPG);
      bsize = daddr + hdr.a_data + hdr.a_bss - baddr;
      if (dlevel > 0) printf("bzero'ing bss %x %x\n",
			     (u_int)baddr, (u_int)bsize);
      if (hdr.a_bss && (int)mmap((caddr_t)baddr, bsize, PROT_READ | 
				 PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON |
				 MAP_PRIVATE, -1, 0) == -1) {
	printf("Mapping error(b): %d\n", errno);
	exit(0);
      }
      bzero((caddr_t)baddr, bsize);

      emu_brkpt = (u_int)baddr+bsize;
      entry = hdr.a_entry;
      break;

    case BF_NMAGIC:
      if (dlevel > 0) printf("mmaping NMAGIC file\n");
      base = mmap((caddr_t)0x1000, hdr.a_text, PROT_READ | PROT_EXEC, 
		  MAP_FIXED | MAP_PRIVATE, f, sizeof(struct exec));
      if ((int)base == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }

      daddr = roundup((int)base+hdr.a_text,NBPG);
      base2 = mmap((caddr_t)daddr, hdr.a_data,
		   PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE, 
		   f, hdr.a_text + sizeof(struct exec));
      if ((int)base2 == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }
      
      baddr = roundup(daddr+hdr.a_data, NBPG);
      bsize = daddr + hdr.a_data + hdr.a_bss - baddr;
      if (dlevel > 0) printf("bzero'ing bss %x %x\n",
			     (u_int)baddr, (u_int)bsize);
      if (hdr.a_bss && (int)mmap((caddr_t)baddr, bsize, PROT_READ | 
				 PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON |
				 MAP_PRIVATE, -1, 0) == -1) {
	printf("Mapping error(b): %d\n", errno);
	exit(0);
      }
      bzero((caddr_t)baddr, bsize);
      
      emu_brkpt = (u_int)baddr+bsize;
      entry = hdr.a_entry;
      break; 

    case BF_QMAGIC:
    case BF_ZMAGIC:
      if (dlevel > 0) printf("mmaping Z/QMAGIC file\n");
      base = mmap((caddr_t)0x1000, hdr.a_text, PROT_READ | PROT_EXEC, 
		  MAP_FIXED | MAP_PRIVATE, f, 0);
      if ((int)base == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }
      
      base2 = mmap((caddr_t)(base+hdr.a_text), hdr.a_data, PROT_READ | 
		   PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE, f, 
		   hdr.a_text);
      if ((int)base2 == -1) {
	printf("Error mapping in executable: (%d)%s\n", errno,
	       strerror(errno));
	exit(0);
      }
      
      if (dlevel > 0) printf("mmaping bss %x %x\n",
			     (u_int)(base + hdr.a_text + hdr.a_data),
			     (u_int)hdr.a_bss);
      if (hdr.a_bss && (int)mmap(base + hdr.a_text + hdr.a_data, hdr.a_bss,
				 PROT_READ | PROT_WRITE | PROT_EXEC,
				 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0)
	  == -1) {
	printf("Mapping error(b): %d\n", errno);
	exit(0);
      }

      emu_brkpt = (u_int)base + hdr.a_text + hdr.a_data + hdr.a_bss;
      entry = hdr.a_entry;
      break;

    default:
      printf("File %s has unknown magic number 0%o\n", fnamep, res);
      exit(0);
    }
    break;

  default:
    printf("File %s has unknown magic number 0%o\n", fnamep, res);
    exit(0);
  }

  if (dlevel > 0) printf("jumping off to %p!\n", (void*)entry);
  emu_brk_heap_zero = 1; /* older mallocs require the heap to be zeroed */

  envc = 0;
  pc = environ;
  while (*(pc++)) envc++;

  aesize = (argc + envc + 3)*4;

  gargc = argc - 1;
  genvc = envc;
  genviron = environ;
  gargv = &argv[1];

  PS_STRINGS->ps_argvstr = gargv;
  PS_STRINGS->ps_nargvstr = gargc;
  PS_STRINGS->ps_envstr = genviron;
  PS_STRINGS->ps_nenvstr = genvc;

  asm("movl %%esp, %0 \n"
      "\taddl %%esp, %1" : "=g" (gloc) : "g" (aesize));

  gloc -= 4;
  *(u_int*)gloc = 0;
  gloc -= genvc*4;
  for (gi = 0; gi < genvc; gi++)
    *(char**)(gloc+gi*4) = genviron[gi];

  gloc -= 4;
  *(u_int*)gloc = 0;

  gloc -= gargc*4;
  for (gi = 0; gi < gargc; gi++)
    *(char**)(gloc+gi*4) = gargv[gi];
  gloc -= 4;
  *(int*)gloc = gargc;

  asm("movl %0, %%esp\n"
      "\tmovl %2, %%ebx\n"
      "\tjmp %1" :: "g" (gloc), "r" (entry), "i" (PS_STRINGS));

  return(0);
}
