
/* 
 * Could use super-operators to nuke a bunch of the jumps.  Need to
 * write a generating program, first.
 */

#include <dpf/dpf-internal.h>
#include <dpf/hash.h>
#include <dpf/action.h>
#include <xok/printf.h>
#include <xok/malloc.h>

void action_run (struct action_block *prog, uint8 * msg);

#define op(opcode, nosib, nopid, nokid) \
	(((nokid) << 0) | ((nopid) << 1) | ((nosib) << 2) | ((opcode) << 3))

#define op2(opcode, nosib, nopid, nokid) \
	((1 << 4) | ((nokid) << 3) | ((nopid) << 2) | ((nosib) << 1) | (opcode))

#define assert(bool)            do { } while(0)
#define fatal(bool)             do { } while(0)
#define demand(bool, msg)       do { } while(0)
#ifndef NULL
#define NULL            0	/* null pointer constant */
#endif
#define AXTION
#undef printf(args... )         do { } while(0)
#undef DPFTRACE

#ifdef DPFTRACE
static int dpftrace = 0;
#endif

/* specialized to various special cases. */
typedef enum
  {
    /*      opcode          no sibs?  no pid?  no kid? */

    EQ = op (0, 0, 0, 0),
    EQ_NO_KID = op (0, 0, 0, 1),
    EQ_NO_PID = op (0, 0, 1, 0),
    EQ_NO_SIB = op (0, 1, 0, 0),
    EQ_NO_PID_NO_SIB = op (0, 1, 1, 0),


    DEQ = op (1, 0, 0, 0),

    DEQ_NO_KID = op (1, 0, 0, 1),
    DEQ_NO_PID = op (1, 0, 1, 0),
    DEQ_NO_SIB = op (1, 1, 0, 0),
    DEQ_NO_PID_NO_SIB = op (1, 1, 1, 0),

#ifdef AXTION
    SEQ = op (2, 0, 0, 0),

    SEQ_NO_KID = op (2, 0, 0, 1),
    SEQ_NO_PID = op (2, 0, 1, 0),
    SEQ_NO_SIB = op (2, 1, 0, 0),
    SEQ_NO_PID_NO_SIB = op (2, 1, 1, 0),
#endif

    SHIFT,
#ifdef AXTION
    ACTION,
#endif

    /* completely fast case: no pid, no sibs, kid, aligned, no mask */
    EQ_X_8,
    EQ_X_16,
    EQ_X_32,
    /* with mask */
    EQ_MX_8,
    EQ_MX_16,
    EQ_MX_32,
    SUPER_OP_32X_8MX,
    DONE
  }
state_t;

static struct atom done_atom;
static const void **jump_table;

static int inline
load_lhs (unsigned *lhs, uint8 * msg, unsigned nbytes, struct ir *ir)
{
  unsigned off;

  off = ir->u.eq.offset;
  if (off >= nbytes)
    return 0;
  memcpy (lhs, msg + off, sizeof off);
  *lhs &= ir->u.eq.mask;
  return 1;
}

static inline Atom 
ht_lookup (Ht ht, uint32 val)
{
  Atom hte;

  assert (ht);
  for (hte = ht->ht[hash (ht, val)]; hte; hte = hte->next)
    if (hte->ir.u.eq.val == val)
      return hte;
  return 0;
}

