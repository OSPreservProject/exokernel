#include "procd.h"

#include "fd/proc.h"
#include <sys/mman.h>
#include <assert.h>
#include <string.h>

#include <exos/vm-layout.h>
#include <exos/uwk.h>
#include <exos/ipcdemux.h>
#include <exos/critical.h>
#include <exos/kprintf.h>


#include <unistd.h>
#include <signal.h>

proc_table_p proc_table = NULL;
proc_p curproc;

/* dprintf prints debugging allocation and deallocation information */
#define dprintf if (0) kprintf

static void initialize_proc_table(void);

int 
proc_table_init(void) {
  int status;

  status = fd_shm_alloc(PROC_TABLE_SHM_OFFSET,
			sizeof(proc_table_t),
			(char *)PROC_TABLE_SHARED_REGION);
  proc_table = (proc_table_p)PROC_TABLE_SHARED_REGION;
  
  StaticAssert((sizeof(proc_table_t)) <= PROC_TABLE_SHARED_REGION_SZ);

  if (status == -1) {
    demand(0, problems attaching shm);
    return -1;
  }

  if (status) {
    proc_p p;
    pgrp_p pgrp0;
    session_p session0;

    initialize_proc_table();
    
    p = proc_alloc();
    assert(p);
    pgrp0 = pgrp_alloc();
    assert(pgrp0);
    session0 = session_alloc();
    assert(session0);
    kprintf("PROC_TABLE_INIT %p %p %p\n",p,pgrp0,session0);

    p->p_pgrp = pgrp0;
    LIST_INIT(&p->p_children);
    p->p_pptr = p;		/* is this needed? */
    LIST_INIT(&pgrp0->pg_members);
    LIST_INSERT_HEAD(&pgrp0->pg_members, p, p_pglist);
    pgrp0->pg_session = session0;
    pgrp0->pg_id = p->p_pid;
    session0->s_count = 1;
    session0->s_leader = p;
    session0->s_login[0] = 0;
    session0->s_ttyp = NULL;
    p->p_flag = 0;
    p->p_stat = SRUN;
    p->p_xstat = 0;
    p->nxchildren = 0;
    p->envid = __envid;
    
    
    fprintf(stderr,"done initializing first process\n");
    print_proc(p);
    print_pgrp(pgrp0);
    print_session(session0);

    set_procd_envid(__envid);
    strcpy(&UAREA.name[0],"init");
#if 0
    if ((status = fork())) {
      kprintf("fork returned: %d, procd started: %d\n",status,__envid);
      /* parent becomes procd */
      UAREA.u_status = U_SLEEP;
      yield(-1);
      kprintf("SHOULD NEVER REACH HERE PROC_TABLE_INIT\n");
      while(1) yield(-1);
    } 
#endif
    kprintf("child carrying on: %d\n",__envid);
    /* child carries on */
  } else {
    protect_proc_table();
  }
  curproc = efind(__envid);
  return 0;
}

void 
protect_proc_table(void) {
  int status;
  status = mprotect((char *)PROC_TABLE_SHARED_REGION, 
		    PROC_TABLE_SHARED_REGION_SZ,PROT_READ);
  if (status != 0) printf("mprotect %p:%d ro failed\n",
			  (char *)PROC_TABLE_SHARED_REGION, PROC_TABLE_SHARED_REGION_SZ);
}

void 
unprotect_proc_table(void) {
  int status;
  status = mprotect((char *)ARP_SHARED_REGION, ARP_SHARED_REGION_SZ,PROT_READ|PROT_WRITE);
  if (status != 0) printf("mprotect %p:%d ro failed\n",
			  (char *)ARP_SHARED_REGION, ARP_SHARED_REGION_SZ);
}


/* 
 * HELPER FUNCTIONS 
 */


static void 
initialize_proc_table(void) {
  proc_table->procd_envid = -1;
  /* initialize bit vectors */
  bit_nclear(proc_table->procs_used,0,MAXPROC - 1);
  bit_nclear(proc_table->sessions_used,0,MAXSESSION - 1);
  bit_nclear(proc_table->pgrps_used,0,MAXPGRP - 1);
  /* register ipc handlers */
  ipc_register(IPC_PROC_SETSID,proc_table_ipc_setsid);
  ipc_register(IPC_PROC_SETPGID,proc_table_ipc_setpgid);
  ipc_register(IPC_PROC_SETLOGIN,proc_table_ipc_setlogin);
  ipc_register(IPC_PROC_SETSTAT,proc_table_ipc_setstat);
  ipc_register(IPC_PROC_CONTROLT,proc_table_ipc_controlt);
  ipc_register(IPC_PROC_REAP,proc_table_ipc_reap);
  ipc_register(IPC_PROC_FORK,proc_table_ipc_fork);
  ipc_register(IPC_PROC_VFORK,proc_table_ipc_fork);
  ipc_register(IPC_PROC_EXEC,proc_table_ipc_exec);
  ipc_register(IPC_PROC_PTRACE,proc_table_ipc_ptrace);
  ipc_register(IPC_PROC_REPARENT,proc_table_ipc_reparent);
}

