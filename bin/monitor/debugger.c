#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "debug.h"
#include "descriptors.h"
#include "disks.h"
#include "emu.h"
#include "handler_utils.h"
#include "int.h"
#include "memoryemu.h"
#include "memutil.h"
#include "pagetable.h"
#include "port.h"
#include "termio.h"
#include "vc.h"

int no_exceptions = 0;
static u_int dump_offset = BOOT_ADDR;
const char *registers[]={"eax", "ecx", "edx", "ebx", "esp", "ebp",
			 "esi", "edi", "es", "cs", "ss", "ds", "fs", "gs", "eip", ""};

int reg_s2i(const char *reg)
{
    int i;

    for(i=0; *registers[i]; i++) {
	if (!strcmp(reg, registers[i]))
	    return i;
    }
    return -1;
}


void usage(void)
{
#if 0
    printf("...control...\n");
    printf("r       show registers\n");
    printf("g       go; resume guest execution\n");
    printf("reg=val sets register equal to value.\n");
    printf("        reg is one of eax ecx edx ebx esp ebp esi edi es cs ss ds fs gs eip\n");
    printf("int n   immediately execute an int n on the guest's state (real mode only)\n");
    printf("quit    quit debugger; shut down guest\n");
    printf("help    this\n");
    printf("...debugging...\n");
    printf("dump [[segment:]offset] [n]\n");
    printf("        display n bytes of memory starting at [segment:]offset\n");
    printf("        n defaults to 0x80; offset defaults to the end of the previous dump\n");
    printf("search n string\n");
    printf("        search memory for string starting at address n.  Stops when found\n");
    printf("        or when an unmapped page is encountered\n");
    printf("debug on|off\n");
    printf("        enables or disables debugging information\n");
    printf("ro n    mark the page associated with address n as read-only\n");
    printf("rw n    mark the page associated with address n as read-write\n");
    printf("...host...\n");
    printf("logue   print epilogue and prologue pointers\n");
    printf("...virtualization...\n");
    printf("disks   print information on virtualized disks\n");
    printf("port n  display port handling information for port n\n");
    printf("exc     print current and pending exceptions\n");
    printf("cr      print guest's control registers\n");
    printf("cr3     displays guest's CR3 and all duplicate mappings\n");
    printf("memory  displays physical and virtualized memory sizes\n");
    printf("ptmap   display a map of the combined guest/host page table\n");
    printf("pte n   displays guest pte, host pte, and physical page number for address n\n");
    printf("gp2hp n translates guest page n to host page\n");
    printf("dt      displays guest's IDT, GDT, and LDT locations and sizes\n");
    printf("gdt n   display the guest's n-th GDT descriptor\n");
    printf("idt n   display the guest's n-th IDT descriptor\n");
    printf("irq     display virtualized irq information\n");
    printf("\n");
#endif
}

void search_memory(u_int start, const char *str)
{
    int i, j, len=strlen(str), done = 0;
    u_int stop;

    start = stop = PGROUNDDOWN(start);
    if (verify_lina(start, ~PG_W)) {
	printf("stopped searching at 0x%08x\n", start);
	return;
    }

    for (i=start; !done; i+=NBPG) {
	if (verify_lina(i+NBPG, ~PG_W)) {
	    stop = i+NBPG;
	    done = 1;
	} else
	    stop = i+NBPG-len-1;

	for(j=i; j<stop; j++)
	    if (!strcmp((void*)j, str)) {
		printf("found at 0x%08x\n", j);
		done = 1;
		break;
	    }
    }

    printf("stopped searching at 0x%08x\n", stop);
}


void dump_memory(u_int start, u_int len)
{
    u_int i, j, seg, newstart;
    int vm86 = IS_VM86;
#define WIDTH 0x10

    if (verify_lina_range(start, ~PG_W, len)) {
	printf("0x%08x - 0x%08x is not entirely readable\n", start, start+len-1);
	return;
    }

    newstart = start - (start % WIDTH);
    seg = newstart>>4;
    for(i=newstart; i<start+len; i+=WIDTH) {
	if (vm86) {
	    printf("%04x:%04x  ", seg, i - (seg<<4));
	} else {
	    printf(" %08x  ", i);
	}
	for(j=0; j<WIDTH; j++) {
	    printf("%c", j && (j%(WIDTH/4)==0) ? '|' : ' ');
	    if (i+j >= start && i+j < start+len)
		printf("%02x", *(Bit8u*)(i+j));
	    else
		printf("  ");
	}
	printf("  ");
	for(j=0; j<WIDTH; j++) {
	    if (i+j >= start && i+j < start+len) {
		Bit8u c = *(Bit8u*)(i+j);
		if (c>=0x20 && c<=0x80)
		    printf("%c", c);
		else
		    printf(".");
	    } else
		printf(" ");
	}
	printf("\n");
    }

    dump_offset = start+len;
}