static int
fast_interp (uint8 * msg, unsigned nbytes, Atom a, int fail_pid, Atom * orstack, struct frag_return *retmsg)
{

  /* orstack is a simple stack for recursion in the decision tree, the next
   * macro is used to operate on this tree */
#define next 						\
	if(!a) {					\
		if(fail_pid != 0)			\
			return fail_pid;		\
 		a = *--orstack;				\
	}						\
	goto *a->code;					\

  static const void *jtable[] =
  {
    [EQ_NO_KID] = &&EQ_NO_KID,
  
    /* the rest have kids. */
    [EQ] = &&EQ,
    [EQ_NO_SIB] = &&EQ_NO_SIB,
    [EQ_NO_PID] = &&EQ_NO_PID,
    [EQ_NO_PID_NO_SIB] = &&EQ_NO_PID_NO_SIB,

    [DEQ] = &&DEQ,
    [DEQ_NO_KID] = &&DEQ_NO_KID,
    [DEQ_NO_PID] = &&DEQ_NO_PID,
    [DEQ_NO_SIB] = &&DEQ_NO_SIB,
    [DEQ_NO_PID_NO_SIB] = &&DEQ_NO_PID_NO_SIB,

#ifdef AXTION
    [SEQ] = &&SEQ,
    [SEQ_NO_KID] = &&SEQ_NO_KID,
    [SEQ_NO_PID] = &&SEQ_NO_PID,
    [SEQ_NO_SIB] = &&SEQ_NO_SIB,
    [SEQ_NO_PID_NO_SIB] = &&SEQ_NO_PID_NO_SIB,
#endif

    [EQ_X_8] = &&EQ_X_8,
    [EQ_X_16] = &&EQ_X_16,
    [EQ_X_32] = &&EQ_X_32,
    [EQ_MX_8] = &&EQ_MX_8,
    [EQ_MX_16] = &&EQ_MX_16,
    [EQ_MX_32] = &&EQ_MX_32,

    [SUPER_OP_32X_8MX] = &&SUPER_OP_32X_8MX,

    [SHIFT] = &&SHIFT,
#ifdef AXTION
    [ACTION] = &&ACTION,
#endif
    [DONE] = &&DONE
  };

  unsigned lhs, off, pid, val;
  struct ir *ir;
#ifdef AXTION
  struct stateeq *st;
#endif
  Atom n, hte;

  off = 0;
  hte = NULL;

  if (!a)
    {
      /* hack to let outsider extract jump table. */
      jump_table = jtable;
      return fail_pid;
    }

  goto *a->code;

EQ:
  assert (a->kids.lh_first);
  assert (a->sibs.le_next);
  assert (a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("e, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || lhs != ir->u.eq.val)
    a = a->sibs.le_next;
  else
    {
      n = a->sibs.le_next;
      fail_pid = a->pid;
      if (n)
	*orstack++ = n;
      a = a->kids.lh_first;
    }
  next;

EQ_NO_KID:
  assert (a->sibs.le_next);
  assert (!a->kids.lh_first);
  assert (a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ek, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || lhs != ir->u.eq.val)
    {
      a = a->sibs.le_next;
      next;
    }
  else
      return a->pid;

EQ_NO_PID:
  assert (a->sibs.le_next);
  assert (a->kids.lh_first);
  assert (!a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ep, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || lhs != ir->u.eq.val)
    a = a->sibs.le_next;
  else
    {
      if ((n = a->sibs.le_next))
	*orstack++ = n;
      a = a->kids.lh_first;
    }
  next;

EQ_NO_SIB:
  assert (!a->sibs.le_next);
  assert (a->kids.lh_first);
  assert (a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("es, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || lhs != ir->u.eq.val)
    {
      a = 0;
    }
  else
    {
      fail_pid = a->pid;
      a = a->kids.lh_first;
    }
  next;

EQ_NO_PID_NO_SIB:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("eps, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || lhs != ir->u.eq.val)
    {
      a = 0;
    }
  else
    {
      a = a->kids.lh_first;
    }
  next;

  /* 
   * completely fast case: no pid, no sibs, kid, aligned, no mask 
   * (eliminate offset check?
   */
EQ_MX_16:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
  /* alignment. */
  assert ((unsigned long) &msg[off] % 2 == 0);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("emx16, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes)
    {
      val = *(unsigned short *) &msg[off] & a->ir.u.eq.mask;
      if (val == a->ir.u.eq.val)
	{
	  a = a->kids.lh_first;
	  next;
	}
    }
  a = 0;
  next;

EQ_X_8:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ex8, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes
      && msg[off] == a->ir.u.eq.val)
    {
      a = a->kids.lh_first;
      next;
    }
  a = 0;
  next;

EQ_X_16:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
  /* alignment. */
  assert ((unsigned long) &msg[off] % 2 == 0);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ex16, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes
      && *(unsigned short *) &msg[off] == a->ir.u.eq.val)
    {
      a = a->kids.lh_first;
      next;
    }
  a = 0;
  next;

EQ_MX_8:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
  assert (!a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("emx8, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes)
    {
      val = msg[off] & a->ir.u.eq.mask;
      if (val == a->ir.u.eq.val)
	{
	  a = a->kids.lh_first;
	  next;
	}
    }
  a = 0;
  next;

EQ_X_32:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
  /* alignment. */
  assert ((unsigned long) &msg[off] % 4 == 0);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ex32, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes
      && *(unsigned *) &msg[off] == a->ir.u.eq.val)
    {
      a = a->kids.lh_first;
      next;
    }
  a = 0;
  next;

EQ_MX_32:
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);
  /* alignment. */
  assert ((unsigned long) &msg[off] % 4 == 0);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("emx32, ");
#endif

  if ((off = a->ir.u.eq.offset) < nbytes)
    {
      val = *(unsigned *) &msg[off] & a->ir.u.eq.mask;
      if (val == a->ir.u.eq.val)
	{
	  a = a->kids.lh_first;
	  next;
	}
    }
  a = 0;
  next;

DEQ:
  assert (a->sibs.le_next);
  assert (a->ht->nterm);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("d, ");
#endif
  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || !(hte = ht_lookup (a->ht, lhs)))
    {
      a = a->sibs.le_next;
      next;
    }
  else
    {
      n = a->sibs.le_next;
      assert (hte->pid);
      fail_pid = hte->pid;
      if (n)
	*orstack++ = n;
      a = hte->kids.lh_first;
      next;
    }

DEQ_NO_KID:
  assert (!a->ht->nterm);
  assert (a->sibs.le_next);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("dk, ");
#endif
  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || !(hte = ht_lookup (a->ht, lhs)))
    {
      if (a->sibs.le_next)
	{
	  a = a->sibs.le_next;
	  next;
	}
      else
	return fail_pid;
    }
  else
    {
      assert (hte->pid);
      return hte->pid;
    }

DEQ_NO_PID:
  assert (a->ht->nterm);
  assert (a->sibs.le_next);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("dp, ");
#endif
  demand (!a->pid, should not have a pid);
  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || !(hte = ht_lookup (a->ht, lhs)))
    {
      a = a->sibs.le_next;
    }
  else
    {
      n = a->sibs.le_next;
      if (n)
	*orstack++ = n;
      a = hte->kids.lh_first;
    }
  next;

DEQ_NO_SIB:
  assert (a->ht->nterm);
  assert (!a->sibs.le_next);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("ds, ");
#endif
  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || !(hte = ht_lookup (a->ht, lhs)))
    {
      a = 0;
    }
  else
    {
      assert (hte->pid);
      fail_pid = hte->pid;
      a = hte->kids.lh_first;
    }
  next;

DEQ_NO_PID_NO_SIB:
  assert (a->ht->nterm);
  assert (!a->sibs.le_next);
  assert (!a->pid);
#ifdef DPFTRACE
  if (dpftrace)
    printf ("dps, ");
#endif

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir)
      || !(hte = ht_lookup (a->ht, lhs)))
    {
      a = 0;
    }
  else
    {
      a = hte->kids.lh_first;
    }
  next;

