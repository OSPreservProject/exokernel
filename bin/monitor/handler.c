#include <exos/critical.h>
#include <xok/env.h>
#include <exos/osdecl.h>
#include <xok/sys_ucall.h>


#include "config.h"
#include "cpu.h"
#include "debug.h"
#include "emu.h"
#include "handler.h"
#include "logue.h"
#include "memutil.h"
#include "opa.h"


int exc_14_C_handler(unsigned char *csp, Bit32u cr2);

static int exc_13_C_handler(unsigned char *lina)
{
    trace("in #d,%x\n", vm86s.err);

    ASSERT(vm86s.trapno==13);
    ASSERT((IS_VM86 && !opa.pe) || (!IS_VM86 && opa.pe)); /* we use VM86 flag; guest can't */
    ASSERT(! (!opa.pe && (vmstate.cr[0] & PG_MASK)));     /* paging ==> protected */
    ASSERT(opa.pe || (REG(eip) & 0xffff0000)==0);         /* seeing weirdness with high bits */
    ASSERT(!IS_PE || (REG(ss) & ~7) != 0);

#undef TIME_SYSCALL_COST
#ifdef TIME_SYSCALL_COST
    {
#define SYSCALLS 1
#define ITERATIONS 100
    static int iter = 0;
    static u_int his=0, los=0;
    u_int hie, loe;
    u_int hi, lo;

    if (*(Bit8u*)REG(eip) == 0xf4) {
	int i;
	if (iter==0) {
	    printf("start clock\n");
	    asm volatile("rdtsc");
	    asm volatile("movl %%edx, %0" : "=r" (his));
	    asm volatile("movl %%eax, %0" : "=r" (los));
	}
	for(i=0; i<SYSCALLS; i++) {
	    sys_null();
	}
	iter++;
	if (iter==ITERATIONS) {
	    asm volatile("rdtsc");
	    asm volatile("movl %%edx, %0" : "=r" (hie));
	    asm volatile("movl %%eax, %0" : "=r" (loe));
	    asm volatile("subl %0, %%eax" :: "g" (los));
	    asm volatile("sbbl %0, %%edx" :: "g" (his));
	    asm volatile("movl %%edx, %0" : "=r" (hi));
	    asm volatile("movl %%eax, %0" : "=r" (lo));
	    printf("stop clock\n");
	    printf("0x%08x%08x - 0x%08x%08x = 0x%08x%08x\n", his, los, hie, loe, hi, lo);
	    printf("0x%08x%08x cycles for %d iterations, %d syscalls per iter\n", hi, lo, ITERATIONS, SYSCALLS);
	    printf("%d cycles per iteration\n", lo/ITERATIONS);
	    for(;;);
	}
	return 0;
    }
    }
#endif

    if (opcode_table[*lina] != 0) {
	(opcode_table[*lina])(lina);
    } else {
	unknown(lina);
    }

    return 0;
}


void return_to_guest(void)
{
    if (IS_PE && (REG(cs)&3) == 0) {
	REG(cs) |= 1;
    }
    vmstate.exc = 0;

    if (IS_PE && (REG(cs) & 3) == 3) {
	/* FIXME: Would be nice to verify we have room on guest stack
	   and fall back to doing sys_run_guest if not, but can't call
	   check_push_available() since that could set a guest
	   exception. */
	u_int esp = make_lina(REG(ss), REG(esp));

	/* if we're returning to an app, then we came from an app, and
	   in that case, we don't have to worry about as much stuff as
	   the kernel function does... no CR0 for example...

	   Have to pull tricks by pushing this trapframe (eflags, cs,
	   eip) onto the guest's stack.  We can't iret from the
	   monitor's stack, since this doesn't restore the guest
	   stack.

	   Also, have to save ss:esp in memory since lss can't load
	   from an offset in a register.  Can't do this after popping
	   es, ds since that the data segment might be wrong. */

	set_memory_lina(esp, REG(eip), opa.opb);
	esp -= opa.opb;
	set_memory_lina(esp, REG(cs), opa.opb);
	esp -= opa.opb;
	set_memory_lina(esp, REG(eflags), opa.opb);
	esp -= opa.opb;
	set_memory_lina(esp, REG(ds), opa.opb);
	esp -= opa.opb;
	set_memory_lina(esp, REG(es), opa.opb);
	REG(esp) -= 5*opa.opb;

	asm volatile("movl %0, %%esp" :: "g" (REGS));
	asm volatile("popal");
	asm volatile("mov %0, %%ss" :: "g" (REG(ss)));
	asm volatile("movl %0, %%esp" :: "g" (REG(esp)));
	asm volatile("pop %es");
	asm volatile("pop %ds");
	asm volatile("iret");
    } else {
	int err;
	
	trace("leaving userspace; %x:%x %x:%x\n", REG(cs), REG(eip), REG(ss), REG(esp));
	err = sys_run_guest(0, REGS, vmstate.cr[0], (u_int)&exc_C_handler_header);
	dprintf("sys_run_guest returned with %d\n", err);
    }

    leaveemu(ERR_ASSERT);
}

