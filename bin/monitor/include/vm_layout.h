#ifndef VM_LAYOUT_H
#define VM_LAYOUT_H

/* dammit, does gas not know sizeof ? */
#define HOST_TF_SIZE	19*4

#if 0
#define XSTACK		(USTACKTOP-2048)
#else
/* FIXME: I'd prefer to put the REGS and exception stack on the
   regular stack (ie, set XSTACK somewhere below USTACKTOP), but I
   haven't been successful at doing so.  Until I figure out why not, I
   just throw it at some random spot in memory and allocate silly
   amounts of memory for it since the page fault handler won't treat
   this as stack.  Oh well.  Life sucks. */
#define XSTACK		0x45051000         /* totally random address, which is the point for now */ /* 0x45050fb4 */
#define XSTACK_PAGES	50
#endif


#define REGS_TOP	XSTACK
#define REGS_BASE	(REGS_TOP - HOST_TF_SIZE)

#endif
