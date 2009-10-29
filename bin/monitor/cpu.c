#include <xok/cpu.h>
#include <xok/sys_ucall.h>

#include "cpu.h"
#include "config.h"
#include "debug.h"
#include "emu.h"
#include "memoryemu.h"


void cpu_setup(void)
{
    Bit8u mem[6];

    ASSERT(REGS);

    if (config.boot_protected) {
	REG(cs)  = 0x08 | 1;
	REG(ss)  = 0x10;
	REG(ds)  = 0x10;
	REG(es)  = 0x10;
    } else {
	REG(cs)  = 0;
	REG(ss)  = 0;	/* 0x30  This is the standard pc bios stack */
	REG(ds86)= 0;	/* 0x40  standard pc ds */
	REG(es86)= 0;
	REG(ds)  = 0;
	REG(es)  = 0;
    }
    REG(fs) = 0;
    REG(gs) = 0;
    REG(esp) = 0;
    REG(eax) = 0;
    REG(ebx) = 0;
    REG(ecx) = 0;
    REG(edx) = (config.realcpu << 8) + (config.realcpu == CPU_386 ? 8 : 1);
    REG(esi) = 0;
    REG(edi) = 0;
    REG(ebp) = 0;
    REG(eip) = BOOT_ADDR;
    REG(eflags) = 0x202;
    if (! config.boot_protected)
	REG(eflags) |= VM_MASK;
    if (config.debugger) {
	REG(eflags) |= (TF_MASK | RF_MASK);
    }

    asm volatile ("sidt %0":"=m" (mem));
    vmstate.h_idt_base = *(Bit32u *)(&mem[2]);
    vmstate.h_idt_limit = *(Bit16u *)(&mem[0]);
    debug("host idt base 0x%08x limit 0x%08x\n",
	  vmstate.h_idt_base, vmstate.h_idt_limit);

    asm ("sgdt %0":"=m" (mem));
    vmstate.h_gdt_base = *(Bit32u *)(&mem[2]);
    vmstate.h_gdt_limit = *(Bit16u *)(&mem[0]);
    debug("host gdt base 0x%08x limit 0x%08x\n",
	  vmstate.h_gdt_base, vmstate.h_gdt_limit);

    set_init(&vmstate.cr3);
    set_init(&vmstate.g_gdt_base);
    set_init(&vmstate.g_ldt_base);

    vmstate.cr[0] = 0x60000010 | (config.boot_protected ? 1 : 0);
    vmstate.cr[1] = 0;
    vmstate.cr[2] = 0;
    vmstate.cr[3] = 0;
    vmstate.cr[4] = 0;

    vmstate.dr[0] = 0;
    vmstate.dr[1] = 0;
    vmstate.dr[2] = 0;
    vmstate.dr[3] = 0;
    vmstate.dr[4] = 0;
    vmstate.dr[5] = 0;
    vmstate.dr[6] = (config.realcpu > CPU_486) ? 0xffff0ff0 : 0xffff1ff0;
    vmstate.dr[7] = (config.realcpu > CPU_486) ? 0x00000400 : 0x00000000;

    if (config.boot_protected) {
	extern Bit8u default_gdt[24];
	int i;
	u_int va;

	va = (u_int)&default_gdt[0];
	set_add(&vmstate.g_gdt_base, va);
	get_host_pte(va, &vmstate.g_gdt_pbase);
	vmstate.g_gdt_pbase = (vmstate.g_gdt_pbase & ~0xfff) + (va & 0xfff);
	vmstate.g_gdt_limit = 24;
	vmstate.g_gdt_loaded = 1;

	for(i=1; i < 3; i++) {
	    int erc;
	    struct seg_descr *gdt;
	
	    gdt = ((struct seg_descr *)default_gdt)+i;
	    if (erc=sys_set_gdt(0, i, (struct seg_desc*)gdt)) {
		error_mem("sys_set_gdt(%d, %08x, %08x) returned %d\n", i, *(u_int*)&gdt, *((u_int*)&gdt+1), erc);
	    }
	}
    } else {
	vmstate.g_gdt_loaded = 0;
    }
    vmstate.g_idt_loaded = 0;

    vmstate.g_ldt_pbase = 0;
    vmstate.g_ldt_limit = 0;
    vmstate.g_ldt_loaded = 0;

    vmstate.ppages = config.phys_mem_size*1024/NBPG;
    vmstate.eid = sys_geteid();

    vmstate.exc = 0;
    vmstate.exc_vect = GARBAGE;
    vmstate.exc_erc = GARBAGE_W;

    vmstate.prot_transition = 0;
}
