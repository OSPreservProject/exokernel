#ifdef TRAP_TEST
#include <xok/sys_ucall.h>

#include "debug.h"
#include "config.h"
#include "types.h"
#include "emu.h"

#define new_cr3   0x8000      /* under 1 meg, so we can write to it as user */
#define new_pde   0x9000
#define new_page  0xa000
#define new_page2 0xb000

void cr3_test(u_int handler)
{
    int tmp;

    debug("Doing cr3 test from 0x%08x to 0x%08x\n", (u_int)&&label1,
	    handler);

    asm("mov $0x11111111, %eax");
    asm("mov %0,         %%ebx"::"i" (new_cr3));
    asm("mov $0x33333333, %ecx");
    asm("mov $0x44444444, %edx");

    asm("pushf");                     /* eflags */
    asm("pushl %0"::"i" (GD_UT));     /* cs  */
    asm("pushl %0"::"i" (&&label1));  /* eip */
    asm("pushl $0");                  /* error code */
    asm("jmp %0"::"r" (handler));
 label1:
    asm("mov %ebx, %cr3");

    /* now fake CR3 should be set.  set a new eip and "trap" again */
    asm("addl $4, %esp");             /* discard old eip */
    asm("pushl %0"::"i" (&&label2));  /* eip */
    asm("pushl $0");                  /* error code */
    asm("jmp %0"::"r" (handler));
 label2:
    asm("mov %cr3, %ecx");

    asm("mov %%ecx, %0"::"m" (tmp));
    ASSERT(tmp == VMSTATE->cr[3]);
    debug("move to/from CR3 seems to work\n\n");
    sleep(1);
}

void lmsw_test(u_int handler)
{
    int tmp;
    u_int cr0 = VMSTATE->cr[0];

    debug("Doing lmsw test from 0x%08x\n", (u_int)&&label1);

    asm("mov $0xa3b4c5d7, %eax");     /* want PE set, otherwise, random */

    asm("pushf");                     /* eflags */
    asm("pushl %0"::"i" (GD_UT));     /* cs  */
    asm("pushl %0"::"i" (&&label1));  /* eip */
    asm("pushl $0");                  /* error code */
    asm("jmp %0"::"r" (handler));
 label1:
    asm("lmsw %eax");

    /* now fake CR0 should be set.  set a new eip and "trap" again */
    asm("addl $4, %esp");             /* discard eip */
    asm("pushl %0"::"i" (&&label2));  /* eip */
    asm("pushl $0");                  /* error code */
    asm("jmp %0"::"r" (handler));
 label2:
    asm("mov %cr0, %ecx");

    asm("mov %%ecx, %0"::"m" (tmp));
    ASSERT((tmp & 0xffff) == (VMSTATE->cr[0] & 0xffff));
    ASSERT((tmp & ~0xffff) == (cr0 & ~0xffff));
    debug("lmsw seems to work\n\n");
    sleep(1);
}

void lidt_test(u_int handler)
{
    unsigned char tmp[6] = {0x08, 0x02, 0xff, 0xff, 0x00, 0x00};

    debug("Doing lidt test from 0x%08x\n", (u_int)&&label1);

    asm("pushf");                        /* eflags */
    asm("pushl %0"::"i" (GD_UT));        /* cs  */
    asm("pushl %0"::"i" (&&label1));     /* trap/return eip */
    asm("pushl $0");                     /* error code */

    /* set up for the lidt call itself.  gas would normally automatically
       generate this from a  asm("lidt %0"::"m" (tmp));  statement, but
       then we don't run this code since it's after the jmp.
    */
    asm(".byte 0x8D, 0x45, 0xF8");       /* lea eax,[ebp-0x8] */
    asm(".byte 0x89, 0x45, 0xF4");       /* mov [ebp-0xc],eax */

    asm("jmp %0"::"r" (handler));
 label1:
    asm(".byte 0x0F, 0x01, 0x5D, 0xF4"); /* lidt [ebp-0xc] */

    debug("lidt seems to work\n\n");
    sleep(1);
}