/* PROCD ENVID */
unsigned int
get_procd_envid(void) {
  return proc_table->procd_envid;
}
void
set_procd_envid(unsigned int envid) {proc_table->procd_envid = envid;}

/* 
 * Allocators and deallocators 
 */

/* entries in the proc table have their pid set before returning */
proc_p 
proc_alloc(void) {
  int offset;
  /* never allocate 0 */
  static unsigned int lastpid = 1;
  proc_p p;
  for(offset = lastpid; offset < MAXPROC; offset++) 
    if (!bit_test(proc_table->procs_used,offset)) goto ready;
  for(offset = 1; offset < lastpid; offset++) 
    if (!bit_test(proc_table->procs_used,offset)) goto ready;
  
  kprintf("Warning: Out of process table entries\n");
  return (proc_p) 0;
ready:
  lastpid = offset + 1;
  if (lastpid >= MAXPROC) lastpid = 1;

  bit_set(proc_table->procs_used,offset);
  p = &proc_table->procs[offset];
  bzero(p,sizeof(proc_t));
  p->p_pid = offset;
  return p;
}
void 
proc_dealloc(proc_p p) {
  int offset = (((u_int)p - (u_int)proc_table->procs) / sizeof (proc_t));
  if (!bit_test(proc_table->procs_used,offset))
    kprintf("Warning proc_dealloc on a unused entry %p\n",p);
 bit_clear(proc_table->procs_used,offset);
}
pgrp_p
pgrp_alloc(void) {
  int offset;
  pgrp_p p;
  bit_ffc(proc_table->pgrps_used,MAXPGRP,&offset);
  if (offset == -1) {
    kprintf("Warning: Out of pgrps table entries\n");
    return (pgrp_p) 0;
  }
  bit_set(proc_table->pgrps_used,offset);
  p = &proc_table->pgrps[offset];
  bzero(p,sizeof(pgrp_t));
  dprintf("pgrp alloc returns %p offset %d\n",p,offset);
  return p;
}
void 
pgrp_dealloc(pgrp_p p) {
  int offset = (((u_int)p - (u_int)proc_table->pgrps) / sizeof (pgrp_t));
  dprintf("pgrp dealloc: offset %d  bit %d\n",offset,bit_test(proc_table->pgrps_used,offset));
  if (!bit_test(proc_table->pgrps_used,offset))
    kprintf("Warning pgrp_dealloc on a unused entry %p\n",p);
  bit_clear(proc_table->pgrps_used,offset);
}
session_p
session_alloc(void) {
  int offset;
  session_p p;
  bit_ffc(proc_table->sessions_used,MAXSESSION,&offset);
  if (offset == -1) {
    kprintf("Warning: Out of session table entries\n");
    return (session_p) 0;
  }
  bit_set(proc_table->sessions_used,offset);
  p = &proc_table->sessions[offset];
  bzero(p,sizeof(session_t));
  dprintf("session alloc returns %p offset %d\n",p,offset);
  return p;
}
void 
session_dealloc(session_p p) {
  int offset = (((u_int)p - (u_int)proc_table->sessions) / sizeof (session_t));
  dprintf("session dealloc: offset %d  bit %d\n",offset,bit_test(proc_table->sessions_used,offset));
  if (!bit_test(proc_table->sessions_used,offset))
    kprintf("Warning session_dealloc on a unused entry %p\n",p);
  bit_clear(proc_table->sessions_used,offset);
}


/* HBXX CHECK THIS */

int 
proc_psignal(proc_p p, int signo) {
  dprintf("%d psignal pid: %d, signo: %d\n",getpid(),p->p_pid, signo);
  return kill(p->p_pid, signo);
}

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
proc_pgsignal(pgrp, signum, checkctty)
	struct pgrp *pgrp;
	int signum, checkctty;
{
	register struct proc *p;

	dprintf("%d pgsignal pgrp: %d, signo: %d checkctty %d\n",
		getpid(),pgrp ? pgrp->pg_id : -1, signum, checkctty);

	if (pgrp)
		for (p = pgrp->pg_members.lh_first; p != 0; p = p->p_pglist.le_next)
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				proc_psignal(p, signum);
}
