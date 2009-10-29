
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

#ifndef __DPF_INTERNAL_H__
#define __DPF_INTERNAL_H__
/*
 *  DPF internals.   For the moment, we emphasize simplicity by wasting
 *  space. It requires no brains to eliminate this, only attention to detail.
 *
 * SHould have both in-place and regen options.  E.g., after getting
 * to a certain size, keep the code associated with each atom rather
 * than copying.  (Hopefully, this is not too much of a big deal, since
 * will mostly be dealing with the same protocol families.)
 *
 * Make the hash table be uniform.
 * Add deq opcode.
 * Make pptr a ptr to a ptr.
 */
#include <dpf/dpf.h>
#include <dpf/dpf-config.h>
#include <dpf/queue.h>

#if 0
#ifdef KERNEL
#include <dpf/demand.h>
#else
#include <xok_include/assert.h>
#endif
#endif

/*
 * Site-specific macros.  These may be redefined.
 */
#ifndef DPF_MINMSG
#	define DPF_MINMSG     64      /* Minimum message size (in bytes). */
#endif

#ifndef DPF_MSG_ALIGN
#	define DPF_MSG_ALIGN 8        /* Message alignment (4 bytes).  */
#endif


#define DPF_MAXELEM		256

/* If this is larger than 64k, need to increase refcnt and pid in atom. */
#define DPF_MAXFILT		256 /* also in ash_ae_net.h */


/* Note, we make eq's and meq's -> meq's and shifts and mshifts -> shifts. */
enum {
	DPF_OP_EQ = 1,
#	define dpf_iseq(x) ((x)->u.eq.op == DPF_OP_EQ)
	DPF_OP_SHIFT,
#	define dpf_isshift(x) ((x)->u.eq.op == DPF_OP_SHIFT)
	DPF_OP_ACTION,
#	define dpf_isaction(x) ((x)->u.eq.op == DPF_OP_ACTION)
	DPF_OP_STATEEQ,
#	define dpf_isstateeq(x) ((x)->u.eq.op == DPF_OP_STATEEQ)

#	define dpf_islegalop(x) (((unsigned)(op)) < DPF_OP_END)
#	define dpf_isend(x) ((x)->u.eq.op == DPF_OP_END)
	DPF_OP_END			/* mark end */
};

/* 
 * (*DANGER*): we rely on op, offset, nbits and mask being located in 
 * the same place in struct eq and struct shift.
 */

/* size of the load is captured in the opcode. */
struct eq {
	uint8		op;	/* opcode. */	
	uint16		offset;	/* message offset */
	uint8		nbits;	/* length (in bits) */
	uint32		mask;	/* maximum width */
	uint32		val;	/* maximum size value */
};

/* size of the shift is captured in the opcode */
struct shift {
	uint8		op;	/* opcode */
	uint16 		offset;  
	uint8		nbits;	/* length (in bits) */
	uint32		mask;
	uint8		shift;
	uint8		align;	/* alignement given by shift */
};

struct stateeq {
        uint8           op;
        uint16          offset;
        uint8           nbits;
        uint32          mask;
        uint32          stateoffset;
        struct state    *state;
};

struct ir {
        /**
         * The following fields are computed by dpf during insertion (we
         * don't trust clients).
         */

        /*
         * Maximum offset into message of code that we have to take
         * if this node succeeds.  Basically, for each or and shift
         * maxoffset holds the largest message offset of all nodes until
         * the next or, shift or end-of-filter.  This allows us to aggregate
         * message checks rather than having to do them on every atom.
         */
        uint16 maxoffset;

        /* Level of atom: is used as an index for various per-filter tables. */
        uint16 level;

        /* Alignment of atom at this point. */
        uint8   alignment;

        /* Whether a shift has occured upstream in the atomlist. */
        uint8 shiftp;

    /* action stuff */
    struct action_block *action;

        union {
                struct eq eq;
                struct shift shift;
	    struct stateeq stateeq;
        } u;
};

/* container for code. */
struct dpf_ir {
	uint32  version;	/* ir may change from version to version. */
	uint16	irn;
	uint32	moffset;	/* the current offset added by dpf_shifti's. */
	struct ir ir[DPF_MAXELEM+1];		/* pointer to code */
};

