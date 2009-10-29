
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

#include <vcode/vcode.h>

#include <xok_include/ctype.h>
#include <xok_include/assert.h>

#ifdef EXOPC
#include <xok/printf.h>
#include <xok/malloc.h>
#else
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif

#define NUM_PHYS_REGS 6 /* number of physical registers we use */
#define MAX_PHYS_REG 8 /* max number of a physical register */

/* prototypes for internal functions */

static void registers_start ();
static void registers_end ();
static void labels_start ();
static int labels_end ();
static v_code *proc_prologue ();
static void proc_epilogue (v_code *backpatch);
static v_reg_t v_getvirt ();

#if 0
register v_code *v_ip asm ("ebx");		/* next space to gen code to */
#else
v_code *v_ip;
#endif

v_label_t v_epilouge_label;

/*
 * ----------------------------------------------------------------------
 * 		    Register Manipulation Routines
 * ----------------------------------------------------------------------
 */


/* remember class of each physical register (V_TEMP or V_VAR). Indexed
   by physical register name (EAX, EDX, etc). */

static int reg_types[MAX_PHYS_REG] = {0};
static const int phys_regs_order[] = {__EAX, __EDX, __ECX, __EBX, __ESI, __EDI};
static int phys_regs_alloced[] = {0, 0, 0, 0, 0, 0};
static const int caller_saved_regs[] = {__EAX, __EDX, __ECX};
static const int callee_saved_regs[] = {__EBX, __ESI, __EDI};
static int next_virtual_reg = 0;
static int num_phys_reg = 0;

/* called when we start generating a procedure */

static void registers_start () {
  num_phys_reg = 0;
  next_virtual_reg = 1;
  memset (phys_regs_alloced, 0, sizeof (phys_regs_alloced));
  memset (reg_types, 0, sizeof (reg_types));
}

int v_getreg (v_reg_t *r,	/* reg struct we fill in with new reg */
	      int type,		/* type of value to be stored in r */
	      int class) {	/* temp or perst value */

  /* we allways try to allocate in this order:
       1) caller saved register
       2) callee saved register
       3) virtual register

     If this is a temp register and it gets assigned to a caller saved
     register, we don't save it on a CALL. */

  if (num_phys_reg == NUM_PHYS_REGS) {
    *r = v_getvirt ();
    return (1);
  } else {
    int i;
    for (i = 0; i < NUM_PHYS_REGS; i++) {
      if (phys_regs_alloced[i]) {
	continue;
      }
      phys_regs_alloced[i] = 1;
      num_phys_reg++;
      reg_types[phys_regs_order[i]] = class;
      *r = __MKNVIRT (phys_regs_order[i]);
      return (1);
    }
  }

  return (0);
}
    
/* allocate a virtual register */
static v_reg_t v_getvirt () {
    int t = next_virtual_reg++;
    return (__MKVIRT(t*4));
}
  
int v_putreg (v_reg_t r,	/* register to free */
	      int type) {	/* type of r */

  /* XXX -- we currently don't free virtual registers */

  if (__NVIRT(r)) {
    int i;

    for (i = 0; i < NUM_PHYS_REGS; i++) {
      if (phys_regs_order[i] == __REG(r)) {
	break;
      }
    }
    assert (i != MAX_PHYS_REG);
    phys_regs_alloced[i] = 0;
    num_phys_reg--;
  }

  return (0);
}

int v_local (int type) {	/* type of value to alloc space for */
  /* just return a virtual register */
  return (__MKVIRT (4*next_virtual_reg++));
}

int v_localb (unsigned size) {	/* number of bytes needed */
  /* we just allocate size/4+1 virtual registers and return the address
    of the first one. */

  next_virtual_reg += size/4+1;
  return (v_local (0));
}
  
#if 0

#define v_save(type)                                                   \
int v_save##type (v_reg_t r) {                                      \
                                                                       \
  /* only deal with physical registers since all virtual registers     \
     are preserved across calls */                                     \
                                                                       \
  if (NVIRT(r)) {                                                      \
    reg_types[r] = V_VAR;	/* force reg to be saved */            \
  }                                                                    \
}

v_save(i)
v_save(ui)
v_save(l)
v_save(ul)

#define v_restore(type)
int 

#endif

/* called when done generating procedure. */
static void registers_end () {
}