SHIFT:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("s, ");
#endif
  if (load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      off = lhs << ir->u.shift.shift;
      if (off <= nbytes
	  && (pid = fast_interp (msg + off, nbytes - off,
				 a->kids.lh_first, a->pid, orstack, retmsg)))
	return pid;
    }
  a = a->sibs.le_next;
  next;

#ifdef AXTION
ACTION:
  if (load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      struct state *scur;
#ifdef DPFTRACE
      printf ("action running: %08x mgs: %08x\n", (int) ir->action, (int) msg);
#endif

      for (scur = ir->action->state; scur->next2 != NULL; scur = scur->next2);	
      /* to get to end of list */
      scur->next2 = calloc (1, sizeof (struct state));
      scur->state = lhs;
      retmsg->headtail = 1;	/* hmmm wrong. only return this when tails
				   passed through? but that is hard.
				   but when else can we make the return
				   value negative? */
      retmsg->msgid = scur->state;
      return a->pid;
      
      if (a->sibs.le_next)
	{
	  a = a->sibs.le_next;
	  next;
	}
      else
	return a->pid;
    }

SEQ:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("seq, ");
#endif
  assert (a->kids.lh_first);
  assert (a->sibs.le_next);
  assert (a->pid);

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      a = a->sibs.le_next;
    }
  else
    {
      struct state *cur;
      st = &ir->u.stateeq;

      for (cur = st->state; cur->next2 != NULL; cur = cur->next2)
	{
	  if (lhs == (cur->state & st->mask))
	    {
	      n = a->sibs.le_next;
	      fail_pid = a->pid;
	      if (n)
		*orstack++ = n;
	      a = a->kids.lh_first;
	      next;
	    }
	}
      retmsg->headtail = 2;
      retmsg->msgid = lhs;
      fail_pid = -(a->pid);
      a = a->sibs.le_next;
    }
  next;

