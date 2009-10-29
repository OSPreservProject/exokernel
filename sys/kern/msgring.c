
/*
 * msg rings - similar to packet rings, except for transfering messages in
 * kernel for communication purposes.
 */

#include <xok/defs.h>
#include <xok/sys_proto.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/msgring.h>
#include <xok/mmu.h>
#include <xok/pctr.h>
#include <xok/cpu.h>
#include <xok/kerrno.h>
#include <xok/mplock.h>


static void 
msgringent_free (msgringent * ktmp)
{
  int i;
  for(i=0; i<ktmp->body.n; i++)
    ppage_unpin (kva2pp ((u_long) ktmp->body.r[i].data));
  if (ktmp->owner != NULL) 
    ppage_unpin (kva2pp ((u_long) ktmp->owner));
  free (ktmp);
}


/* values for checking read access and write access of syscall parameters */
#define READ_MASK        (PG_P|PG_U)
#define WRITE_MASK       (PG_P|PG_U|PG_W)

static msgringent *
msgringent_setup (msgringent * u_msgringent)
{
  msgringent *ktmp;
  Pte *pte = NULL;
  int scatptr = 0;
  int total_len = 0;

  ktmp = (msgringent *) malloc (sizeof (msgringent));
  if (ktmp == NULL)
  {
    warn ("msgringent_setup: failed malloc");
    return (NULL);
  }

  ktmp->appaddr = u_msgringent;
  ktmp->owner = NULL;
  ktmp->body.n = 0;

  /* Verify and translate owner field */
  if ((((u_int) u_msgringent->owner % sizeof (int)) ||
       ! (pte = va2ptep ((u_int) u_msgringent->owner)) ||
         ((*pte & WRITE_MASK) != WRITE_MASK)))
  {
    warn ("msgringent_setup: owner field failed\n");
    msgringent_free (ktmp);
    return (NULL);
  }
  ktmp->owner = (u_int *) pa2kva (va2pa (u_msgringent->owner));
  ppage_pin (kva2pp ((u_long) ktmp->owner));

  
  /* Verify and translate data field */
  if (u_msgringent->body.n > 1)
  {
    warn ("msgringent_setup: not allowed to setup disjoint message body\n");
    msgringent_free (ktmp);
    return (NULL);
  }

  scatptr = 0;
  total_len = 0;
  
  {
    int len = u_msgringent->body.r[0].sz;
    caddr_t addr = u_msgringent->body.r[0].data;
    u_int pagebound = NBPG-(((u_long)addr)&(NBPG - 1));

    while (len > 0)
    {
      u_int slen = min (len, pagebound);
      if (!(pte = va2ptep ((u_int) addr)) || 
          ((*pte & READ_MASK) != READ_MASK))
      {
        /* physical page is not accessible */
        warn ("msgringent_setup: can't read scatter ptr\n");
        msgringent_free (ktmp);
        return (NULL);
      }
            
      ktmp->body.r[scatptr].data = (char *) pa2kva (va2pa (addr));
      ktmp->body.r[scatptr].sz = slen;
      ktmp->body.n++;
	
      /* pin the page to prevent re-allocation */
      ppage_pin (kva2pp ((u_long) ktmp->body.r[scatptr].data));
      len -= slen;
      addr += slen;
      total_len += slen;
      pagebound = NBPG;
      scatptr++;
    
      if (scatptr > IPC_MAX_SCATTER_PTR || total_len > IPC_MAX_MSG_SIZE)
      {
        msgringent_free (ktmp);
        warn ("msgringent_setup: message body too big\n");
        return (NULL);
      }
    }
  }

  return (ktmp);
}


void 
msgring_free (msgringent * ringhead)
{
  msgringent *ktmp;

  while (ringhead != NULL) 
  {
    ktmp = ringhead->next;
    ringhead->next = ktmp->next;
    if (ktmp == ktmp->next) 
      ringhead = NULL;
    msgringent_free (ktmp);
  }
}


int 
sys_msgring_setring (u_int sn, struct msgringent *ringhead_user)
{
  int ringentries = 0;
  msgringent *utmp, *ktmp, *kprev = NULL;
  struct msgringent ringhead_kernel;

  MP_SPINLOCK_GET(&curenv->env_spinlock);

  if (curenv->msgring != 0L) 
  {
    msgring_free (curenv->msgring);
    curenv->msgring = 0L;
  }
  utmp = ringhead_user;

  do 
  {
    if (utmp == NULL) 
    {
      msgring_free (curenv->msgring);
      curenv->msgring = 0L;
      MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
      return (-E_FAULT);
    }
      
    copyin (utmp, &ringhead_kernel, sizeof (ringhead_kernel));
    if ((ktmp = msgringent_setup (&ringhead_kernel)) == NULL)
    {
      msgring_free (curenv->msgring);
      curenv->msgring = 0L;
      MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
      return (-E_FAULT);
    }

    if (kprev)
    {
      ktmp->next = kprev->next;
      kprev->next = ktmp;
    }
    else
    {
      ktmp->next = ktmp;
      curenv->msgring = ktmp;
    }

    kprev = ktmp;
    ringentries++;
    utmp = ringhead_kernel.next;
  }
  while (utmp != ringhead_user);

  MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
  return 0;
}



