
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

#include <xok/pmap.h>
#include <xok/sysinfo.h>	/* for pkt.h */
#include <xok/defs.h>
#include <xok/pktring.h>
#include <xok/pkt.h>
#include <xok/mmu.h>
#include <xok/sys_proto.h>
#include <xok/mplock.h>
#include <xok/cpu.h>
#include <xok_include/assert.h>
#include <xok/kerrno.h>
#include <xok/printf.h>
#include <xok/mplock.h>


/* TODO -- add protection for rings (which are now orthogonal to filters */


/* Don't use 0th ringid */
static pktringent *rings[(MAX_PKTRING_COUNT + 1)];
static int notify_when_empty[(MAX_PKTRING_COUNT + 1)];
static int ringused[(MAX_PKTRING_COUNT + 1)];

#ifdef __SMP__
/* protectes rings and ringused arrays */
static struct kspinlock ring_spinlocks[(MAX_PKTRING_COUNT + 1)];
#endif

void
pktring_init()
{
  int i;

  for(i = 0; i < MAX_PKTRING_COUNT+1; i++)
  {
    MP_SPINLOCK_INIT(&ring_spinlocks[i]);
    ringused[i] = 0;
    rings[i] = NULL;
  }
}


static void 
pktringent_free (pktringent * ktmp)
{
  int i;

  for (i = 0; i < ktmp->recv.n; i++)
    {
      ppage_unpin (kva2pp ((u_long) ktmp->recv.r[i].data));
    }
  ppage_unpin (kva2pp ((u_long) ktmp->owner));
  free (ktmp);
}


/* values for checking read access and write access of syscall parameters */
#define READ_MASK       (PG_P|PG_U)
#define WRITE_MASK       (PG_P|PG_U|PG_W)

static pktringent *
pktringent_setup (pktringent * u_pktringent)
{
  int i;
  pktringent *ktmp;
  Pte *pte = NULL;
  int scatptr = 0;

  ktmp = (pktringent *) malloc (sizeof (pktringent));
  if (ktmp == NULL)
    {
      warn ("pktringent_setup: failed malloc");
      return (NULL);
    }

  ktmp->appaddr = u_pktringent;
  ktmp->owner = NULL;
  ktmp->recv.n = 0;

  /* Verify and translate owner field */
  if ((((u_int) u_pktringent->owner % sizeof (int)) ||
       ! (pte = va2ptep ((u_int) u_pktringent->owner)) ||
         ((*pte & WRITE_MASK) != WRITE_MASK)))
    {
      warn ("pktringent_setup: owner field failed\n");
      pktringent_free (ktmp);
      return (NULL);
    }
  ktmp->owner = (u_int *) pa2kva (va2pa (u_pktringent->owner));
  ppage_pin (kva2pp ((u_long) ktmp->owner));

  /* Verify and translate recv fields */
  scatptr = 0;
  for (i = 0; i < u_pktringent->recv.n; i++)
    {
      int len = u_pktringent->recv.r[i].sz;
      caddr_t addr = u_pktringent->recv.r[i].data;
      u_int pagebound = NBPG - (((u_long) addr) & (NBPG - 1));

      while (len > 0)
	{
	  u_int slen = min (len, pagebound);
	  if (!(pte = va2ptep ((u_int) addr)) || ((*pte & READ_MASK) != READ_MASK))
	    {
	      /* physical page is not accessible */
	      warn ("pktringent_setup: can't read scatter ptr\n");
	      pktringent_free (ktmp);
	      return (NULL);
	    }
	  ktmp->recv.r[scatptr].data = (char *) pa2kva (va2pa (addr));
	  ktmp->recv.r[scatptr].sz = slen;
	  ktmp->recv.n++;
	  /* pin the page to prevent re-allocation */
	  ppage_pin (kva2pp ((u_long) ktmp->recv.r[scatptr].data));
	  len -= slen;
	  addr += slen;
	  pagebound = NBPG;
	  scatptr++;
	  if (scatptr >= AE_RECV_MAXSCATTER)
	    {
	      pktringent_free (ktmp);
	      warn ("pktringent_setup: too many scatter ptrs\n");
	      return (NULL);
	    }
	}
    }

  return (ktmp);
}


static void 
pktring_free (pktringent * ringhead)
{
  pktringent *ktmp;

  while (ringhead != NULL)
    {
      ktmp = ringhead->next;
      ringhead->next = ktmp->next;
      if (ktmp == ktmp->next)
	{
	  ringhead = NULL;
	}
      pktringent_free (ktmp);
    }
}


int 
pktring_adddpfref (int ringid, int filterid)
{
  if (ringused[ringid] == 0)
    return (-1);
  return (0);
}


void 
pktring_deldpfref (int ringid, int filterid)
{
}


int 
sys_pktring_setring (u_int sn, u_int k, u_int ringid,
		     struct pktringent *ringhead_user)
{
  int ringentries = 0;
  pktringent *utmp, *ktmp, *kprev = NULL;
  struct pktringent ringhead_kernel;

#if 0
  if (dpf_check_modify_access (curenv, k, ringid) == -1)
    return -E_NO_ACCESS;
#endif

  MP_SPINLOCK_GET(GLOCK(PKTRING_LOCK));
  if (ringid <= 0)
    {
      int i;
      for (i = 1; i <= MAX_PKTRING_COUNT; i++)
	{
	  if (ringused[i] == 0)
	    {
	      ringused[i] = 1;
	      ringid = i;
	      break;
	    }
	}
      if (ringid == 0)
	{
	  printf ("sys_pktring_setring: Out of pktrings\n");
	  MP_SPINLOCK_RELEASE (GLOCK(PKTRING_LOCK));
	  return (-E_NO_PKTRINGS);
	}
    }
  MP_SPINLOCK_RELEASE (GLOCK(PKTRING_LOCK));
  