SEQ_NO_KID:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("seq_nokid, ");
#endif
  assert (a->sibs.le_next);
  assert (!a->kids.lh_first);
  assert (a->pid);

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      if (a->sibs.le_next)
	{
	  a = a->sibs.le_next;
	  next;
	}
      else
	return fail_pid;
    }
  else
    {
      struct state *cur;
      st = &ir->u.stateeq;

      for (cur = st->state; cur->next2 != NULL; cur = cur->next2)
	{
	  if (lhs == (cur->state & st->mask))
	    {
	      return a->pid;
	    }
	}
      retmsg->headtail = 2;
      retmsg->msgid = lhs;
      fail_pid = -(a->pid);
      a = a->sibs.le_next;
    }
  next;

SEQ_NO_PID:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("seq_nopid, ");
#endif
  assert (a->sibs.le_next);
  assert (a->kids.lh_first);
  assert (!a->pid);

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      if (a->sibs.le_next)
	{
	  a = a->sibs.le_next;
	  next;
	}
      else
	return fail_pid;
    }
  else
    {
      struct state *cur;
      st = &(ir->u.stateeq);

      cur = st->state;
      if (cur != NULL && cur->state != NULL)
	{
	  for (; cur->next2 != NULL; cur = cur->next2)
	    {
#ifdef DPFTRACE
	      printf ("testing match: 0x%08x with lhs: 0x%08x\n", (cur->state[st->stateoffset] & st->mask), lhs);
#endif
	      if (lhs == (cur->state & st->mask))
		{
		  if ((n = a->sibs.le_next))
		    *orstack++ = n;
		  a = a->kids.lh_first;
		  next;
		}
	    }
	}
      retmsg->headtail = 2;
      retmsg->msgid = lhs;
      fail_pid = -(a->pid);
      a = a->sibs.le_next;
    }
  next;

SEQ_NO_SIB:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("seq_nosib, ");
#endif
  assert (!a->sibs.le_next);
  assert (a->kids.lh_first);
  assert (a->pid);

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      a = 0;
      next;
    }
  else
    {
      struct state *cur;
      st = &ir->u.stateeq;

      for (cur = st->state; cur->next2 != NULL; cur = cur->next2)
	{
	  if (lhs == (cur->state & st->mask))
	    {
	      fail_pid = a->pid;
	      a = a->kids.lh_first;
	      next;
	    }
	}
      retmsg->headtail = 2;
      retmsg->msgid = lhs;
      fail_pid = -(a->pid);
      a = 0;
      next;
    }

SEQ_NO_PID_NO_SIB:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("seq_nopidsib, ");
#endif
  assert (a->kids.lh_first);
  assert (!a->sibs.le_next);
  assert (!a->pid);

  if (!load_lhs (&lhs, msg, nbytes, ir = &a->ir))
    {
      a = 0;
      next;
    }
  else
    {
      struct state *cur;
      st = &ir->u.stateeq;

      for (cur = st->state; cur->next2 != NULL; cur = cur->next2)
	{
	  if (lhs == (cur->state & st->mask))
	    {
	      a = a->kids.lh_first;
	      next;
	    }
	}
      retmsg->headtail = 2;
      retmsg->msgid = lhs;
      fail_pid = -(a->pid);
      a = 0;
      next;
    }
#endif

SUPER_OP_32X_8MX:
  *(unsigned volatile *) 0 = 0;
  *(unsigned volatile *) 0 = 0;
  *(unsigned volatile *) 0 = 0;
  *(unsigned volatile *) 0 = 0;
#if 0
  *(unsigned volatile *) 0 = 0;
  *(unsigned volatile *) 0 = 0;

  if ((off = a->ir.u.eq.offset) < nbytes
      && *(unsigned *) &msg[off] == a->ir.u.eq.val)
    {
      a = a->kids.lh_first;
      if ((off = a->ir.u.eq.offset) < nbytes)
	{
	  val = msg[off] & a->ir.u.eq.mask;
	  if (val == a->ir.u.eq.val)
	    {
	      a = a->kids.lh_first;
	      next;
	    }
	}
    }
  a = 0;
  next;
#endif

DONE:
#ifdef DPFTRACE
  if (dpftrace)
    printf ("done\n");
#endif
  return fail_pid;

  fatal (should not get here);
}