int 
sys_msgring_modring (u_int sn, 
		     struct msgringent *newentry_user,
		     struct msgringent *oldentry, 
		     int replace)
{
  msgringent *ktmp = NULL;
  msgringent *kprev;
  msgringent newentry_kernel;

  if (oldentry == NULL)
    return (-E_INVAL);

  MP_SPINLOCK_GET(&curenv->env_spinlock);

  kprev = curenv->msgring;
  if (kprev == NULL)
  {
    warn ("msgring_modring: can't mod a non-existent ring\n");
    MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
    return (-E_NO_MSGRING);
  }

  do
  {
    if (kprev->next->appaddr == oldentry)
      break;
    kprev = kprev->next;
  }
  while (kprev != curenv->msgring);

  if (kprev->next->appaddr != oldentry)
  {
    warn ("msgring_modring: can't find matching msgringent for %p\n", oldentry);
    MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
    return (-E_NOT_FOUND);
  }

  if (newentry_user)
  {
    copyin (newentry_user, &newentry_kernel, sizeof (newentry_kernel));
    if ((ktmp = msgringent_setup (&newentry_kernel)) == NULL) 
    { 
      warn ("msgring_modring: failed to setup new msgringent\n"); 
      MP_SPINLOCK_RELEASE (&curenv->env_spinlock); 
      return (-E_INVAL); 
    } 
  }

  if (replace && newentry_user)
  {
    ktmp->next = kprev->next->next;
    if (curenv->msgring == kprev->next)
      curenv->msgring = ktmp;
    msgringent_free (kprev->next);
    kprev->next = ktmp;
  }
  else if (newentry_user)
  {
    ktmp->next = kprev->next->next;
    kprev->next->next = ktmp;
  }
  else if (replace)
  {
    ktmp = kprev->next;
    kprev->next = ktmp->next;
    if (curenv->msgring = ktmp)
      curenv->msgring = ktmp->next;
    msgringent_free (kprev->next);
  }

  MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
  return (0);
}



int 
sys_msgring_delring (u_int sn)
{
  MP_SPINLOCK_GET(&curenv->env_spinlock);

  if (curenv->msgring == 0)
  {
    MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
    return (-E_NOT_FOUND);
  }
  msgring_free (curenv->msgring);
  curenv->msgring = 0L;

  MP_SPINLOCK_RELEASE (&curenv->env_spinlock);
  return (0);
}


int 
sys_ipc_sendmsg (u_int sn, u_int k, u_int envid, u_int sz, u_int addr)
{
  int r;
  // unsigned char msg[IPC_MAX_MSG_SIZE];
  struct Env *e;
  extern int msgring_handlemsg(struct Env *, int sz, unsigned char *);

  if (sz == 0)
    return -E_INVAL;

  if (sz > IPC_MAX_MSG_SIZE)
    return -E_INVAL;

  if (!(e = env_id2env (envid, &r)))
    return -E_BAD_ENV;
  
  if (e->msgring == NULL)
    return -E_NO_MSGRING;
  
  // copyin((void*)addr, &msg[0], sz);

  MP_SPINLOCK_GET(&e->env_spinlock);
  r = msgring_handlemsg(e, sz, (unsigned char *)addr);
  MP_SPINLOCK_RELEASE (&e->env_spinlock);

  return r;
}



int 
msgring_handlemsg (struct Env *e, int len, unsigned char *data)
  __XOK_REQ_SYNC(on env)
{
  int i;
  int runlen = 0;
  msgringent *ktmp = e->msgring;
  struct ae_recv incoming;

  if (*(ktmp->owner) != IPC_MSGRINGENT_OWNEDBY_APP)
  {
    // warn ("msgring full (envid %d, cpu %d)\n", e->env_id, cpu_id);
    return -E_MSGRING_FULL;
  }

  e->msgring = ktmp->next;

  incoming.n = 1;
  incoming.r[0].sz = len;
  incoming.r[0].data = data;
  
  for (i = 0; i < ktmp->body.n; i++)
  {
    int tmplen = min ((len - runlen), ktmp->body.r[i].sz);
    ae_recv_datacpy_in (&incoming, ktmp->body.r[i].data, runlen, tmplen);
    runlen += tmplen;
    if (runlen >= len)
      break;
  }

  if (runlen != len)
  {
#if 0
    warn ("remaining runlen %d (len %d, recv.n %d, recv.r[0].sz %d)\n", 
	  runlen, len, ktmp->body.n, 
	  ((ktmp->body.n) ? ktmp->body.r[0].sz : 0));
#endif
  }
  *(ktmp->owner) = len;
  return 0;
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

