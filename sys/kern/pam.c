#include <xok/capability.h>
#include <xok/sys_proto.h>
#include <xok/types.h>
#include <xok/printf.h>
#include <xok/mmu.h>
#include <xok/pctr.h>
#include <xok/env.h>
#include <xok/syscallno.h>
#include <xok/trap.h>
#include <xok/kerrno.h>
#include <xok_include/assert.h>
#include "kern_pam.h"
#include <xok/pmap.h>

//
// The abstraction table (abs_tab) is the data structure that helps
// xok keep track of currently defined abstractions. Each entry in
// abs_tab corresponds to an abstraction. Every abstraction is defined
// by a mtab_t (method table) which contains the number of methods
// that the abstraction has, the capability that protects the shared
// state, and each method's offset (relative to
// PROT_AREA_BOT). next_abs simply keeps track of the next entry to be
// filled in. 
//

//
// NOTATION:   
//             UP = user space pointer
//             KP = kernel space pointer
//

mtab_t  abs_tab [MAX_NABS];
uint    next_abs = 0;

//===========================================================================
// sys_prot_define is used to define a new abstraction.

int 
sys_prot_define (uint sn,          // system call number               
		 uint mtab_addr,   // user address of abstraction's method table
		 uint slot,        // slot that contains the cap for shared state
		 uint nbytes,      // size (in bytes) of abs. (beginning at 1st method)
		 uint metadata_va, // the bootstrap va
		 uint *handlers_p  // ptr to handlers structure
		 )

{
  mtab_t       *UP_mtab = (mtab_t *) mtab_addr;
  mtab_t       *KP_abs  = &abs_tab[next_abs];
  mtab_t       *KP_mtab = (mtab_t *) ptov (va2pa (UP_mtab));
  int r;
  cap          KP_cap;
  int          i;
  uint         va, num_completed;
  Pte          pte;
  struct Ppage *pp;
  metadata_t   *kptr;
  
  r = env_getcap (curenv, slot, &KP_cap);
  if (r < 0) return r;

  // Validate the arguments and make sure we have enough room to
  // define a new abstraction

  ensure (next_abs < MAX_NABS,       PROT_NABS);
  ensure (nbytes < MAX_SIZE,         PROT_TOO_BIG);
  ensure ((uint) KP_mtab > KERNBASE, PROT_BAD_VA);

  // We expect to find at KP_mtab a mtab_t struct that describes the
  // abstraction's methods. Make sure the number of methods is within
  // admissable limits. Note that we may end up mapping parts of the
  // code that were not intended to be seen by others if they are
  // contained in the protmeth pages.

  va = PGROUNDDOWN ((uint) KP_mtab->start[0]);
  ensure (KP_mtab->nmethods <= MAX_NMETHODS, PROT_NMETHS);
  ensure (va >= PROT_AREA_BOT,               PROT_LOW);
  ensure (va + nbytes < PROT_AREA_TOP,       PROT_HIGH);

  // Now fill in the relevant abs_tab entry for the new abstraction. 

  KP_abs->state_cap = KP_cap;
  KP_abs->nmethods = KP_mtab->nmethods;
  for (i=0 ; i < KP_mtab->nmethods ; i++) {
    KP_abs->start[i] = KP_mtab->start[i];
  }
  
  kptr = (metadata_t *) KERN(handlers_p);
  ensure ((uint) kptr > KERNBASE, PROT_BAD_VA);

  KP_abs->handlers.entepilogue = kptr->entepilogue;
  KP_abs->handlers.entprologue = kptr->entprologue;
  KP_abs->handlers.entfault    = kptr->entfault;
  KP_abs->handlers.entipc1     = kptr->entipc1;
  KP_abs->handlers.entipc2     = kptr->entipc2;
  KP_abs->handlers.entipca     = kptr->entipca;
  KP_abs->handlers.entrtc      = kptr->entrtc;
  
  // Allocate and map a metadata page for the
  // abstraction. pmap_insert_range takes care of va being a valid
  // address.

  KP_abs->va = metadata_va;
  pte = 0 | PG_U | PG_W | PG_P;
  num_completed = 0;
  i = pmap_insert_range (SN, CAP_PROT, &pte, 1, metadata_va, &num_completed,
			 0, 0, 0);
  ensure (i == 0,  PROT_NO_PAGE);

  // Make sure that the physical page (with metadata) stays
  // around. This hack will not let ppage_free get rid of the
  // page. This is really not done right; we should rely instead on
  // the defining process remaining alive and thus keeping the
  // reference counts right.

  pte = *pt_get_ptep (curenv->env_pdir, metadata_va);
  pp = ppages_get(pte >> PGSHIFT);
  INC_FIELD(pp,Ppage,pp_writers,1); // pp->pp_writers++;
  INC_FIELD(pp,Ppage,pp_readers,1); // pp->pp_readers++;
  INC_FIELD(pp,Ppage,pp_refcnt,1); // pp->pp_refcnt++;
  KP_abs->pte = pte;  // ... so that we know what to map in sys_prot_call

  // Figure out which (and how many) pages contain the code of this
  // abstraction. They need to be mapped whenever a protmeth belonging
  // to this abstraction is invoked. Do the same ugly hack to make
  // sure the pages stick around. 

  for (va = KP_abs->start[0] ; va < KP_abs->start[0] + nbytes ; va += NBPG, i++) {
    pp = ppages_get(PGNO (va2pa ((void *) va)));
    KP_abs->ptes[i] = pp2pa(pp) | PG_U | PG_P | PG_A;
    INC_FIELD(pp,Ppage,pp_refcnt,1); // pp->pp_refcnt++;
    INC_FIELD(pp,Ppage,pp_readers,1); // pp->pp_readers++;
  }

  KP_abs->npages = i;

  // Bump the abstraction index an return

  next_abs ++;
  return PROT_OK;
}