void debugger(void)
{
    extern u_int history[3];
    extern int irq0_pending;
    static int in_debugger = 0;
    char buf[80], buf2[80], reg[4];
    u_int i, j, k;

    if (in_debugger)
	return;
    in_debugger ++;
    no_exceptions = 1;

#if 0
    char *textbuf;
    textbuf = (char*)malloc(TEXT_SIZE);
    if (!textbuf) {
	leaveemu(ERR_MEM);
    }
    memcpy(textbuf, SCR_STATE.virt_address, TEXT_SIZE);
#endif

    push_debug_flags();
    DEBUG_OFF();

    for(;;) {
	printf("\ndbg> ");
	if (fgets(buf, 80, stdin) == NULL)
	    leaveemu(0);
	buf[strlen(buf)-1] = 0;  /* kill \n */
	
	if (*buf==0) {
	    continue;
	} else if (!strcmp(buf, "help")) {
	    usage();
	} else if (!strcmp(buf, "r")) {
	    show_regs(0, 0);
	} else if (!strcmp(buf, "logue")) {
	    printf("prologue:  0x%08x\n", UAREA.u_entprologue);
	    printf("epilogue:  0x%08x\n", UAREA.u_entepilogue);
	} else if (!strcmp(buf, "exc")) {
	    printf("current exception:       #0x%x, 0x%x\n", vm86s.trapno, vm86s.err);
	    printf("pending guest exception: ");
	    if (vmstate.exc)
		printf("#0x%x, 0x%x\n", vmstate.exc_vect, vmstate.exc_erc);
	    else
		printf("none\n");
	} else if (!strcmp(buf, "cr")) {
	    show_cregs();
	} else if (!strcmp(buf, "g")) {
	    REG(eflags) &= ~TF_MASK;
	    break;
	} else if (!strcmp(buf, "q") || !strcmp(buf, "quit")) {
	    leaveemu(0);
	} else if (!strcmp(buf, "disks")) {
	    print_disks();
	} else if (!strcmp(buf, "ptmap")) {
	    /* everything is in k to avoid overflows */
	    u_int granularity = 4*1024;
	    u_int width = 64;
	    printf("granularity:  0x%08x\n", granularity*1024);

	    for (i=0; i < (4*1024*1024)/(width*granularity); i++) {
		u_int start = i*width*granularity;
		printf("0x%08x  ", start*1024);
		for (j=0; j<width; j++) {
		    int gp = 0, hp = 0;
		    for (k=0; k < granularity/NBPG; k++) {
			u_int pte, err=0;
			pte = sys_read_pte(k*NBPG + (j*granularity + i*width*granularity)*1024, 0, vmstate.eid, &err);
			if (err == -E_NOT_FOUND) {
			    err = pte = 0;
			}			    
			if (err == 0) {
			    if (pte&1) {
				if (pte & PG_GUEST)
				    gp = 1;
				else
				    hp = 1;
			    }
			}
		    }
		    if (!gp && !hp)
			printf("-");
		    else if (gp && hp)
			printf("+");
		    else if (gp && !hp)
			printf("g");
		    else
			printf("h");
		}
		printf("\n");
	    }
#if 0
	} else if (!strcmp(buf, "memmap")) {
	    memcheck_dump();
#endif
	} else if (sscanf(buf, "port %x", &i) == 1) {
	    print_port(i);
	} else if (sscanf(buf, "int %x", &i) == 1) {
	    pop_debug_flags();
	    push_debug_flags();
	    no_exceptions = 0;
	    do_int(i);
	    no_exceptions = 1;
	    DEBUG_OFF();
	} else if (sscanf(buf, "gdt %x", &i) == 1) {
	    struct descr *sd;
	    if (set_get_any(&vmstate.g_gdt_base, (u_int*)&sd)) {
		printf("no gdt is defined\n");
		continue;
	    }
	    print_dt_entry(i, sd);
	} else if (sscanf(buf, "idt %x", &i) == 1) {
	    print_dt_entry(i, (struct descr *)vmstate.g_idt_base);
	} else if (sscanf(buf, "ro %x", &i) == 1) {
	    protect_range(PGROUNDDOWN(i), NBPG);
	} else if (sscanf(buf, "rw %x", &i) == 1) {
	    unprotect_range(PGROUNDDOWN(i), NBPG);
	} else if (!strcmp(buf, "history")) {
	    printf("most recent trap eip:  %x  %x  %x\n", history[2], history[1], history[0]);
	} else if (!strcmp(buf, "irq")) {
	    struct gate_descr *sg = (struct gate_descr *)vmstate.g_idt_base + hardware_irq_number(0);
	    for (i=0; i<16; i++) {
		printf("irq %2d %s, handled by idt[%2d], function @ 0x%08x\n", i, irq_disabled(i) ? "disabled" : " enabled",
		       hardware_irq_number(i), GATE_OFFSET(sg+i));
	    }	    
	} else if (sscanf(buf, "dump %x:%x %x", &i, &j, &k) == 2) {
	    dump_memory((i<<4)+j, k);
	} else if (sscanf(buf, "dump %x:%x", &i, &j) == 2) {
	    dump_memory((i<<4)+j, 0x80);
	} else if (sscanf(buf, "dump %x %x", &i, &j) == 2) {
	    dump_memory(i, j);
	} else if (sscanf(buf, "dump %x", &i) == 1) {
	    dump_memory(i, 0x80);
	} else if (!strcmp(buf, "dump")) {
	    dump_memory(dump_offset, 0x80);
	} else if (sscanf(buf, "search %x %79s", &i, buf2) == 2) {
	    search_memory(i, buf2);
	} else if (!strcmp(buf, "debug on")) {
	    pop_debug_flags();
	    DEBUG_ON();
	    push_debug_flags();
	} else if (!strcmp(buf, "debug off")) {
	    pop_debug_flags();
	    DEBUG_OFF();
	    push_debug_flags();
	} else if (sscanf(buf, "pte %x", &i) == 1) {
	    Bit32u host_pte = 0;
	    
	    if (! (vmstate.cr[0] & PG_MASK)) {
	    	printf("guest paging not enabled\n");
		printf("guest_phys_to_host_phys(0x%08x) = 0x%08x\n", i, guest_phys_to_host_phys(i));
	    } else {
		Bit32u gpte = guest_pte(i);
		
		printf("guest cr3           0x%08x\n", vmstate.cr[3]);
		printf("guest 0x%08x -> 0x%08x\n", i, gpte);
		printf("guest_phys_to_host_phys(0x%08x) = 0x%08x\n", gpte & ~PGMASK, guest_phys_to_host_phys(gpte & ~PGMASK));
	    }
	    get_host_pte(i, &host_pte);
	    printf("host  0x%08x -> 0x%08x\n", i, host_pte);
	} else if (sscanf(buf, "gp2hp %x", &i) == 1) {
	    printf("&vmstate.gp2hp[0] = %p, 0x%x mappings\n", vmstate.gp2hp, vmstate.ppages);
	    if (i<vmstate.ppages)
		printf("gp2hp[%x] = 0x%08x\n", i, vmstate.gp2hp[i]);
	} else if (!strcmp(buf, "cr3")) {
	    u_int cr3;
	    Set *set = &vmstate.cr3;

	    printf("cr3 register:  0x%08x\n", vmstate.cr[3]);
	    printf("cr3 set     :  ");
	    for(set_iter_init(set); set_iter_get(set, &cr3); set_iter_next(set)) {
		printf("0x%08x ", cr3);
	    }
	    printf("\n");
	} else if (!strcmp(buf, "dt")) {
	    print_dt_mappings();
	    printf("h gdt base:lim  0x%08x:0x%04x\n", vmstate.h_gdt_base, vmstate.h_gdt_limit);
	    printf("h idt base:lim  0x%08x:0x%04x\n", vmstate.h_idt_base, vmstate.h_idt_limit);
	} else if (!strcmp(buf, "memory")) {
	    printf("0x%08x real physical pages (%3d megs)\n", PHYSICAL_PAGES, PHYSICAL_MEGS_RAM);
	    printf("0x%08x fake physical pages (%3d megs)\n", vmstate.ppages, config.phys_mem_size/1024);
#if 0
	    printf("Eavesdropping on Linux:\n");
	    printf("RAM               %dk\n",   *((Bit32u*)0x901e0));
	    printf("pointing device?  0x%x\n",  *((Bit16u*)0x901ff));
	    printf("APM?              0x%x\n",  *((Bit16u*)0x90040));
#endif
	    ASSERT(vmstate.ppages == config.phys_mem_size*1024/NBPG);
	} else if (sscanf(buf, "%2s=%x", reg, &i) == 2 || 
		   sscanf(buf, "%3s=%x", reg, &i) == 2) {
	    int r = reg_s2i(reg);
	    if (r == -1) {
		printf("unknown register\n");
	    } else if (r==14) {
		REG(eip) = i;
	    } else if (r<=REGNO_EDI) {
		set_reg(r, i, 4);  /* normal regs */
	    } else {
		set_reg(r, i, 2);  /* segment regs */
	    }
	} else {
	    printf("huh?\n");
	}
    }

    pop_debug_flags();

#if 0
    if (debug_flags == 0)
	memcpy(SCR_STATE.virt_address, textbuf, TEXT_SIZE);
    free(textbuf);
#endif

    REG(eflags) |= RF;

    in_debugger --;
    no_exceptions = 0;
    irq0_pending = 0;
}
