
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

/* 
 * Code generation machinery required to compile DPF trie.
 *
  TODO:
	1. Don't have to regen unless we went from eq -> ht or we had 
	   a collision.
	2. Need support for incremental linkage (vcode mod).
	3. Simplicity has been emphasized, at the cost of a few extra loads.
		Could eliminate them.
	4. Need to cleanup the vcode perl script so that it will work 
	on systems with fragile perls. 

	5. Rehash after collisions or hash table fillup.

   Some of the decisions we have made assume that work required is
   proportional to number of different protocols and that the number of
   protocols is relatively small (say 20).  If there is a near 1-1
   correspondence of filters and protocols or many protocols period we
   will perform badly on insertion/deletion.  While the fix requires
   work, it is quite feasible (and actually not hard, if one is willing
   to take a ~10-20% hit in demultiplexing speed) -- let me know if 
   you have a style of usage that corresponds to this situation.

  A possible optimization is to extend coalescing to glom together as
  many atoms as possible, ignoring machine word size, and use the xor
  trick to get rid of branches.  Might be more efficient on modern 
  machines.

  Every or point has a value associated with it, giving the maximum
  offset of the straightline code that follows it.  On branches where a
  shift or or occurs, we check whether the value exceeds the message size.
      Just pass a ptr to the last shift/or we have seen
      that is free and clear.

  During code generation we track the number of shifts we see.  At each
  shift point we store the current message pointer before overwriting
  it with the computed one.  If any node fails, we load the previous
  value up.  After code gen is complete, we allocate an array of size
  equal to the longest sequence of shifts.

 */
#include <dpf/dpf-internal.h>
#include <dpf/demand.h>
#include <xok/defs.h>
#include <vcode/vcode.h>
#include <xok/printf.h>


/* We make these globals since passing as params makes more bugs. */

/* Should just make these statically allocated registers (using T0 & co.). */
static v_reg_t hte_r, val_r, label_r; 	/* Used by the hash table */
static v_reg_t src_r;			/* Value loaded from message */
static v_reg_t msg_r;			/* Register holding message ptr */
static v_reg_t nbytes_r;		/* Register holding nbytes in mesg */
static v_reg_t t0;			/* Volatile temp */

/* 
 * Vector that holds stack offsets to store the shifts (so we can restore
 * if their associated filter does not accept.  It is sized to the maximum
 * number of simultaneous shifts that can occur.
 */
static struct { 
	int nbytes;
	int msg;
} shift_stackpos[DPF_MAXELEM];
/* Track the largest shift.  Needed to reuse stack locations.  */
static int shift_highwater;
static int shift_sp;	/* Current index into shift_stackpos. */ 

/* Compile an atom. */
static void gen(Atom a, v_label_t elsel, int alignment, int shiftp);

/* initialize the shift save/restore routines. */
static void shift_init(void) {
	shift_highwater = shift_sp = 0;
}

/* 
 * Save nbytes & msg.  Note: this shift can be elided if failure of the 
 * current node will not lead to further message reads before (1)
 * a shift right above us restores the msg/nbyte state or (2) no more
 * filters exist.  These conditions probably hold in more cases than
 * one would suppose, but I'm not sure they are worth optimizing for.
 */
static void save_shift_state(void) {
	int sp = shift_sp++;
	if(shift_sp > shift_highwater) {
		shift_highwater = shift_sp;
		shift_stackpos[sp].nbytes = v_local(V_U);
		shift_stackpos[sp].msg = v_local(V_U);		
	}
	v_stui(nbytes_r, v_lp, shift_stackpos[sp].nbytes);
	v_stpi(msg_r, v_lp, shift_stackpos[sp].msg);
}

/* Restore msg & nbytes */
static void restore_shift_state(void) {
	int sp = shift_sp--;
	demand(shift_sp >= 0, bogus deallocation!);
	v_raw_load(v_ldui(nbytes_r, v_lp, shift_stackpos[sp].nbytes), 1);
	/* Could eliminate this nop if we really really cared. */
	v_ldpi(msg_r, v_lp, shift_stackpos[sp].msg);
}

/* Compute log base 2 of x.1 */
static int log2(unsigned x) {
	int i;

	demand(x == (x & -x), x is not a power of two!);
	for(i = 0; i < 32; i++)
		if(x & (1 << i))
			return i;
	return 0;
}

/* 
 * if(hte->val != val) 
 *	goto out;
 * else
 *	goto hte->label;
 */
static void gen_lookup(v_label_t out) {
	/* load val and (optimistically) label */
	v_raw_load(v_ldui(val_r, hte_r, offsetof(struct atom, ir.eq.val)), 1);
	v_raw_load(v_ldpi(label_r, hte_r, offsetof(struct atom, label)), 1);

	v_bnep(src_r, val_r, out);	/* hte->val == val */
	v_jp(label_r);		/* goto hte->label */
}