//===========================================================================
// sys_prot_call is used to invoke a protected method (as specified by
// id) and pass it arguments (pointed to by arg_ptr). This syscall has only
// three args so it can take the fast path. Unless you have a good reason,
// don't change the number of args.

int sys_prot_call (u_int sn,       // system call number
		   u_int id,       // desired abs. and method and code map bit
		   void *arg_ptr   // pointer to protmeth's arguments
		   )
{
  mtab_t     *KP_abs;
  int        i, res;
  uint       abs_id = ABS_ID(id);
  uint       meth_id = METH_ID(id);
  uint       map_code = MAP_BIT(id);
  uint       num_completed;
  Pte        *ptep;

  // Validate the arguments

  ensure (abs_id >= 0  &&  abs_id < next_abs, PROT_NO_ABS);
  KP_abs = &abs_tab[abs_id];
  ensure (meth_id >= 0  &&  meth_id < KP_abs->nmethods, PROT_NO_METH);
  
  // Insert the capability associated with the requested abstraction
  // in slot CAP_PROT of the caller's env
  
  curenv->env_clist[CAP_PROT] = KP_abs->state_cap;

  // Map the metadata page on the protected method's behalf, if
  // necessary. In the common case (page is mapped, but disabled), we
  // just need to flip one bit and save the cost of an entire
  // remap. The protmeth's stack starts at the end of the metadata
  // page (and grows towards its beginning).

  pt_check_n_alloc (curenv->env_pdir, KP_abs->va);
  ptep = pt_get_ptep (curenv->env_pdir, KP_abs->va);
  
  if (*ptep >> PGSHIFT  ==  KP_abs->pte >> PGSHIFT)
    *ptep |= PG_U;  // just flip the protection bit
  else {
    num_completed = 0;
    if (pmap_insert_range (SN, CAP_PROT, &KP_abs->pte, 1, KP_abs->va,
			   &num_completed, 0, 0, 0) < 0)
      return PROT_NO_MAP;
  }
  
  // Install the abstraction's fault handlers. The old entry points
  // are saved in the caller's environment (so we can restore them
  // upon return). Change the handler entry points in the caller's
  // environment. 

  {
    struct Uenv *p  = curenv->env_u;
    metadata_t *kp = &KP_abs->handlers;

    p->prot_entepilogue = p->u_entepilogue;
    p->prot_entprologue = p->u_entprologue;
    p->prot_entfault    = p->u_entfault;
    p->prot_entipc1     = p->u_entipc1;
    p->prot_entipc2     = p->u_entipc2;
    p->prot_entipca     = p->u_entipca;
    p->prot_entrtc      = p->u_entrtc;

    p->u_entepilogue = kp->entepilogue;
    p->u_entprologue = kp->entprologue;
    p->u_entfault    = kp->entfault;
    p->u_entipc1     = kp->entipc1;
    p->u_entipc2     = kp->entipc2;
    p->u_entipca     = kp->entipca;
    p->u_entrtc      = kp->entrtc;
  }

  // Map the pages containing the invoked methods, if the caller
  // thinks this is necessary. 
  
  if (map_code) {
    uint num_completed;

    for (i=0 ; i < KP_abs->npages ; i++) {
      
      Pte  pte = KP_abs->ptes[i];
      Pte  *ptep;
      uint va  = KP_abs->start[0] + i*NBPG;
      
      pt_check_n_alloc (curenv->env_pdir, va);
      ptep = pt_get_ptep (curenv->env_pdir, va);
      if (pte != *ptep) {
	num_completed = 0;
	res = pmap_insert_range (SN, 11, &pte, 1, va, &num_completed, 0, 0, 0);
	if (res < 0) {
	  DBG ("sys_prot_call: Couldn't insert 0x%x -> 0x%x\n", va, pte);
	  return res;
	}
      }
    }
  }

  // Set up the protmeth stack and the caller's stack. Then switch to
  // the protmeth's stack.
  
  {
    #define SET(addr,val) (* ((uint *) (addr)) = (uint) (val))
    
    uint u_caller_esp = utf->tf_esp;
    uint k_caller_esp = (uint) KERN(u_caller_esp);
    uint u_prot_esp = utf->tf_esp - 0x100;
    // uint u_prot_esp = KP_abs->va + 0x100;
    uint k_prot_esp = (uint) KERN(u_prot_esp);

    SET(k_caller_esp-4, utf->tf_eip);  // push onto caller's stack its ret. addr.
    SET(k_prot_esp-4, u_caller_esp-4); //  onto protmeth's stack the caller %esp

    SET(k_prot_esp-8, arg_ptr);        // push onto protmeth's stack the argument ptr
    utf->tf_esp = u_prot_esp-8;        // Set esp in trap frame to protmeth's stack
  }
  
  utf->tf_eip = KP_abs->start[meth_id];  // IRET into the protmeth entry point
  curenv->prot_abs_id = abs_id;

  return 0;
}

