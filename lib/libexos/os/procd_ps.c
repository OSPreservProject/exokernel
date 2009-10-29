
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

#include "procd.h"

#include <string.h>
#include <bitstring.h>

#include <stdio.h>
#include <exos/kprintf.h>
#include <assert.h>

#include <exos/cap.h>
#include <exos/vm.h>
#include <exos/mallocs.h>
#include <xok/env.h>
#include <exos/vm-layout.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <exos/fpu.h>

#include "fd/proc.h"
#include <sys/tty.h>
#include <xok/sys_ucall.h>


/* 
 * PS FUNCTIONALITY 
 */



void
print_proc(proc_p p) {
  if (p == NULL) {
    fprintf(stderr,"ERROR PROC null\n");
    return;
  }
  fprintf(stderr,"PROC %10p: pid: %3d, pgrp: %10p, p_pptr: %10p ",
	 p,p->p_pid,p->p_pgrp,p->p_pptr);
  fprintf(stderr,"p_xstat: %4x, p_flag: %6x nxchildren: %1d p_stat: %1d envid: %d\n",
	 p->p_xstat,p->p_flag,p->nxchildren,p->p_stat,p->envid);
}
void
print_pgrp(pgrp_p pg) {
  proc_p p;
  if (pg == NULL) {
    fprintf(stderr,"ERROR PGRP null\n");
    return;
  }
  fprintf(stderr,"PGRP %10p session: %3d pg_id: %3d pg_jobc: %1d (",
	  pg, 
	  (pg->pg_session && pg->pg_session->s_leader) 
	  ? pg->pg_session->s_leader->p_pid : -1, 
	  pg->pg_id, 
	  pg->pg_jobc);
  for (p = pg->pg_members.lh_first; p != 0; p = p->p_pglist.le_next) {
    fprintf(stderr,"%d ",p->p_pid);
  }
  fprintf(stderr,")\n");

}

void
print_session(session_p s) {
  if (s == NULL) {
    fprintf(stderr,"ERROR SESS null\n");
    return;
  }
  fprintf(stderr,"SESS %10p leader: %3d login: \"%8s\" count: %1d ttyp: %10p ttyvp: %10p ",
	  s, 
	  (s->s_leader) ? s->s_leader->p_pid : -1 , 
	  s->s_login, 
	  s->s_count,
	  s->s_ttyp,
	  s->s_ttyvp);
  if (s->s_ttyvp) fprintf(stderr,"ttyvp->f_dev %d,%d ",
			  major(s->s_ttyvp->f_dev),minor(s->s_ttyvp->f_dev));

  if (s->s_ttyp) fprintf(stderr,"ttyp pgrp %3d session %3d",
			 s->s_ttyp->t_pgrp ? s->s_ttyp->t_pgrp->pg_id : -1,
			 s->s_ttyp->t_session ? 
			 s->s_ttyp->t_session->s_leader->p_pid : -1);
  fprintf(stderr,"\n");
}



static char *ps_get_comm(u_int envid, int shortname) {
  static struct Uenv cu;
  char *name;
  if (sys_rdu (CAP_ROOT, envid, &cu) < 0)
    return "*";
  if (shortname && (name = rindex(&cu.name[0],'/'))) name++; /* skip '/' */
  else name = &cu.name[0];
  return name;
}

static char *ps_get_flags(int flag) {
  static char flags[] = "    ";

  if (flag == -1) {
    return "E: Execed, w: waited, C: has controlling term.\nW: vfork parent waiting\n";
  }
  flags[0] = flags[1] = flags[2] = flags[3] = ' ';
  if (flag & P_EXEC) flags[0] = 'E';
  if (flag & P_WAITED) flags[1] = 'w';
  if (flag & P_CONTROLT) flags[2] = 'C';
  if (flag & P_PPWAIT) flags[3] = 'W';

  return flags;
}

static u_int read_write_memory (int request,u_int env, u_int addr, int data) {
  u_int write;
  u_int pte;
  u_int offset;
  u_int va = (u_int )addr;
  u_int temp_page;
  int r;

#define PT_WRITE_D 1
#define PT_WRITE_I 1
#define PT_READ_D 0
#define PT_READ_I 0

  if (request == PT_WRITE_D || request == PT_WRITE_I) {
    write = 1;
  } else {
    write = 0;
  }

  assert (env);
  if (!(pte = sys_read_pte (va, 0, env, &r))) {
    printf ("ptrace: couldn't read pte (va %x, env %x)\n", va, env);
    return (0);
  }
  temp_page = (u_int)__malloc(NBPG);
  if (temp_page == 0 ||
      _exos_self_insert_pte (0, write ? pte | PG_W : pte, temp_page,
			     ESIP_DONTPAGE, NULL) < 0) {
    printf ("ptrace: couldn't map page\n");
    __free((void*)temp_page);
    return (0);
  }
  offset = va & PGMASK;
  va = temp_page+offset;

  if (write) {
    *(int *)va = data;
  } else {
    data = *(int *)va;
  }

  if (_exos_self_unmap_page (0, temp_page) < 0) {
    printf ("ptrace: couldn't unmap page\n");
    __free((void*)temp_page);
    assert (0);
  }

  __free((void*)temp_page);
  return (data);
}
static int get_counts (u_int envid, int *epilogue, int *yield) {
  struct Uenv cu;
  int ret;

  if ((ret = sys_rdu(CAP_ROOT,envid,&cu)) >= 0) {
    if (epilogue) *epilogue = cu.u_epilogue_count;
    if (yield) *yield = cu.u_yield_count;
    return 0;
  }
  if (epilogue) *epilogue = -1;
  if (yield) *yield = -1;
    return -1;
}
static unsigned int get_eip (u_int envid) {
  unsigned int esp;
  unsigned int eip = 0;

  struct Env *e = (struct Env *)&__envs[envidx (envid)];
  esp = e->env_tf.tf_esp + 44;

  /* check if pid is saving its fpu state */ 
  { 
    struct Uenv cu;
    int ret;
    if ((ret = sys_rdu(CAP_ROOT,e->env_id,&cu)) >= 0) {
      if (cu.u_fpu) esp += FPU_SAVE_SZ; 
    }
    /* if this fails, read_write_memory will fail too. */
  }
  eip = read_write_memory (PT_READ_D, envid, esp, 0);

  return (eip);
}