static int 
dpf_interp (uint8 * msg, unsigned nbytes, struct frag_return *retmsg)
{
  Atom orstack[256];

  orstack[0] = &done_atom;
  retmsg->headtail = 0;
  return fast_interp (msg, nbytes, dpf_base->kids.lh_first, 0, orstack + 1, retmsg);
}

int (*dpf_compile (Atom trie)) ()
{
  return dpf_interp;
}

/*
 * Compute what special-case optimizations we can do.  Currently
 * optimize for:
 *      1. hash table has no collisions.
 *      2. hash table has only terminals.
 *      3. hash table has only non-terminals.
 */
unsigned 
ht_state (Ht ht)
{
  unsigned state;

  state = 0;

  /* no collisions means we can elide collision checks. */
  if (!ht->coll)
    state |= DPF_NO_COLL;

  /*
   * note: when the hash table is empty, it will trigger ALL_NTERMS,
   * HOWEVER, the hash table will be set to REGEN, which will
   * make ht_regenp return 1.  A bit sleazy.  Weep.
   */
  if (!ht->nterm)
    state |= DPF_ALL_TERMS;
  else if (!ht->term)
    state |= DPF_ALL_NTERMS;

  return state;
}

void 
dpf_dump (void *ptr)
{

  if (ptr == dpf_interp)
    printf ("<interpreter routine>\n");
  /* dump the code */
  else
    {
      printf ("<>\n");
    }
}

/* label with the right state. */
void 
dpf_compile_atom (Atom a)
{
  struct ir *ir;
  unsigned op, off;

  ir = &a->ir;
  op = 0;

//      printf("a: %x sib: %08x\n", (int)a, (int)a->sibs.le_next);

  if (!jump_table)
    fast_interp (0, 0, 0, 0, 0, 0);
  if (dpf_isshift (ir))
    {
      op = SHIFT;
#ifdef AXTION
    }
  else if (dpf_isaction (ir))
    {
      op = ACTION;
      ir->u.eq.offset = ir->action->coffset;
    }
  else if (dpf_isstateeq (ir))
    {
      if (!a->kids.lh_first)
	op = SEQ_NO_KID;
      if (!op)
	op = op (2, (a->sibs.le_next == 0), (a->pid == 0), 0);
#endif
    }
  else if (a->ht)
    {
      if (!a->ht->nterm)
	op = DEQ_NO_KID;
      else
	op = op (1, (a->sibs.le_next == 0), 1, 0);
    }
  else
    {
      if (!a->kids.lh_first)
	{
#if 0
	  if (a->sibs.le_next == 0)
	    {
	      Atom cur = a;

	      while (!a->sibs.le_next && cur)
		cur = cur->parent;
	      if (cur)
		{
		  printf ("setting sib to: %08x\n", (int) cur->sibs.le_next);
		  a->sibs.le_next = cur->sibs.le_next;
		}
	    }
#endif

	  op = EQ_NO_KID;
	}
      else if (!a->pid && !a->sibs.le_next)
	{
	  off = ir->alignment + ir->u.eq.offset;
	  /* 
	   * completely fast case: no pid, no sibs, kid, 
	   * aligned, no mask (eliminate offset check?)
	   */
	  switch (ir->u.eq.nbits)
	    {
	    case 8:
	      if (ir->u.eq.mask == 0xff)
		op = EQ_X_8;
	      else
		op = EQ_MX_8;
	      break;
	    case 16:
	      if (off % 2 == 0)
		{
		  if (ir->u.eq.mask == 0xffff)
		    op = EQ_X_16;
		  else
		    op = EQ_MX_16;
		}
	      break;
	    case 32:
	      if (off % 4 == 0)
		{
		  if (ir->u.eq.mask == 0xffffffff)
		    op = EQ_X_32;
		  else
		    op = EQ_MX_32;
		}
	      break;
	    }
	}
      if (!op)
	op = op (0, (a->sibs.le_next == 0), (a->pid == 0), 0);

    }
  a->code = (void *) jump_table[op];
  demand (a->code, Bogus op !);
  done_atom.code = (void *) jump_table[DONE];
#if 0
  if (a->code == jump_table[EQ_X_32])
    {
      k = a->kids.lh_first;
      assert (k->code);
      if (k && k->code == jump_table[EQ_MX_8])
	{
	  a->code = jump_table[SUPER_OP_32X_8MX];
	}
    }
#endif
}