void register_test () {
  v_reg_t r;
  int i;

  for (i = 0; i < 10; i++) {
    if (v_getreg (&r, V_I, V_VAR) < 0) {
      printf ("out of registers\n");
      return;
    }
    if (__VIRT(r)) {
      printf ("alloced virtual register %d\n", __ADDR(r));
    } else {
      printf ("alloced physical register %d\n", __REG(r));
    }
  }

  printf ("putting register back...\n");

  v_putreg (r, V_VAR);

  r = 45645;

  v_getreg (&r, V_I, V_VAR);

  printf ("re-alloced %d\n", r);

  r = v_localb (13);

  printf ("alloced 13 bytes of local space at %d\n", r);
}

/*
 * ----------------------------------------------------------------------
 * 		    Label Mangement Routines
 * ----------------------------------------------------------------------
 */

/* build up a table of lables, the addresses they refer to, and 
   any other refs we have to fill in once we know the label's
   address. */

#define MAX_LABELS 256
#define MAX_REFS 128

struct v_label_rec {
  void *refs[MAX_REFS];
  enum {RELATIVE, ABSOLUTE} type[MAX_REFS];
  int num_refs;
  v_code *addr;
};
typedef struct v_label_rec v_label_rec;

static v_label_rec labels[MAX_LABELS];
static int last_label = 0;

/* called when we start building a procedure. */

static void labels_start () {
  last_label = 0;
}

/* let someone find out where a label is placed */

v_code *v_getlabel (v_label_t l) {
  assert (l < MAX_LABELS);
  return (labels[l].addr);
}

v_label_t v_genlabel (void) {
  assert (last_label < MAX_LABELS);
  labels[last_label].num_refs = 0;
  labels[last_label].addr = (v_code *)0;
  return (last_label++);
}

void v_label (v_label_t l) {
  assert (l < MAX_LABELS);
  labels[l].addr = v_ip;
}

/* record a reference to the absolute address of a label */
void v_dlabel (void *addr, v_label_t l) {
  assert (l < MAX_LABELS);
  assert (labels[l].num_refs < MAX_REFS);
  labels[l].type[labels[l].num_refs] = ABSOLUTE;
  labels[l].refs[labels[l].num_refs++] = addr;
}

/* record a reference to the relative address of a label */
void v_dmark (v_code *addr, v_label_t l) {
  assert (l < MAX_LABELS);
  assert (labels[l].num_refs < MAX_REFS);
  labels[l].type[labels[l].num_refs] = RELATIVE;
  labels[l].refs[labels[l].num_refs++] = addr;
}

/* Called when we're done building a procedure.

   go through each defined label and fill in it's actual address
   everwhere that it's referenced */

static int labels_end () { 
  int i,j;

  for (i = 0; i < last_label; i++) {
    for (j = 0; j < labels[i].num_refs; j++) {
      if (labels[i].addr == (v_code *)0) {
	return -1;
      }
      if (labels[i].type[j] == ABSOLUTE) {
	*(void **)labels[i].refs[j] = labels[i].addr;
      } else {
	*(void **)labels[i].refs[j] = 
	  (char *)labels[i].addr - (int )labels[i].refs[j] - 4;
      }
    }
  }
  return 0;
}

void label_test () {
  v_label_t l1, l2, l3;
  int a, b = 2;

  labels_start ();

  l1 = v_genlabel ();
  l2 = v_genlabel ();
  l3 = v_genlabel ();

  /* XXX -- hack */
  v_ip = (char *)50;

  v_label (l1);
  v_label (l2);
  v_label (l3);

  a = b;
  v_dmark ((v_code *)&a, l1);
  v_dmark ((v_code *)&b, l1);

  labels_end ();

  printf ("should print 50 twice\n");
  printf ("a = %d b = %d\n", a, b);
}
  
/*
 * ----------------------------------------------------------------------
 * 		    Activation Record Mangement Routines
 * ----------------------------------------------------------------------
 */

/* initialize call state to zero args */
void v_push_init (struct v_cstate *cstate) {
  cstate->next_args = 0;
}

/* routines to build up a set of parameters that should be pushed
   for a call */

void push_arg_reg (struct v_cstate *cstate, v_reg_t r) {
  cstate->type[cstate->next_args] = REGISTER;
  cstate->val[cstate->next_args++] = r;
}