static void ps_print_level(int level, proc_p p) {
  /* pid ppid envid pgrp session   xstat stat flag nxch */
#define MAXLEVEL 4
  int i;
  int epilogue_count, yield_count;
#if 0
  char state[] = "XIRSHZ";
#endif
  if (level == -1) {
    printf("PID ");
    for (i = 0; i < MAXLEVEL; i++) printf("  ");

#if 0    
    printf("PPID  PG SES "); 
    printf("sta xst flag  nx ");
    printf("ticks ctxcnt eip      name\n");
#else
    printf ("    TICKS\tCTXCNT\tEIP\t\tNAME\t\tEID\n\n");
#endif
    return;
  }
  if (level > MAXLEVEL) level = MAXLEVEL;
  for (i = 0; i < level; i++) printf("  ");

  if (p == NULL) {printf(" null proc entry\n");return;}

  printf("%-3d",p->p_pid);
  for (i = level; i <= MAXLEVEL; i++) printf("  ");
#if 0
  printf("%3d ",(p->p_pptr) ? p->p_pptr->p_pid : -1);
  printf("%3d ",(p->p_pgrp) ? p->p_pgrp->pg_id : -1);
  printf("%3d ",(p->p_session->s_leader) ? p->p_session->s_leader->p_pid : -1);
  printf("%c %3d %4s %3d ",
	   state[p->p_stat], p->p_xstat, ps_get_flags(p->p_flag), p->nxchildren);
#endif
  epilogue_count = 0;
  yield_count = 0;
  get_counts(p->envid, &epilogue_count, &yield_count);
  printf("\t%5d\t%6d\t%08x\t%.10s",
	 __envs[envidx(p->envid)].env_ticks,
	 __envs[envidx(p->envid)].env_ctxcnt, /*  epilogue_count, */
	 (p->envid == __envid || p->p_stat == SZOMB) ? 0 : get_eip(p->envid),
	 ps_get_comm(p->envid,1));
  if (strlen (ps_get_comm (p->envid,1)) < 8)
    printf ("\t\t");
  else
    printf ("\t");
  printf ("%d\n", p->envid);
  for (p = p->p_children.lh_first; p != 0; p = p->p_sibling.le_next) {
    ps_print_level(level+1,p);
  }

}

void procd_ps(void) {
  proc_p p;
#if 0
  int i;
  pgrp_p pg;
  session_p s;

  fprintf(stderr,"proc_table: %p, envid: %d\n",proc_table,proc_table->procd_envid);

  for (i = 0; i < MAXPGRP; i++) {
    if (!bit_test(proc_table->pgrps_used,i)) continue;
    pg = &proc_table->pgrps[i];
    print_pgrp(pg);
  }
  for (i = 0; i < MAXSESSION; i++) {
    if (!bit_test(proc_table->sessions_used,i)) continue;
    s = &proc_table->sessions[i];
    print_session(s);
  }
  fprintf(stderr,"done\n");
#endif
  p = __pd_pfind(INITPROC);
  if (p) {
    ps_get_flags(-1);
    ps_print_level(-1,NULL);
    ps_print_level(0,p);
  }
  else fprintf(stderr,"COULD NOT FIND PID %d\n",INITPROC);
}


void procd_ps2(void) {
  proc_p p;
  int i;
  pgrp_p pg;
  session_p s;
  fprintf(stderr,"proc_table: %p, envid: %d\n",proc_table,proc_table->procd_envid);

  for (i = 0; i < MAXPROC; i++) {
    fprintf(stderr,"%d ",(bit_test(proc_table->procs_used,i)) ?1:0);
    p = &proc_table->procs[i];
    print_proc(p);
  }
  for (i = 0; i < MAXPGRP; i++) {
    fprintf(stderr,"%d ",(bit_test(proc_table->pgrps_used,i)) ?1:0);
    pg = &proc_table->pgrps[i];
    print_pgrp(pg);
  }
  for (i = 0; i < MAXSESSION; i++) {
    fprintf(stderr,"%d ",(bit_test(proc_table->sessions_used,i)) ?1:0);
    s = &proc_table->sessions[i];
    print_session(s);
  }
  fprintf(stderr,"done\n");
}