u_int history[3];
void exc_C_handler_header(u_int trapno, u_int err)
{
    static u_int in_13_handler = 0, in_14_handler = 0;
    unsigned char *lina;

    REGS = (tfp_vm86)(&err + 1);

    if (!config.boot_protected && (REG(cs) == (GD_UT|3) ||
				   REG(cs) == (GD_KT))) {
	dprintf("\nBUG: trapped with #%x,%x at %x:%x\n", trapno, err, REG(cs), REG(eip));
	for(;;);  /* XXX */
	abort();
    }

    get_guest_mode();

    if (opa.pe && opa.cpl == 0) {	/* pretend we're CPL 0, if appropriate */
	REG(cs) &= ~3;
    }

    vm86s.err = err;
    vm86s.trapno = trapno;
    if (opa.pe) {
	Bit16u reg;
	asm("mov %%fs, %0" : "=g" (reg));
	REG(fs) = reg;
	asm("mov %%gs, %0" : "=g" (reg));
	REG(gs) = reg;
    } else {
	ASSERT(trapno == 13);
	REG(es) = REG(es86);
	REG(ds) = REG(ds86);
    }

    lina = (unsigned char *)make_lina(REG(cs), REG(eip));

    debug("#%x,%x ip=%x:%x sp=%x:%x  %02x %02x...\n",
	  trapno, err, REG(cs), REG(eip), REG(ss), REG(esp),
	  lina[0], lina[1]);

    get_prefixes(lina);
    lina += opa.prefixes;

#if 0  /* Add this back later for smoother running.  Too unstable now tho. */
    {
	extern int irq0_pending;
	
	if (irq0_pending && IS_PE && vmstate.g_idt_loaded && !irq_disabled(0)) {
	    int erc;
	    debug("sending guest irq 0...\n");
	    erc = do_pe_int(hardware_irq_number(0));
	    ASSERT(erc == 0);
	    irq0_pending = 0;
	    /* disable_irq(0); */	/* avoid nested IRQ 0!  guest will disable (again) and reenable. */
	    trace("sending IRQ 0 to eip=%x, cs=%x\n", REG(eip), REG(cs));
	}
    }
#endif

    history[0] = history[1];
    history[1] = history[2];
    history[2] = REG(eip);

    if (trapno==13) {
	if (in_13_handler + in_14_handler)
	    goto too_many_faults;
	in_13_handler++;
	exc_13_C_handler(lina);
	in_13_handler--;
    } else if (trapno==14) {
	if (in_14_handler)
	    goto too_many_faults;
	in_14_handler++;
	exc_14_C_handler(lina, REGS->tf_oesp);  /* HACK!  cr2 is in oesp */
	in_14_handler--;
    } else if (opa.cpl==3 && opa.pe) {
	set_guest_exc(vm86s.trapno, vm86s.err);
    } else {
	dprintf("got unexpected exception #%d,%d\n", trapno, err);
	leaveemu(ERR_ASSERT);
    }

#if 0  /* FIXME:  this #GPFs but I don't know why. */
    if (IS_PE) {
	Bit16u reg;
	reg = REG(fs) & ~3;
	asm volatile ("mov %0, %%fs" :: "r" (reg));
	reg = REG(gs) & ~3;
	asm volatile ("mov %0, %%gs" :: "r" (reg));
    }
#endif

    ASSERT((IS_VM86 && !IS_PE) || (!IS_VM86 && IS_PE)); /* we use VM86 flag; guest can't */
    ASSERT(! (!IS_PE && (vmstate.cr[0] & PG_MASK)));    /* paging ==> protected */
    ASSERT(IS_PE || (REG(cs)  & 0xffff0000)==0);
    ASSERT(!IS_PE || (REG(cs) & ~7) != 0);
    ASSERT(IS_PE || (REG(eip) & 0xffff0000)==0);
    ASSERT(IS_PE || ((REG(cs)<<4)+REG(eip))<0x10fff0);
    ASSERT(IS_PE || (REG(ss)  & 0xffff0000)==0);
    ASSERT(!IS_PE || (REG(ss) & ~7) != 0);
    ASSERT(IS_PE || (REG(esp) & 0xffff0000)==0);
    ASSERT(IS_PE || ((REG(ss)<<4)+REG(esp))<0x10fff0);
    ASSERT(!IS_PE || (REG(cs)/8) < FREE_GDT); 
    ASSERT(!IS_PE || (REG(ss)/8) < FREE_GDT); 
    ASSERT(!IS_PE || (REG(ds)/8) < FREE_GDT); 
    ASSERT(!IS_PE || (REG(es)/8) < FREE_GDT); 

    return_to_guest();
    leaveemu(ERR_ASSERT);

 too_many_faults:
    dprintf("\n\nTrap %x,%x at %x:%x with nesting 13:%d 14:%d\n", trapno, err,
	    REG(cs), REG(eip), in_13_handler, in_14_handler);
    for(;;);  /* XXX */
    leaveemu(ERR_ASSERT);
}