void push_arg_imm (struct v_cstate *cstate, int imm) {
  cstate->type[cstate->next_args] = IMMEDIATE;
  cstate->val[cstate->next_args++] = imm;
}

/* caller-side code for calls with args specified at compile time. Args
   are descirbed by the printf like format string passed in */

v_reg_t scall (v_vptr ptr, char *format, va_list ap) {
  char *fmt, *fmt_base;
  struct v_cstate c;
  int imm;

  /* copy format since we modify it */
  fmt_base = fmt = (char *)malloc (strlen (format)+1);
  assert (fmt);
  strcpy (fmt, format);

  /* first figure out what our arguments are */

  v_push_init (&c);

  while (fmt[0]) {
    imm = 0;

    /* check leading '%' */
    assert (fmt[0] == '%');
    fmt++;

    /* see if we're dealing with a constant or a register */
    if (isupper ((int)fmt[0])) {
      imm = 1;
    }
    fmt[0] = tolower ((int)fmt[0]);

    /* get the type of the argument */
    if (imm) {
      switch (fmt[0]) {
	/* all the word sized values */
      case 'i':
      case 'u': 
      case 'l':
      case 'p':
      case 'v': push_arg_imm (&c, va_arg (ap, int)); break;
        /* ok, shorts */
      case 's': push_arg_imm (&c, va_arg (ap, short)); break;
      /* and finally chars */
      case 'c': push_arg_imm (&c, va_arg (ap, char)); break;
      default: assert (0);
      }
    } else {
      /* argument is a register, so just push it and ignore type */
      push_arg_reg (&c, va_arg (ap, int));
    }
    while (fmt[0] != '%' && fmt[0]) fmt++;
  }

  /* now actually generate the call. ccall will return the temp register
     that holds the result of the function call */

  free (fmt_base);
  return (ccall (&c, ptr));
}

/* the user visible scall variants are all inlined and so defined 
   in vcode.h */

/* caller-side code for calls with args built up at runtime */

v_reg_t ccall (struct v_cstate *cstate, v_vptr ptr) {
  int i;
  v_reg_t ret;

  /* first save all allocated caller saved registers of class V_VAR */
  for (i = 0; i < sizeof (caller_saved_regs)/sizeof (caller_saved_regs[0]); 
       i++) {
    PUSHR (__MKNVIRT(caller_saved_regs[i]));
  }

  /* now push args */
  for (i = cstate->next_args-1; i >= 0; i--) {
    if (cstate->type[i] == REGISTER) {
      if (__NVIRT(cstate->val[i])) {
	PUSHR (cstate->val[i]);
      } else {
	PUSHV (cstate->val[i]);
      }
    } else {
      PUSHI (cstate->val[i]);
    }
  }

  /* allocate a register to hold the return value. We use a virtual
     register since a) user has to move the ret value somewhere else
     immediately so speed doesn't matter and b) things get difficult
     if we were to accidentally allocate a caller-saved register here. */

  ret = v_getvirt ();

  /* and finally the call itself */
  v_jalpi (ptr);

  /* now need to gen code to inc stack back up to get rid
     of arguments we just pushed on */

  v_addui (__ESP, __ESP, cstate->next_args * 4);

  /* need to move returned value into the temp register we just
     allocated */

  v_movu (ret, __EAX);

  /* finally, pop caller saved registers back into place */
  for (i = sizeof (caller_saved_regs)/sizeof (caller_saved_regs[0])-1; i >= 0; 
       i--) {
    POPR (__MKNVIRT(caller_saved_regs[i]));
  }

  /* return the name of the temp register that holds result of function call */
  return (ret);
}
  
/* user-visible ccall variants are defined as inline and so are in vcode.h */

/* generate prologue code for a procedure */

