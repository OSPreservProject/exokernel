
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

#include <xok/scode.h>
#if 0
#include <stdio.h>
#endif
#include <kern/extern.h>
#include <xok_include/assert.h>

/*
 * Labels
 * 
 * scode labels are integers that map to vcode labels. scode labels can
 * be created by the acceptor or the provider. The provider will typically
 * pass, along with its instructions, a range of labels that its code
 * uses. The acceptor will generate a vcode label for each of these. 
 * The provider must have placed any labels it references withing its
 * own code. Additionally, the acceptor may wish to provide additional
 * labels placed anywhere for the provider to use. 
 *
 * vcode itself verifies that a label that is being referenced has 
 * actually been placed. scode has to verify that instructions only
 * refer to labels that have been defined.
 *
 */

#define NLABELS 32
int s_labels[NLABELS];	/* array of labels t*/

void s_setlabel (int sl, v_label_t vl) {
  assert (sl < NLABELS && sl >= 0);
  s_labels[sl] = vl;
}

/*
 * Register management.
 *
 * Acceptor must map scode registers to vcode registers. Provider refernces
 * scode registers that then get mapped to vcode registers in the acceptor.
 *
 */

#define NREGS 32
int s_regs[NREGS];		/* map from scode regs to vcode regs */

void s_setreg (int sreg, v_reg_t vreg) {
  assert (sreg < NREGS && sreg >= 0);
  s_regs[sreg] = vreg;
}

/*
 * Start building a procedure.
 *
 */
void s_lambda (unsigned char *ip, int nbytes) {
  int i;

  for (i = 0; i < NLABELS; i++) {
    s_labels[i] = -1;
  }
  for (i = 0; i < NREGS; i++) {
    s_regs[i] = -1;
  }

  v_lambda ("", "", NULL, 0, ip, nbytes);
}

union v_fp s_end () {
  int t;
  return (v_end (&t));
}

/* verify that an instruction's label and registers references 
   are to labels and registers that exist. */

static int verify_general (struct Sinsn *i) {
  int k;
  unsigned int u;

  for (k = 0; k < 3; k++) {
    switch (i->s_type[k]) {
    case S_LAB: {
      u = i->s_arg[k];
      if (u >= NLABELS) {
	return (E_INV_LAB);
      }
      u = s_labels[u];
      if (u == -1) {
	return (E_INV_LAB);
      }
      break;
    }
    case S_REG: {
      if (i->s_arg[k] < 0 || i->s_arg[k] >= NREGS) {
	return (E_INV_REG);
      }
      if (s_regs[i->s_arg[k]] == -1) {
	return (E_INV_REG);
      }
      break;
    }
    case S_IMM: break;
    case S_NONE: break;
    default: 
      printf ("unknown type for arg %d: %u\n", k, i->s_type[k]); 
      return (-1); /* XXX -- should use a proper errno val */
    }
  }
  return 0;
}

/* translate a single scode instruction into its vcode equivalent */

int s_gen (struct Sinsn *i) {
  unsigned int u;
  int k;
  extern int s_subsched_emit (struct Sinsn *i);
  extern int s_procsched_emit (struct Sinsn *i);
  extern int s_reject_emit (struct Sinsn *i);
  extern int s_guard_branch (struct Sinsn *i);
  extern int s_load_emit (struct Sinsn *i);
  extern int s_store_emit (struct Sinsn *i);

  if ((k = verify_general (i)) < 0) {
    return k;
  }

  if (S_IS_CTRANSFER(i)) {
    s_guard_branch (i);
  }

  /* XXX -- still need to honor the sign, size, and type bits */

  switch (i->s_op) {
  case S_ADD: {
    if (i->s_type[2] == S_REG) {
      v_addu (s_regs[i->s_arg[0]], s_regs[i->s_arg[1]], s_regs[i->s_arg[2]]);
    } else {
      v_addui (s_regs[i->s_arg[0]], s_regs[i->s_arg[1]], i->s_arg[2]);
    }
    return 0;
  }
  case S_LD: return (s_load_emit (i));
  case S_ST: return (s_store_emit (i));
  case S_BLT: {
    if (i->s_type[1] == S_REG) {
      v_bltu (s_regs[i->s_arg[0]], s_regs[i->s_arg[1]], s_labels[i->s_arg[2]]);
    } else {
      v_bltui (s_regs[i->s_arg[0]], i->s_arg[1], i->s_arg[2]);
    }
    return 0;
  }
  case S_SET: v_setu (s_regs[i->s_arg[0]], i->s_arg[1]); return 0;
  case S_JMP: v_jv (s_labels[i->s_arg[0]]); return 0;
  case S_LABEL: v_label (s_labels[i->s_arg[0]]); return 0;
  case S_SUBSCHED: return (s_subsched_emit (i));
  case S_PROCSCHED: return (s_procsched_emit (i));
  case S_REJECT: return (s_reject_emit (i));

  default: printf ("unknown opcode type %d\n", i->s_op); return (-1);
  }

  return 0;			/* to keep compiler happy */
}
  
  
