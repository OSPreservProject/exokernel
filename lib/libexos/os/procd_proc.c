#include "procd.h"

#include <string.h>
#include <bitstring.h>



#include <stdio.h>
#include <exos/kprintf.h>
#include <assert.h>

#include <sys/tty.h>

#include <exos/cap.h>
#include <errno.h>


#define DIAGNOSTIC

#include <exos/critical.h>	/* removeme  */
/* LOCATORS */

#define dprintf(fmt, args...) if (0) kprintf(fmt, ##args )
 
proc_p 
efind(unsigned int envid) {
  proc_p p;
  int iter = init_iter_proc_p();
  while( (p = iter_proc_p (&iter)) ) {
    if (p->envid == envid) return p;
  }
  return (proc_p) 0;
}


/* ITERATORS */
/* iterate over all processes on the system */

int
init_iter_proc_p (void)
{
   return(0);
}


proc_p
iter_proc_p (int *iterator_pid_ptr)
{
  proc_p p;
  int iterator_pid = *iterator_pid_ptr;
  for(;;) {
    if (iterator_pid == MAXPROC) {
      *iterator_pid_ptr = iterator_pid;
      return (proc_p) 0;
    }
    if (iterator_pid > MAXPROC) {
      kprintf("BAD ITER_PROC_P\n");
      *iterator_pid_ptr = iterator_pid;
      return (proc_p) 0;
    }
    assert(iterator_pid >= 0);
    assert(iterator_pid < MAXPROC);
    if (bit_test(proc_table->procs_used,iterator_pid)) {
      p = &proc_table->procs[iterator_pid++];
      *iterator_pid_ptr = iterator_pid;
      return p;
    } 
    iterator_pid++;
  }
}


proc_p
init_iter_pg_p(pgrp_p pg)
{
   return (pg->pg_members.lh_first);
}


proc_p
iter_pg_p (proc_p *iterator_p_ptr)
{
  proc_p p = *iterator_p_ptr;
  if (p) {
     *iterator_p_ptr = p->p_pglist.le_next;
  }
  return p;
}


/**********************************
 * Similar to OpenBSD kern_proc.c *
 **********************************/

static void orphanpg(pgrp_p pg);

#define INITPID 0
/* 
 * not using procinit()
 */

/* 
 * not using chgproccnt()
 */

/*
 * Is p1 inferior (child...) to p2?
 */
int
inferior(proc_p p1, proc_p p2) {
  for (; p1 && p1 != p2; p1 = p1->p_pptr) {
    if (p1->p_pid == INITPID)
      return (0);
    if (p1 == p1->p_pptr) break;
  }
  return (1);
}

/*
 * Locate a process by number
 */
proc_p 
__pd_pfind(pid_t pid) {
  proc_p p;
  int iter = init_iter_proc_p();
  while( (p = iter_proc_p (&iter)) ) {
    if (p->p_pid == pid) return p;
  }
  return (proc_p)0;
}


/*
 * Locate a process group by number
 */
pgrp_p 
pgfind(pid_t pgid) {
  int i;
  pgrp_p pg;
  for (i = 0 , pg = &proc_table->pgrps[0]; i < MAXPGRP; i++, pg++) 
    if (bit_test(proc_table->pgrps_used,i) &&
	pg->pg_id == pgid) return pg;
  return (pgrp_p)0;
}


/*
 * Move p to a new or existing process group (and session)
 */