/* Used by hash table optimizer */
enum {
	DPF_REGEN = 0,
	DPF_NO_COLL =  (1 << 0),
	DPF_ALL_NTERMS = (1 << 1),
	DPF_ALL_TERMS = (1 << 2),
};

/* Hash table.  Should track terms/non-terms so we can optimize. */
typedef struct ht {
	uint16 htsz;	/* size of the hash table. */
	uint8 log2_sz;  /* log2(htsz) */
	uint8 nbits;	/* number of bits key is on. */
	uint32 ent;	/* number of unique buckets. */
	int coll;	/* number of collisions. */
	fid_t term;	/* number of unique terminals. */
	fid_t nterm;  /* number of unique non-terminals. */
	unsigned state;  /* last optimization state. */
	struct atom *ht[DPF_INITIAL_HTSZ]; 	/* struct hack */
} *Ht;

/* 
 * Could squeeze a lot of space by aggregating straight-line atoms or
 * by using segments for the pointers.  Size of node:
 *	1. with 64 bit ptrs = 5 * 8 + 4 + 2 + 2 + 3 * 4 + 4 = 64 bytes.
 *	2. with 32 bit ptrs = 5 * 4 + 4 + 2 + 2 + 3 * 4 + 4 = 44 bytes.
 * Uch.
 */
typedef struct atom {
	/**
	 * The three pointers that situate us in the filter trie. 
	 */

	/* Parent of this node (if any). */
	struct atom *parent;	

	/* List of kids (if any): can either be or-kids or entries in ht. */
	LIST_HEAD(alist_head, atom) kids;

	/* Siblings (if any). */
	LIST_ENTRY(atom) sibs;	

	/**
	 * Hash table support. 
	 */

	/* Pointer to hash table (non-nil if we are a disjunctive node). */
	struct ht *ht;
	/* Pointer to next bucket. */
	struct atom *next;
	/* Used by hash table to perform indirect jump to child (if any). */
	void *label;

	/**
	 * Code generation information.  Computed by DPF (can't trust app).
	 */

	/* 
	 * Number of filters dependent on this node.  NOTE: this field 
	 * can be removed by using the insight that the last atom
	 * in the filter with a refcnt > 1 must either be (1) a hash table, 
	 * (2) be marked with a pid, or (3) be an or list.  Deletion can
	 * be done by deleting all atoms upto the first atom satisfying
	 * one of these conditions.
	 */
	fid_t	refcnt;		

	/* If you want to do more than 64k filters, redefine these. */
	fid_t	pid;		/* process id of owner. */

	/* The atom at this level. */
	struct ir ir;		
#ifdef VCODE
	unsigned code[DPF_MAXCODE];
#else
	const void **code;	/* label to jump to. */
#endif
} *Atom;

/* Filter insertion/deletion. */
int dpf_insert(struct dpf_ir *filter);
int real_dpf_delete(unsigned pid);

void dpf_init_refs ();
int dpf_fid_getringval (int fid);
struct Env *dpf_fid_lookup (int fid);
int dpf_add_ref (int fid, struct Env *e);
void dpf_del_ref (u_int fid, int envid);
void dpf_del_env (int envid);
int dpf_check_modify_access (struct Env *e, u_int k, u_int filterid);

extern Atom dpf_active[DPF_MAXFILT+1];

struct dpf_frozen_ir;
struct dpf_ir *dpf_xlate(struct dpf_frozen_ir *ir, int nelems);

extern Atom dpf_base;
//int dpf_interp(uint8 *msg, unsigned nbytes, struct frag_return *retmsg);

/* dump trie. */
void dpf_output(void);

//Atom ht_lookup(Ht p, uint32 val);

/* check internal consistency */
void dpf_check(void);

/* Coalesce adjacent atoms. */
void dpf_coalesce(struct dpf_ir *irs);

/* Compute alignement given by ir + alignment. */
int dpf_compute_alignment(struct ir *ir, int alignement);

#ifdef VCODE
	int (*dpf_compile(Atom trie))();
#endif

int (*dpf_compile(Atom trie))();

void dpf_compile_atom(Atom a);

extern unsigned log2(unsigned);

#define hashmult 1737350767	/* magic prime used for hashing. */

int dpf_allocpid(void);
void dpf_freepid(int pid);

unsigned ht_state(Ht ht);
int orlist_p(Atom a);

void dpf_dump(void *ptr);
void dpf_verbose(int v);

#endif /* __DPF_INTERNAL_H__ */