static v_code *proc_prologue (int num_args) {
  int i;
  v_code *backpatch_stack_space;

  /* have to save EBP and load our new one. We backup EBP five words
     to allow for the return EIP and up to four words of arguments
     that can be addressed off of EBP. Additionally, virtual registers
     and stack allocated space are addressed off of EBP. */

  PUSHR (__EBP);
  v_movu (__EBP, __ESP);
  v_addui (__EBP, __EBP, (num_args+2)*4); /* +2 for EIP and EBP */

  /* remember where we need to backpatch--point it to the imm part
     of a sub instruction. The imm 0 will get overwritten with the
     actual amount of space needed. The 2 is 'cause v_ip points
     before the sub ins. that we're about to generate but we want
     the backpatch point to be the immediate after the opcode of
     the sub and the opcode is 2 bytes long. */

  backpatch_stack_space = v_ip + 2;
  v_subui (__ESP, __ESP, 0);
  
  /* push all callee saved registers */
  for (i = 0; i < sizeof (callee_saved_regs)/sizeof (callee_saved_regs[0]);
       i++) {
    PUSHR (__MKNVIRT(callee_saved_regs[i]));
  }

  return (backpatch_stack_space);
}

/* finish generating code for a procedure by generating it's prologue
   code and backpatching the amount of stack space needed */

void proc_epilogue (v_code *backpatch) {
  int i;

  /* backpatch points to a space big enough for an DECSP instruction.
     We've used next_virtual_reg-1 words of space (the -1 is there since
     we've already allocated the 1 word of space for RA (vreg 0). */

  if (next_virtual_reg != 0) {
    *(int *)backpatch = (next_virtual_reg-1)*4;
  }

  /* mark our epilogue with a label so any ret's from the middle
     of the function will bounce here */

  v_label (v_epilouge_label);

  /* now generate code to restore callee saved registers and to return
     to our caller */
  
  /* pop all callee saved registers */
  for (i = sizeof (callee_saved_regs)/sizeof (callee_saved_regs[0])-1; i >= 0;
       i--) {
    POPR (__MKNVIRT(callee_saved_regs[i]));
  }

  if (next_virtual_reg != 0) {
    /* eat up space taken by virtual registers */
    v_addui (__ESP, __ESP, ((next_virtual_reg-1)*4));    
  }

  /* restore EBP */
  POPR (__MKNVIRT (__EBP));

  /* ok, all that remains on the stack at this point is our return address
     and any arguments pushed on by our caller. Caller will pop args
     off, so we're done here */

  RET;
}

static int is_leaf = 0;		/* 1 if current func is a leaf */
static v_code *backpatch;	/* where write amount of local space needed */
static v_code *code;		/* start of generated code */

v_reg_t v_ra;		/* return EIP for this function */

int parse_args (char *fmt, v_reg_t *args) {
  int count = 0;
  int save_count; 

  while (fmt[0]) {
    assert (fmt[0] == '%');
    fmt++;
    assert (fmt[0]);
    fmt++;
    if (fmt[0] && fmt[0] != '%') {
      fmt++;
    }
    count++;
  }

  save_count = count;
  while (count) {
    *(args+count-1) = v_getvirt ();
    count--;
  }

  v_ra = v_getvirt ();		/* this is the return EIP */
  v_getvirt ();			/* this is our saved EBP  */
  return (save_count);
}

void v_lambda (char *name,	/* name of function */
	       char *fmt,	/* describes type of arguments */
	       v_reg_t *args, /* regs for each argument */
	       int leaf,	/* 1 if func is a leaf */
	       v_code *ip,	/* where to write instructions */
	       int nbytes) {	/* size of ip */

  int num_args;

  is_leaf = leaf;
  v_ip = code = ip;

  registers_start ();
  labels_start ();

  /* get a label to mark the end of the procedure */
  v_epilouge_label = v_genlabel ();

  num_args = parse_args (fmt, args);
  backpatch = proc_prologue (num_args);
}

/* end the generation of a procedure and return a function pointer to it */
union v_fp v_end (int *nbytes) {
  union v_fp f;

  /* we need to generate a prologue and backpatch labels in */
  proc_epilogue (backpatch);
  if (labels_end () < 0) {
    f.v = (v_vptr )0;
    return (f);
  }
  registers_end ();

  if (nbytes) {
    *nbytes = (int )v_ip - (int )code;
  }

  f.v = (v_vptr )code;
  return (f);
}
  
/* MISC */

void v_dump () {
  char line[80];
  unsigned current, old_current;
  extern void __disassemble (unsigned *, char *);

  for (current = (unsigned )code; current < (unsigned )v_ip;) {
    old_current = current;
    __disassemble (&current, line);
    printf ("0x%08x:\t%s\n", old_current, line);  
  }
}

void v_fatal () {
  assert (0);
}