/* 
 * Generate code for the hash table:
	 for(hte = ht[val & (ht->htsz - 1)]; hte != 0; hte = hte->or)
		if(hte->val == val)
			goto hte->label;
 */
static void compile_ht(Ht ht, v_label_t l) {
	v_label_t loop, elsel;

	/* Allocate hash table registers (should allocate statically). */
	if(!v_getreg(&hte_r, V_P, V_TEMP) 
	|| !v_getreg(&val_r, V_UL, V_TEMP)
	|| !v_getreg(&label_r, V_P, V_TEMP))
		fatal(Out of registers);

	/* 
	 * hte = ht[val & (ht->htsz - 1)]; 
	 */
	v_setp(hte_r, (int) &ht->ht[0]);		   /* load base ptr */
	v_andui(val_r, src_r, (ht->htsz-1)); 	  /* compute hash function */
	v_lshui(val_r, val_r, log2(sizeof ht->ht[0]));/* compute index */
	v_ldp(hte_r, val_r, hte_r); 			  /* Load hte */

	/* hte != 0 */
	v_beqpi(hte_r, 0, l);

	/* 
	 * TODO: handle all four cases:
	 * 	1. ht w/ collisions and all terminals.
	 * 	2. ht w/ collisions and one or more non-terminals.
	 * 	3. ht w/o collisions and all terminals.
	 * 	4. ht w/o collisions and one or more non-terminals.
 	 * 
	 * We just do two right now (collisions and no collisions). 
	 * Could, I suppose, unroll the collision loop to the length 
	 * of the longest chain.
	 */
	
	/* elide loop if there are no collisions. */
	if(!ht->coll) {
		gen_lookup(l);
	} else {
		loop = v_genlabel(); elsel = v_genlabel();

		gen_lookup(elsel);
		v_label(loop);
			gen_lookup(elsel);

		v_label(elsel);
			v_ldpi(hte_r, hte_r, offsetof(struct atom, or));
			v_bnepi(hte_r, 0, loop); 	/* hte == 0, goto loop */
	}

	/* Failure: jump to label. Could optimize away for short matches. */
	v_jv(l);   

	/* Deallocate temps. */
	v_putreg(hte_r, V_P);
	v_putreg(val_r, V_U);
	v_putreg(label_r, V_P);
}

/* 
 * If we have shifted on this path or the offset surpasses
 * the minimal amount of buffer space we are allocated,
 * emit a check to see if we have exceeded our memory.
 */
static void emit_bounds_check(int shiftp, uint32 offset, v_label_t l) {
	if(shiftp || offset > DPF_MINMSG)
		v_bleui(nbytes_r, offset, l);
}

/* Generate code for hte entry. */
static void compile_hte(Atom hte, v_label_t elsel, int alignment, int shiftp) {
	v_label_t label;
	int pid;

	/*
	 * Jump table setup.
	 */
	label = v_genlabel(); 		/* Allocate label for indirect goto. */
	v_label(label); 		/* Mark the location of code. */
	v_dlabel(&hte->label, label); 	/* Store address for indirect goto. */

	pid = hte->pid;

	/* If no kids, just return id. */
	if(!hte->child) {
		demand(pid, bogus node: must have a pid!);
		v_retii(pid);
	/* 
	 * A bit of messiness to handle short filters:
	 * we either want to jump to the provided label
	 * or to the address of a short filter. 
	 * 
	 * Note: this should be optimized away when
	 * we only have terminals in the hash table.
	 */
	} else {
		v_label_t l;

		l = !pid  ? elsel : v_genlabel();

		/* gen a label for this atom to jump too. */
		gen(hte->child, l, alignment, shiftp);

		/* If the atoms for this node reject, jump here. */
		if(pid) {
			v_label(l);
			v_retii(pid);
		}
	}
}

/* Compile the lhs of a message ld: (msg[offset:nbits] &  mask) */
static void compile_msgld(struct eq *e, v_label_t l, int alignment, int shiftp) {

	uint32 mask;
	uint16 offset;

	mask = e->mask;
	offset = e->offset;
	emit_bounds_check(shiftp, offset, l);

	switch(e->nbits) {
	case 8:		
		v_alduci(src_r, msg_r, offset, alignment);
		if(mask != 0xff)
			v_andui(src_r, src_r, mask); 
		break;
	case 16:
		v_aldusi(src_r, msg_r, offset, alignment);
		if(mask != 0xffff)
			v_andui(src_r, src_r, mask); 
		break;
	case 24:
		v_aldui(src_r, msg_r, offset, alignment);
		v_andui(src_r, src_r, mask);
		break;
	case 32:
		v_aldui(src_r, msg_r, offset, alignment);
		if(mask != 0xffffffff)
			v_andui(src_r, src_r, mask); 
		break;
	/* 
	 * Should extend to 64 bits at least.  Possibly further and use
	 * branchless code for equality testing.
	 */

	/* Death. */
	default: fatal(bogus number of bits);
	}
}