void lgdt_test(u_int handler)
{
    unsigned char tmp[6] = {0x08, 0x02, 0xff, 0xff, 0x00, 0x00};

    debug("Doing lgdt test from 0x%08x\n", (u_int)&&label1);

    asm("pushf");                        /* eflags */
    asm("pushl %0"::"i" (GD_UT));        /* cs  */
    asm("pushl %0"::"i" (&&label1));     /* trap/return eip */
    asm("pushl $0");                     /* error code */

    /* set up for the lgdt call itself.  gas would normally automatically
       generate this from a  asm("lidt %0"::"m" (tmp));  statement, but
       then we don't run this code since it's after the jmp.
    */
    asm(".byte 0x8D, 0x45, 0xF8");       /* lea eax,[ebp-0x8] */
    asm(".byte 0x89, 0x45, 0xF4");       /* mov [ebp-0xc],eax */

    asm("jmp %0"::"r" (handler));
 label1:
    asm(".byte 0x0F, 0x01, 0x55, 0xF4"); /* lgdt [ebp-0xc] */


    /* asm("lgdt %0"::"m" (tmp)); */
    /* __asm__ __volatile__ ("lgdt %0"::"m" (&tmp[0])); */

    debug("lgdt seems to work\n\n");
    sleep(1);
}

void hlt_test(u_int handler)
{
    debug("Doing hlt test from 0x%08x\n", (u_int)&&label1);

    asm("pushf");                        /* eflags */
    asm("pushl %0"::"i" (GD_UT));        /* cs  */
    asm("pushl %0"::"i" (&&label1));     /* eip */
    asm("pushl $0");                     /* error code */
    asm("jmp %0"::"r" (handler));
 label1:
    asm("hlt");

    debug("hlt seems to work\n\n");
    sleep(1);
}

void clts_test(u_int handler)
{
    u_int cr0;
    VMSTATE->cr[0] |= 8;        /* pretend TS is set so we can clear it */
    cr0 = VMSTATE->cr[0];

    debug("Doing clts test from 0x%08x\n", (u_int)&&label1);

    asm("pushf");                        /* eflags */
    asm("pushl %0"::"i" (GD_UT));        /* cs  */
    asm("pushl %0"::"i" (&&label1));     /* eip */
    asm("pushl $0");                     /* error code */
    asm("jmp %0"::"r" (handler));
 label1:
    asm("clts");

    debug("original cr0:  0x%08x\n", cr0);
    debug("     new cr0:  0x%08x\n", VMSTATE->cr[0]);
    ASSERT((VMSTATE->cr[0] & 8) == 0);
    ASSERT((VMSTATE->cr[0] & ~8) == (cr0 & ~8));
    debug("clts seems to work\n\n");
    sleep(1);
}

void setup_pt(void)
{
    *((Bit32u*)new_pde) = new_page | 7;
}


void add_page_test(u_int handler)
{
    debug("Adding a page-directory entry\n");

    asm("pushf");                     /* eflags */
    asm("pushl %0"::"i" (GD_UT));     /* cs  */
    asm("pushl %0"::"i" (&&label1));  /* eip */
    asm("pushl $3");                  /* error code - write protection error */

    VMSTATE->cr[2] = new_cr3;
    asm("movl %0, %%eax"::"i" (new_pde|7));
    asm("movl %0, %%ebx"::"i" (new_cr3));

    asm("movl 8(%ebp),%edx");         /* get handler arg */
    asm("jmp %edx");
 label1:
    asm("mov %eax, (%ebx)");          /* map pde at first entry */

    ASSERT((*((Bit32u*)new_cr3) & 0xfffff000) == new_pde);
    ASSERT((*((Bit32u*)new_pde) & 0xfffff000) == new_page);

    debug("adding a page mapping seems to work\n\n");
    sleep(1);
}