//===========================================================================
// This system call enhances the security of the system :-)  (just kidding)
// NEED TO ELIMINATE THIS ONCE WE ARE DONE TESTING

void sys_prot_wipeout (uint sn)
{
  next_abs = 0;
}

void sys_prot_exit (uint sn)
{
  uint        abs_id = curenv->prot_abs_id;
  uint        va = abs_tab[abs_id].va;
  Pde *const  pd = curenv->env_pdir;
  struct Uenv *p = curenv->env_u;
  Pte         *ptep;

  //-----------------------------------------------------------------
  // Make the metadata page unavailable

  ptep = pt_get_ptep (pd, va);
  *ptep &= ~PG_U;
  tlb_invalidate (va, curenv->env_pd->envpd_id);

  //-----------------------------------------------------------------
  // Invalidate the capability

  curenv->env_clist[CAP_PROT].c_valid = 0;

  //-----------------------------------------------------------------
  // Restore the fault handlers

  p->u_entepilogue = p->prot_entepilogue;
  p->u_entprologue = p->prot_entprologue;
  p->u_entfault    = p->prot_entfault;
  p->u_entipc1     = p->prot_entipc1;
  p->u_entipc2     = p->prot_entipc2;
  p->u_entipca     = p->prot_entipca;
  p->u_entrtc      = p->prot_entrtc;

  curenv->prot_abs_id = -1;
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#endif