/* Compile the left hand part of an eq. */
static void 
compile_eq(Atom a, v_label_t l, v_label_t clabel, int alignment, int shiftp) {
	struct eq *e;
	Ht ht;

	e = &a->ir.eq;	

	compile_msgld(e, l, alignment, shiftp);


	if((ht = a->ht)) {
		int i,n;
		Atom hte;

		compile_ht(a->ht, l);

		/* Generate code for all entries in the hash table. */
		for(i = 0, n = ht->htsz; i < n; i++)
			for(hte = ht->ht[i]; hte; hte = hte->or)
				compile_hte(hte, clabel, alignment,shiftp);
	} else {
#		ifndef MIPS
			v_bneui(src_r, e->val, l);
#		else
			/* Fill delay with (possibly unnecessary) imm load. */
			v_nuke_nop();
			v_setu(t0, e->val);
			v_bneu(src_r, t0, l);
#		endif

		if(a->child || a->ht)
			v_nuke_nop();	/* I believe this is safe. */
		gen(a->child, clabel, alignment, shiftp);
	}
}

static void 
compile_shift(Atom a, v_label_t l, v_label_t clabel, int align, int shiftp) {
	uint8 shift;
	v_label_t restore;

	/* 
	 * Simple optimization: if clabel is the exit label, don't
	 * emit the save/restore since either we will succeed (and
	 * return a pid) or we will fail and so it won't matter
	 * if the msg/nbytes is saved.
	 */

	/* Load value into src_r. */
	compile_msgld(&a->ir.eq, l, align, shiftp); 
	v_nuke_nop();
	/*
	 *  (*DANGER*): if save-shift-state could be a nop have to remove
	 * nuke-nop.
	 */
	save_shift_state(); 

	/* shift src by the required amount. */
	if((shift = a->ir.shift.shift))
		v_lshui(src_r, src_r, shift);

	/* 
	 * Subtract off the loaded value from nbytes & add it to the message
 	 * ptr.   This, of course, can be reduced to a single, but it would
	 * make checking message offsets (ala eq and ht's) more expensive.
	 */
	v_subu(nbytes_r, nbytes_r, src_r);
	v_addp(msg_r, msg_r, src_r);

	restore = v_genlabel();

	/* Indicate that we encountered a shift. */
	gen(a->child, restore, align, 1);


	/* Only get here on failure. */
	v_label(restore);
		restore_shift_state();	/* restore nybtes & msg */
		/* now jump to where we were supposed too on failure. */
		v_jv(clabel);		
}

/* Compile atom. */
static void gen(Atom a, v_label_t elsel, int alignment, int shiftp) {

	/* Walk down each branch */
	for(; a; a = a->or) {
		int pid;
		v_label_t next_or, child_label;

		/* The last label is the elsel */
		next_or = !a->or ? elsel : v_genlabel();
		pid = a->pid;

		/* XXX Special case no kids to save on cost of labels. */

		demand(!a->ht || !a->pid, ht should not have a pid);
		/* 
		 * If this node has a pid, jump to pid return on failure
		 * in the child.
		 */
	  	child_label = (!pid || !a->child) ? next_or : v_genlabel();

		/* 
		 * Label rules:
		 * 	1. If an atom's initial comp fails, jump to next
		 * 	label.  This label will either be generated here
	 	 * 	or, on the last node, will be elsel.
		 *	2. If an atom's initial comp succeeds and the
		 * 	the atom has a pid, send a new label to gen to
		 * 	jump to on failure & at this label's location
		 * 	insert a return with the pid (this is for longest
		 * 	match).
		 *	3. If an atom's initial comp succeeds and the
		 *	atom does not have pid, jump to the next label.
		 */
		if(!dpf_isshift(a->ir.eq.op))
			compile_eq(a, next_or, child_label, alignment, shiftp);
		else
			compile_shift(a, next_or, child_label, alignment, shiftp);
		/* Return pid. */
		if(pid) {
			if(a->child)
				v_label(child_label);
			v_retii(pid);
		}

		/* Position label. */
		if(a->or)
			v_label(next_or);
	}
}

/* Compile DPF trie. */
v_iptr dpf_compile(Atom trie) {
	/* Memory to hold code in -- size is a hack. */
        static v_code insn[2048];      
	v_reg_t args[2];
	v_label_t no_match;
	
        v_lambda("dpf-filter", "%p%u", args, V_LEAF, insn, sizeof insn);
		shift_init();

		if(!v_getreg(&src_r, V_UL, V_TEMP))
			fatal(out of registers!);
		/* volatile temporary. */
		if(!v_getreg(&t0, V_UL, V_TEMP))
			fatal(out of registers!);
		
		no_match = v_genlabel();
		msg_r = args[0]; 
		nbytes_r = args[1];

		/* Walk down trie, Generating code for all elements. */
		gen(trie, no_match, DPF_MSG_ALIGN, 0);

		v_label(no_match);	/* jump here if no matches */
			v_retii(0);
	return v_end(0).i;
}