int
enterpgrp(proc_p p, pid_t pgid, int mksess) {
  register pgrp_p pgrp = pgfind(pgid);
#ifdef DIAGNOSTIC
  if (pgrp != NULL && mksess)	/* firewalls */
    panic("enterpgrp: setsid into non-empty pgrp");
  if (SESS_LEADER(p))
    panic("enterpgrp: session leader attempted setpgrp");
#endif
  {
    extern int getpid();
    dprintf("%d enterpgrp(p %p (pid: %d),pgid %d,mksess %d) (pgrp = %p pgid: %d)\n",
	    getpid(),p,p ? p->p_pid : -1,pgid,mksess, pgrp,pgrp ? pgrp->pg_id : -1);
  }
  if (pgrp == NULL) {
    pid_t savepid = p->p_pid;
    proc_p np;
    /*
     * new process group
     */
#ifdef DIAGNOSTIC
    if (p->p_pid != pgid)
      panic("enterpgrp: new pgrp and pid != pgid");
#endif

    if ((np = __pd_pfind(savepid)) == NULL || np != p)
      return (ESRCH);

    pgrp = pgrp_alloc();

    if (mksess) {
      session_p sess;
      /*
       * new session
       */
      sess = session_alloc();

      sess->s_leader = p;
      sess->s_count = 1;
      sess->s_ttyp = (struct tty *)0;
      bcopy(p->p_session->s_login, sess->s_login,
	    sizeof(sess->s_login));
      p->p_flag &= ~P_CONTROLT;
      pgrp->pg_session = sess;
    } else {
      pgrp->pg_session = p->p_session;
      pgrp->pg_session->s_count++;
    }
    pgrp->pg_id = pgid;
    LIST_INIT(&pgrp->pg_members);
    pgrp->pg_jobc = 0;
  } else if (pgrp == p->p_pgrp)
    return (0);
  
  /*
   * Adjust eligibility of affected pgrps to participate in job control.
   * Increment eligibility counts before decrementing, otherwise we
   * could reach 0 spuriously during the first call.
   */
  fixjobc(p, pgrp, 1);
  fixjobc(p, p->p_pgrp, 0);
  
  LIST_REMOVE(p, p_pglist);
  if (p->p_pgrp->pg_members.lh_first == 0)
    pgdelete(p->p_pgrp);
  p->p_pgrp = pgrp;
  LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
  return (0);
}

/*
 * remove process from process group
 */
int
leavepgrp(proc_p p) {
  LIST_REMOVE(p, p_pglist);
  if (p->p_pgrp->pg_members.lh_first == 0)
    pgdelete(p->p_pgrp);
  p->p_pgrp = 0;
  return (0);
}

/*
 * delete a process group
 */
void
pgdelete(pgrp_p pgrp) {
  /* HBXX fix terminal stuff later */
#if 0
  if (pgrp->pg_session->s_ttyp != NULL && 
      pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
    pgrp->pg_session->s_ttyp->t_pgrp = NULL;
#endif
  if (--pgrp->pg_session->s_count == 0)
    session_dealloc(pgrp->pg_session);
  pgrp_dealloc(pgrp);
}

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(proc_p p, pgrp_p pgrp, int entering) {
  register pgrp_p hispgrp;
  register session_p mysession;

  if (pgrp == NULL) {printf("ERROR: fixjobc NULL pgrp\n"); return;}
  mysession = pgrp->pg_session;
  if (mysession == NULL) {printf("ERROR: fixjobc NULL mysession\n"); return;}

  /*
   * Check p's parent to see whether p qualifies its own process
   * group; if so, adjust count for p's process group.
   */
  if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
      hispgrp->pg_session == mysession)
    if (entering)
      pgrp->pg_jobc++;
    else if (--pgrp->pg_jobc == 0)
      orphanpg(pgrp);

  /*
   * Check this process' children to see whether they qualify
   * their process groups; if so, adjust counts for children's
   * process groups.
   */
  for (p = p->p_children.lh_first; p != 0; p = p->p_sibling.le_next)
    if ((hispgrp = p->p_pgrp) != pgrp &&
	hispgrp->pg_session == mysession &&
	p->p_stat != SZOMB)
      if (entering)
	hispgrp->pg_jobc++;
      else if (--hispgrp->pg_jobc == 0)
	orphanpg(hispgrp);
}

/* 
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(pgrp_p pg) {
  register proc_p p;

  for (p = pg->pg_members.lh_first; p != 0; p = p->p_pglist.le_next) {
    if (p->p_stat == SSTOP) {
      for (p = pg->pg_members.lh_first; p != 0;
	   p = p->p_pglist.le_next) {
	proc_psignal(p, SIGHUP);
	proc_psignal(p, SIGCONT);
      }
      return;
    }
  }
}