void change_page_test(u_int handler)
{
    debug("Changing a page-directory entry\n");

    asm("pushf");                     /* eflags */
    asm("pushl %0"::"i" (GD_UT));     /* cs  */
    asm("pushl %0"::"i" (&&label1));  /* eip */
    asm("pushl $3");                  /* error code - write protection error */

    VMSTATE->cr[2] = new_pde;
    asm("movl %0, %%eax"::"i" (new_page2 | 7));
    asm("movl %0, %%ebx"::"i" (new_pde));

    asm("movl 8(%ebp),%edx");         /* get handler arg */
    asm("jmp %edx");
 label1:
    asm("mov %eax, (%ebx)");          /* map pde at first entry */

    ASSERT((*((Bit32u*)new_cr3) & 0xfffff000) == new_pde);
    ASSERT((*((Bit32u*)new_pde) & 0xfffff000) == new_page2);

    debug("changing a page mapping seems to work\n\n");
    sleep(1);
}


void test_cpl(void)
{
    int cpl;

    change_cpl(1);
    cpl = get_cpl();
    ASSERT(cpl == 1);

    change_cpl(3);
    cpl = get_cpl();
    ASSERT(cpl == 3);

    debug("changing CPL seems to work\n\n");
    sleep(1);
}


void trap_test(void)
{
    test_cpl();

#ifdef USER_DEBUG
    /* If the #gpf handler stuff is really broken, you might want to
       start out by manually building the trap frames, and trapping
       back to user space.
    */
    {
	u_int handler13 = (u_int)&exc_13_handler;
	u_int handler14 = (u_int)&exc_14_handler;
	VMSTATE->cr[0] |= 1;
	
	lmsw_test(handler13);
	lidt_test(handler13);
	hlt_test(handler13);
	lgdt_test(handler13);
	clts_test(handler13);
	cr3_test(handler13);          /* set a new page table  */
	setup_pt();
	add_page_test(handler14);     /* add an entry to it    */
	change_page_test(handler14);  /* change an entry in it */
    }
#else
    /* If the #gpf handlers aren't fundamentally broken, test this
       way.
    */
    {
	Bit32u tmp, tmp2;

	change_cpl(1);
	debug("Simulating guest running at CPL %d\n\n", get_cpl());
	sleep(1);

	asm("in $0x80, %al");
	asm("mov $0x80, %dx");
	asm(".byte 0xec");
	debug("trivial inb seems to work\n\n");
	sleep(1);
	
	asm("hlt");
	debug("hlt seems to work\n\n");
	sleep(1);

	/* IF */
	asm("pushf");
	asm("popl %0"::"m" (tmp));
	asm("cli");
	asm("pushf");
	asm("popl %0"::"m" (tmp2));
#if 0    /* only for V86 :(  */
	ASSERT((tmp2 & IF_MASK) == 0);
	ASSERT(((tmp & ~IF_MASK) & ~0xff) == (tmp2 & ~0xff));
#endif
	asm("sti");
	asm("pushf");
	asm("popl %0"::"m" (tmp2));
#if 0    /* only for V86 :(  */
	ASSERT((tmp2 & IF_MASK));
	ASSERT((tmp & ~0xff) == (tmp2 & ~0xff));
#endif
	debug("cli/sti seem to work\n\n");
	sleep(1);

	/* test move to/from segment registers */
	
	/* test move to/from control registers */
	VMSTATE->cr[0] = 0x12456789;
	VMSTATE->cr[2] = 0x1da53799;
	VMSTATE->cr[3] = 0x12416189;
	VMSTATE->cr[4] = 0x1dd5378a;
	asm("mov %cr0, %ebx");
	asm("mov %%ebx, %0"::"m" (tmp));
	ASSERT(tmp == VMSTATE->cr[0]);
	asm("mov %cr2, %ebx");
	asm("mov %%ebx, %0"::"m" (tmp));
	ASSERT(tmp == VMSTATE->cr[2]);
	asm("mov %cr3, %ebx");
	asm("mov %%ebx, %0"::"m" (tmp));
	ASSERT(tmp == VMSTATE->cr[3]);
	asm("mov %cr4, %ebx");
	asm("mov %%ebx, %0"::"m" (tmp));
	ASSERT(tmp == VMSTATE->cr[4]);
	debug("move from CR3 seems to work\n\n");

	change_cpl(3);
    }
#endif
}
#endif
