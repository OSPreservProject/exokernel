
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

#include <exos/vm-layout.h>
#include <exos/vm.h>
#include <exos/mallocs.h>
#include <exos/process.h>	/* for pid2envid */
#include <unistd.h>		/* for __progname */
#include <stdio.h>
#include <stdlib.h>
#include <xok/env.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>

extern char* symlookup(char *fname, u_int val);

/* do a stack backtrace */
void backtrace_stacktrace(struct Env *env, char *path, u_int temp_pages)
{
  unsigned int ebp = env->env_tf.tf_esp + 16;
  unsigned int eip, lastebp;
  unsigned int temp;
  u_int offset = PGROUNDDOWN(env->env_tf.tf_esp) - temp_pages;
  int repeats;
  int foundend = 0;

  printf("***** starting stack backtrace ***** envid: %d, (%s)\n", env->env_id,
	 path);
  printf("(There may be a pause while debugging symbols are loaded)\n");
#define BACKTRACE_MAXFRAMES	20

  repeats = BACKTRACE_MAXFRAMES;
  ebp -= offset;
  asm ("movl 0(%1), %0\n" : "=r" (temp) : "r" (ebp));
  ebp = temp - offset;
  lastebp = ebp;
  while (repeats > 0)
    {
      repeats--;

      asm ("movl 4(%2), %1\n"
	   "\tmovl 0(%2), %0\n" : "=r" (temp), "=r" (eip) : "r" (ebp));
      eip -= 4;
      ebp = temp - offset;
      if ((ebp >= USTACKTOP - offset - 4 /*|| ebp < lastebp*/) && 
	  repeats <= BACKTRACE_MAXFRAMES - 2)
	{
	  foundend = 1;
	  break;
	}
      lastebp = ebp;
      printf ("\tcalled from 0x%x", eip);
      {
	char *s;

	s = symlookup(((eip >= 0x10000000) && (eip < 0x20000000)) ?
		      "/usr/lib/libexos.so" : path, eip);
	if (s > 0)
	  printf(": %s",s);
	else
	  printf(": symlookup error: use non-stripped binary");
      }
      printf("\n");
    }
  if (foundend)
    printf ("***** traced back to bad value *****\n");
  else
    printf ("***** quit backtrace at %d levels *****\n", BACKTRACE_MAXFRAMES);
  
  return;
}

int restore_u_status(int u_status, struct Uenv *uenv, u_int e)
{
  uenv->u_status = u_status;
  if (sys_wru(0, e, uenv))
    {
      printf("ERROR: sys_wru failed!\n");
      return -1;
    }

  return 0;
}

void usage(void) {
  extern char *__progname;
  printf("Usage: %s pid|-e envid\n", __progname);
  exit(-1);
}

int main(int argc, char *argv[])
{
  u_int e;
  struct Env *env;
  struct Uenv uenv;
  Pte p;
  u_int va, self_va=0, i;
  int u_status_save, stack_pages;
  int r;


  if (argc == 3) {
    if (!strcmp(argv[1],"-e")) usage();
    e = atoi(argv[2]);
    if (e <= 0) usage();
  } else if (argc == 2) {
    pid_t pid = atoi(argv[1]);
    e = pid2envid(pid);
    if (e <= 0) {
      fprintf(stderr,"Negative or non-existing environment for pid: %d\n",pid);
      usage();
    }
  } else usage();
    

  printf("Attempting to backtrace envid: %u\n", e);

  if (!(env = env_id2env(e, &r)) || sys_rdu(0, e, &uenv))
    {
      printf("Bad (or no such) environment\n");
      goto err;
    }
  u_status_save = uenv.u_status;
  uenv.u_status = 0;
  if (sys_wru(0, e, &uenv))
    {
      printf("sys_wru failed\n");
      goto err;
    }

  printf("Name of program: %s\n", uenv.name);
  printf("ESP of envid %u = 0x%x\n", e, env->env_tf.tf_esp);
  stack_pages = PGNO(USTACKTOP - PGROUNDDOWN(env->env_tf.tf_esp));
  printf("%d page(s) of stack\n", stack_pages);
  if (stack_pages > 80) {
    stack_pages = 80;
    printf("Too many pages, map only %d page(s) of stack\n", stack_pages);
  }

  self_va = (u_int)__malloc(stack_pages*NBPG);
  if (self_va == 0) {
    printf("__malloc failed\n");
    goto err;
  }
  va = PGROUNDDOWN(env->env_tf.tf_esp);
  for (i=0; i < stack_pages && va < USTACKTOP; i++) {
    p = sys_read_pte(va+i*NBPG, 0, e, &r);
    if (p)
      {
	if (_exos_self_insert_pte(0, p, self_va+i*NBPG, 0, NULL))
	  {
	    printf("Error mapping in page: 0x%x\n", self_va+i*NBPG);
	    goto err_wru;
	  }
      }
    else
      printf("Warning: hole in stack.\n");

  }
  
  printf("Successfully mapped in stack from other process.\n");

  backtrace_stacktrace(env, uenv.name, self_va);

  __free((void*)self_va);
  return restore_u_status(u_status_save, &uenv, e);

err_wru:
  restore_u_status(u_status_save, &uenv, e);
err:
  __free((void*)self_va);
  return -1;
}