  MP_SPINLOCK_GET(&ring_spinlocks[ringid]);

  pktring_free (rings[ringid]);
  rings[ringid] = NULL;

  utmp = ringhead_user;
  do
    {
      if (utmp == NULL)
	{
	  pktring_free (rings[ringid]);
          MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
	  return (-E_FAULT);
	}
      
      copyin (utmp, &ringhead_kernel, sizeof (ringhead_kernel));
      if ((ktmp = pktringent_setup (&ringhead_kernel)) == NULL)
	{
	  pktring_free (rings[ringid]);
          MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
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
	  rings[ringid] = ktmp;
	}

      kprev = ktmp;
      ringentries++;
      utmp = ringhead_kernel.next;
    }
  while (utmp != ringhead_user);

  /* GROK -- this is just a hack to control printing of warning messages when
   * a packet ring overflows.  It is nice to know, but irritating with the ARP
   * ring (which overflows for no bad reason... */

  if (ringid > 1)
    notify_when_empty[ringid] = 1;

  MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
  return (ringid);
}


/****************************************/

int 
sys_pktring_modring (u_int sn, u_int k, u_int ringid,
		     struct pktringent *newentry_user,
		     struct pktringent *oldentry, int replace)
{
  pktringent *ktmp = NULL;
  pktringent *kprev;
  pktringent newentry_kernel;

  if (ringid < 0 || ringid >= MAX_PKTRING_COUNT)
    return -E_INVAL;

/*
   if (dpf_check_modify_access (curenv, k, ringid) == -1) 
   return (-E_NO_ACCESS);
 */

  if (oldentry == NULL)
    return (-E_INVAL);

  MP_SPINLOCK_GET(&ring_spinlocks[ringid]);

  kprev = rings[ringid];
  if (kprev == NULL)
    {
      printf ("pktring_modring: Can't mod a non-existent ring (%d)\n", ringid);

      MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
      return (-E_INVAL);
    }

  do
    {
      if (kprev->next->appaddr == oldentry)
	break;
      kprev = kprev->next;
    }
  while (kprev != rings[ringid]);

  if (kprev->next->appaddr != oldentry)
    {
      printf ("pktring_modring: Can't find matching pktringent for %p\n", oldentry);
      MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
      return (-E_NOT_FOUND);
    }

  if (newentry_user)
    {
      copyin (newentry_user, &newentry_kernel, sizeof (newentry_kernel));
      if ((ktmp = pktringent_setup (&newentry_kernel)) == NULL)
	{
	  printf ("pktring_modring: Failed to setup new pktringent\n");
          MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
	  return (-E_NO_PKTRINGS);
	}
    }

  if (replace && newentry_user)
    {
      ktmp->next = kprev->next->next;
      if (rings[ringid] == kprev->next)
	{
	  rings[ringid] = ktmp;
	}
      pktringent_free (kprev->next);
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
      if (rings[ringid] = ktmp)
	{
	  rings[ringid] = ktmp->next;
	}
      pktringent_free (kprev->next);
    }

  MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
  return (0);
}


int 
sys_pktring_delring (u_int sn, u_int k, u_int ringid)
{
  if (ringid < 0 || ringid >= MAX_PKTRING_COUNT)
  {
    return -E_INVAL;
  }

  MP_SPINLOCK_GET (GLOCK(PKTRING_LOCK));
   
  if (ringused[ringid] == 0)
  {
    MP_SPINLOCK_RELEASE (GLOCK(PKTRING_LOCK));
    return (-E_NOT_FOUND);
  }

  MP_SPINLOCK_GET(&ring_spinlocks[ringid]);
  
  pktring_free (rings[ringid]);
  rings[ringid] = NULL;
  ringused[ringid] = 0;

  /* let the nettap functionality know that a pktring has been deleted */
  xokpkt_nettap_delpktring (ringid);

  MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
  MP_SPINLOCK_RELEASE (GLOCK(PKTRING_LOCK));
  return (0);
}


void 
pktring_handlepkt (int ringid, struct ae_recv *recv)
{
  int i;
  pktringent *ktmp;
  int len = ae_recv_datacnt (recv);
  int runlen = 0;

  if (ringid < 0 || ringid >= MAX_PKTRING_COUNT)
    {
      warn ("pktring_handlepkt: bogus ringid passed in");
      return;
    }

  MP_SPINLOCK_GET(&ring_spinlocks[ringid]);
   
  ktmp = rings[ringid];
  if ((ktmp == NULL) || (len == 0) || (*(ktmp->owner) != 0))
    {
      if (notify_when_empty[ringid])
	{
	  extern int dpf_ringval_getfid (int ringval);
	  printf ("pktring full (ringid %d, len %d, fid %d)\n", 
	      ringid, len, dpf_ringval_getfid (ringid));
	}
      
      MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
      return;
    }

  rings[ringid] = ktmp->next;
  for (i = 0; i < ktmp->recv.n; i++)
    {
      int tmplen = min ((len - runlen), ktmp->recv.r[i].sz);
      ae_recv_datacpy (recv, ktmp->recv.r[i].data, runlen, tmplen);
      runlen += tmplen;
      if (runlen >= len)
	{
	  break;
	}
    }

  if (runlen != len)
    {
      printf ("remaining runlen %d (len %d, recv.n %d, recv.r[0].sz %d)\n", runlen, len, ktmp->recv.n, ((ktmp->recv.n) ? ktmp->recv.r[0].sz : 0));
    }
  *(ktmp->owner) = len;

  MP_SPINLOCK_RELEASE(&ring_spinlocks[ringid]);
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif


