#include <xok/sysinfo.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#include "bios.h"
#include "cpu.h"
#include "debug.h"
#include "descriptors.h"
#include "disks.h"
#include "emu.h"
#include "handler_utils.h"
#include "int.h"
#include "iodev.h"
#include "memoryemu.h"
#include "memutil.h"
#include "opa.h"
#include "pagetable.h"
#include "pic.h"
#include "tasks.h"
#include "timers.h"
#include "tss.h"
#include "uidt.h"
#include "vc.h"
#include "video.h"


static void *interrupt_function[0x100];
static int int_queue_start = 0;
static int int_queue_end = 0;
static int int_queue_running = 0;
struct int_queue_struct int_queue[IQUEUE_LEN];
struct int_queue_list_struct int_queue_head[NUM_INT_QUEUE];

#ifndef __min
#define __min(x,y)   (((x)<(y))?(x):(y))
#endif


#if CATCH_HARD_INTS
int do_hard_int(int intno)
{
    queue_hard_int(intno, NULL, NULL);
    return (1);
}
#endif

inline int do_soft_int(int intno)
{
    ASSERT(! IS_PE);
    do_int(intno);
    return 1;
}

int interrupt_has_error_code_p(int i)
{
    return (i>=8 && i<=17 && i!=9 && i!=15 && i!=16);
}

/* _hardware_ interrupts */
int irq_disabled(u_int irq)
{
    ASSERT(irq<16);

    if (irq<8)
	/* 0021  RW  PIC master interrupt mask register OCW1 (see #P0014) */
	return (read_pic0(0x21) & (1<<irq));
    else
	/* 00A1  RW  PIC 2  same as 0021 for PIC 1 except for OCW1 (see #P0418) */
	return (read_pic1(0xa1) & (1<<irq-8));
}


void disable_irq(u_int irq)
{
    ASSERT(irq<16);

    if (irq<8)
	write_pic0(0x21, read_pic0(0x21) | (1<<irq));
    else
	write_pic1(0xa1, read_pic1(0xa1) | (1<<(irq-8)));
}

inline int hardware_irq_number(u_int i)
{
    ASSERT(i<16);

    return get_pic0_base() + i;
}


#if 0
void kill_time(long usecs)
{
    struct timeval scr_tv, t_start, t_end;
    long t_dif;
    scr_tv.tv_sec = 0L;
    scr_tv.tv_usec = usecs;

    gettimeofday(&t_start, NULL);
    while ((int) select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv) < (int) 1) {
	gettimeofday(&t_end, NULL);
	t_dif = ((t_end.tv_sec * 1000000 + t_end.tv_usec) -
		 (t_start.tv_sec * 1000000 + t_start.tv_usec));

	if ((t_dif >= usecs) || (errno != EINTR))
	    return;
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = usecs - t_dif;
    }
}
#endif

/*
 * DO_INT is used to deal with interrupts returned to DOSEMU by the
 * kernel.
 */

int can_revector(int i)
{
#if 0  /* nothing is revectored now */
    if (i==0x10 || i==0x13)
	return REVECT;
#endif
    return NO_REVECT;
}

void do_int(u_int i)
{
    void (*caller_function) ();

    ASSERT(i<0x100);

    if ((LWORD(cs)) != BIOSSEG && IS_REDIRECTED(i) && can_revector(i)==REVECT) {
	ASSERT(0);  /* not ready to run_int */
	run_int(i);
	return;
    }
    caller_function = interrupt_function[i];
    ASSERT(caller_function);
    caller_function(i);
}

void run_int(int i)
{
    unsigned char *ssp;
    unsigned long sp;

    ssp = (unsigned char *) (REG(ss) << 4);
    sp = (unsigned long) LWORD(esp);

    pushw(ssp, sp, vflags);
    pushw(ssp, sp, REG(cs));
    pushw(ssp, sp, LWORD(eip));
    LWORD(esp) -= 6;
    REG(cs) = ((Bit16u *) 0)[(i << 1) + 1];
    LWORD(eip) = ((u_short *) 0)[i << 1];

    /* clear TF (trap flag, singlestep), VIF/IF (interrupt flag), and
     * NT (nested task) bits of EFLAGS
     * NOTE: IF-flag only, because we are not sure that we will test it in
     *       some of our own software (...we all are human beeings)
     *       For vm86() 'VIF' is the candidate to reset in order to do CLI !
     */
    REG(eflags) &= ~(VIF | TF | IF | NT);
}

struct gate_descr *get_idt_desc(u_int vect)
{
    struct gate_descr *gidt;
    ASSERT(vmstate.g_idt_loaded);
    if (! (vmstate.cr[0]&PG_MASK)) {
	gidt = (struct gate_descr *)vmstate.g_idt_pbase;
    } else {
	gidt = (struct gate_descr *)vmstate.g_idt_base;
    }
    gidt += vect;
    return gidt;
}

/*
  This is ONLY for explicit int n, int3, into instructions.
*/

int do_pe_int(u_int i)
{
    struct descr cs;
    struct gate_descr *gidt;

/* docs tend to say errorcode is i*8+2+EXT, and EXT is bit 0, but what
   is it??? */
#define EXT 0

    ASSERT(opa.pe);
    ASSERT(vmstate.g_idt_loaded);

    gidt = get_idt_desc(i);

    if (vmstate.g_idt_limit/8 < i ||
	(!DESC_IS_INTGATE(gidt) &&
	 !DESC_IS_TRAPGATE(gidt) &&
	 !DESC_IS_TASKGATE(gidt))) {
	set_guest_exc(EXC_GP, i*8+2+EXT);
	goto exc;
    }
    if (gidt->dpl < opa.cpl) {
	set_guest_exc(EXC_GP, i*8+2);
	goto exc;
    }
    if (!gidt->p) {
	set_guest_exc(EXC_NP, i*8+2+EXT);
	goto exc;
    }

    if (DESC_IS_TASKGATE(gidt)) {
	struct seg_descr tss;

	if (get_tss_descriptor(gidt->segsel, &tss, 0) ||
	    switch_tasks(gidt->segsel, &tss))
	    goto exc;
    } else {
	/* TRAP-OR-INTERRUPT-GATE */
	u_int esp;
	struct gate_descr *gd = (struct gate_descr *)gidt;

	if (get_descriptor(gd->segsel, (struct descr*)&cs))
	    goto exc;
	if (!DESC_IS_CODE(&cs) || cs.dpl > opa.cpl) {
	    set_guest_exc(EXC_GP, gd->segsel);
	    goto exc;
	}
	if (!cs.p) {
	    set_guest_exc(EXC_NP, gd->segsel);
	    goto exc;
	}
	    
	if (cs.type & 4 || cs.dpl==opa.cpl) {		/* conforming or same PL */
	    /* INTRA-PRIVILEGE-LEVEL-INTERRUPT */

	    esp = make_lina(REG(ss), REG(esp));
	    if (check_push_available(esp, 3*opa.opb))	/* no error code */
		goto exc;
	    if (check_eip(gd->segsel, GATE_OFFSET(gd)))
		goto exc;
	    esp -= opa.opb; set_memory_lina(esp, REG(eflags), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(cs), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eip), opa.opb);
	    REG(esp) -= 3*opa.opb;
	    REG(cs) = (gd->segsel & ~3) | opa.cpl;
	    REG(eip) = GATE_OFFSET(gd);
	} else {
	    /* INTER-PRIVILEGE-LEVEL-INTERRUPT */
	    struct seg_descr ss;
	    struct Ts *ts;
	    u_int new_ss=GARBAGE_W, new_esp=GARBAGE_W;

	    ASSERT(vmstate.tr_loaded);
	    ts = (struct Ts*)vmstate.tr_base;

	    /* FIXME assuming a 32 TSS */

	    switch(cs.dpl) {
	    case 0:
		new_ss = ts->ts_ss0;  new_esp = ts->ts_esp0;  break;
	    case 1:
		new_ss = ts->ts_ss1;  new_esp = ts->ts_esp1;  break;
	    case 2:
		new_ss = ts->ts_ss2;  new_esp = ts->ts_esp2;  break;
	    default:
		ASSERT(0);
	    }

	    if (get_descriptor(new_ss, (struct descr*)&ss))
		goto exc;
	    if (new_ss & 3 != ss.dpl ||
		ss.dpl != cs.dpl ||
		! DESC_IS_WRITABLE_DATA(&ss)) {
		set_guest_exc(EXC_TSS, new_ss);
		goto exc;
	    }
	    if (! ss.p) {
		set_guest_exc(EXC_SS, new_ss);
		goto exc;
	    }

	    esp = make_lina(new_ss, new_esp);
	    if (check_push_available(esp, 5*opa.opb))	/* no error code */
		goto exc;
	    if (check_eip(ts->ts_cs, ts->ts_eip))
		goto exc;
	    esp -= opa.opb; set_memory_lina(esp, REG(ss), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(esp), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eflags), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(cs), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eip), opa.opb);
	    REG(ss) = new_ss;
	    REG(esp) = new_esp - 5*opa.opb;
	    REG(cs) = (ts->ts_cs & ~3) | cs.dpl;
	    REG(eip) = ts->ts_eip;
	}
	if (DESC_IS_INTGATE(gd)) {
	    REG(eflags) &= ~IF_MASK;
	}
	REG(eflags) &= ~(TF_MASK | NT_MASK | VM_MASK | RF_MASK);
    }

    return 0;

 exc:
    return -1;
}


static void default_interrupt(u_char i)
{
    trace("default interrupt; int 0x%02x, ax=0x%04x\n", i, LWORD(eax));

    ASSERT(0);  /* haven't thought about this yet... still dosemu-ish */

    if (!IS_REDIRECTED(i) ||
	(LWORD(cs) == BIOSSEG && LWORD(eip) == (i * 16 + 2))) {
	debug("DEFIVEC: int 0x%02x @ 0x%04x:0x%04x\n", i, ISEG(i), IOFF(i));

	/* This is here for old SIGILL's that modify IP */
	if (i == 0x00)
	    LWORD(eip) += 2;
    } else if (IS_IRET(i)) {
	if ((i != 0x2a) && (i != 0x28))
	    debug("just an iret 0x%02x\n", i);
    } else {
	debug("doing runint 0x%02x\n", i);
	ASSERT(0);  /* not ready to run_int */
	run_int(i);
    }
}

static void int08(u_char i)
{
    ASSERT(0);  /* not ready to run_int */
    run_int(0x1c);
    REG(eflags) |= VIF;
    return;
}

/* memory */
static void int15(u_char i)
{
    int num;
    int unimpl = 0;

    if (HI(ax) != 0x4f)
	NOCARRY;
    /* REG(eflags) |= VIF; */

    switch (HI(ax)) {
    case 0x10:			/* TopView/DESQview */
	switch (LO(ax)) {
	case 0x00:		/* giveup timeslice */
	    /* yield(-1); */
	    break;
	}
	CARRY;
	break;
    case 0x41:			/* wait on external event */
	break;
    case 0x4f:			/* Keyboard intercept */
	HI(ax) = 0x86;
	k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));
#if 0
	CARRY;
	if (LO(ax) & 0x80 )
	    if (1 || !(LO(ax)&0x80) ){
		debug("Carrying it out\n");
		CARRY;
	    }
	    else
		NOCARRY;
#endif
	break;
    case 0x5c:  /* INT 15 - Advanced Power Management v1.0+ - INSTALLATION CHECK */
	if (LO(ax)==0) {
	    HI(ax) = 0x86;
	    CARRY;
	} else {
	    unimpl = 1;
	}
	break;
    case 0x80:			/* default BIOS device open event */
	LWORD(eax) &= 0x00FF;
	break;
    case 0x81:
	LWORD(eax) &= 0x00FF;
	break;
    case 0x82:
	LWORD(eax) &= 0x00FF;
	break;
    case 0x83:
	debug("int 15h event wait:\n");
	show_regs(__FILE__, __LINE__);
	CARRY;
	break;			/* no event wait */
    case 0x84:
#ifdef USE_MRP_JOYSTICK
	mrp_read_joystick();
	h_printf("int 15h joystick int: %d %d\n", (int) (LWORD(eax)), (int) (LWORD(ebx)));
	break;
#else
	CARRY;
	break;			/* no joystick */
#endif
    case 0x85:
	num = LWORD(eax) & 0xFF;	/* default bios handler for sysreq key */
	if (num == 0 || num == 1) {
	    LWORD(eax) &= 0x00FF;
	    break;
	}
	LWORD(eax) &= 0xFF00;
	LWORD(eax) |= 1;
	CARRY;
	break;
    case 0x86:
	/* wait...cx:dx=time in usecs */
	debug("doing int15 wait...ah=0x86\n");
	usleep((unsigned long) ((LWORD(ecx) << 16) | LWORD(edx)));
	NOCARRY;
	break;

    case 0x87:
	if (config.xms_size) {
	    error("xms_init15 not implemented\n");
	    /* xms_int15(); */
	} else {
	    LWORD(eax) &= 0xFF;
	    LWORD(eax) |= 0x0300;	/* say A20 gate failed - a lie but enough */
	    CARRY;
	}
	break;

    case 0x88: /* INT 15 - SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
	LWORD(eax) = config.phys_mem_size - 1024;
	NOCARRY;
	break;

    case 0x89:			/* enter protected mode : kind of tricky! */
	LWORD(eax) |= 0xFF00;	/* failed */
	CARRY;
	break;
    case 0x8a: /* Phoenix BIOS v4.0 - GET BIG MEMORY SIZE */
	LWORD(eax) = config.phys_mem_size & 0xfff;
	LWORD(edx) = config.phys_mem_size >> 16;
	set_CF();
	break;
    case 0x90:			/* no device post/wait stuff */
	CARRY;
	break;
    case 0x91:
	CARRY;
	break;
    case 0xbf:			/* DOS/16M,DOS/4GW */
	switch (REG(eax) &= 0x00FF) {
	case 0:
	case 1:
	case 2:		/* installation check */
	default:
	    REG(edx) = 0;
	    CARRY;
	}
	break;
    case 0xc0: /* SYSTEM - GET CONFIGURATION */
	LWORD(es) = ROM_CONFIG_SEG;
	LWORD(ebx) = ROM_CONFIG_OFF;
	HI(ax) = 0;
	break;
    case 0xc1: /* SYSTEM - RETURN EXTENDED-BIOS DATA-AREA SEGMENT ADDRESS (PS) */
	set_CF();
	break;
    case 0xc2:
#if 0
	debug("PS2MOUSE: Call ax=0x%04x\n", LWORD(eax));
	if (!mice->intdrv)
	    if (mice->type != MOUSE_PS2) {
		REG(eax) = 0x0500;	/* No ps2 mouse device handler */
		CARRY;
		break;
	    }
	switch (REG(eax) &= 0x00FF) {
	case 0x0000:
	    mouse.ps2.state = HI(bx);
	    if (mouse.ps2.state == 0)
		mice->intdrv = 0;
	    else
		mice->intdrv = 1;
	    HI(ax) = 0;
	    NOCARRY;
	    break;
	case 0x0001:
	    HI(ax) = 0;
	    LWORD(ebx) = 0xAAAA;	/* we have a ps2 mouse */
	    NOCARRY;
	    break;
	case 0x0003:
	    if (HI(bx) != 0) {
		CARRY;
		HI(ax) = 1;
	    } else {
		NOCARRY;
		HI(ax) = 0;
	    }
	    break;
	case 0x0004:
	    HI(bx) = 0xAA;
	    HI(ax) = 0;
	    NOCARRY;
	    break;
	case 0x0005:		/* Initialize ps2 mouse */
	    HI(ax) = 0;
	    mouse.ps2.pkg = HI(bx);
	    NOCARRY;
	    break;
	case 0x0006:
	    switch (HI(bx)) {
	    case 0x00:
		LO(bx) = (mouse.rbutton ? 1 : 0);
		LO(bx) |= (mouse.lbutton ? 4 : 0);
		LO(bx) |= 0;	/* scaling 1:1 */
		LO(bx) |= 0x20;	/* device enabled */
		LO(bx) |= 0;	/* stream mode */
		LO(cx) = 0;	/* resolution, one count */
		LO(dx) = 0;	/* sample rate */
		HI(ax) = 0;
		NOCARRY;
		break;
	    case 0x01:
		HI(ax) = 0;
		NOCARRY;
		break;
	    case 0x02:
		HI(ax) = 1;
		CARRY;
		break;
	    }
	    break;
#if 0
	case 0x0007:
	    pushw(ssp, sp, 0x000B);
	    pushw(ssp, sp, 0x0001);
	    pushw(ssp, sp, 0x0001);
	    pushw(ssp, sp, 0x0000);
	    REG(cs) = REG(es);
	    REG(eip) = REG(ebx);
	    HI(ax) = 0;
	    NOCARRY;
	    break;
#endif
	default:
	    HI(ax) = 1;
	    debug("PS2MOUSE: Unknown call ax=0x%04x\n", LWORD(eax));
	    CARRY;
	}
	break;
#else
	HI(ax) = 1;
	CARRY;
	debug("PS2MOUSE: unimplemented\n");
	unimpl = 1;
	break;
#endif
    case 0xc3: /* SYSTEM - ENABLE/DISABLE WATCHDOG TIMEOUT (PS50+) */
    case 0xc4: /* SYSTEM - PROGRAMMABLE OPTION SELECT (PS50+) */
	set_CF();
	break;
    case 0xc9:
	if (LO(ax) == 0x10) {
	    HI(ax) = 0;
	    HI(cx) = vm86s.cpu_type;
	    LO(cx) = 0x20;
	    break;
	}
	/* else fall through */
    case 0x24: /* PS/2 A20 gate support */
    case 0xd8: /* EISA - should be set in config? */
    case 0xda: /* AMI PCI BIOS things */
    case 0xdb: /* AMI BIOS - Flash ROM */
	HI(ax) = 0x86;
	break;
    case 0xe8:
	switch (LO(ax)) {
	case 0x01: /* Phoenix BIOS v4.0 - GET MEMORY SIZE FOR >64M CONFIGURATIONS */
	    LWORD(eax) = __min(config.phys_mem_size-1024, 15*1024);
	    if (config.phys_mem_size <= 16*1024)
		LWORD(ebx) = 0;
	    else {
		LWORD(ebx) = (config.phys_mem_size - 16*1024) / 64;
	    }
	    LWORD(ecx) = LWORD(eax);
	    LWORD(edx) = LWORD(ebx);
	    clear_CF();
	    break;
	case 0x20: /* newer BIOSes - GET SYSTEM MEMORY MAP */
	    unimpl = 1;
	    break;
	case 0x81: /* Phoenix BIOS v4.0 - GET MEMORY SIZE FOR >64M CONFIGURATIONS (32 bit) */
	    REG(eax) = __min(config.phys_mem_size-1024, 15*1024);
	    if (config.phys_mem_size <= 16*1024)
		REG(ebx) = 0;
	    else {
		REG(ebx) = (config.phys_mem_size - 16*1024) / 64;
	    }
	    REG(ecx) = REG(eax);
	    REG(edx) = REG(ebx);
	    clear_CF();
	    break;
	default:
	    unimpl = 1;
	    break;
	}
	break;
    default:
	unimpl = 1;
	break;
    }

    if (unimpl) {
	dprintf("int 0x15 ax=0x%04x unimplemented\n", LWORD(eax));
	leaveemu(ERR_UNIMPL);
    }
}


#if 0
void get_secs(u_int *secs, u_int *usecs)
{
    u_long sys_usecs;
    sys_usecs = (SYSINFO_GET(si_system_ticks) * SYSINFO_GET(si_rate));
    *secs = SYSINFO_GET(si_startup_time) + (sys_usecs / 1000000);
    *usecs = sys_usecs % 1000000;
}
#endif

static int offset_ticks = 0;

u_int get_cur_ticks(void)
{
    u_long ticks;

    ticks = (unsigned long)__sysinfo.si_system_ticks * 1820 / (unsigned long) __sysinfo.si_rate;
    ticks += offset_ticks;
    return ticks % (u_int)(18*60*60*24);

#if 0
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    secs = tv.tv_sec - (tz.tz_minuteswest * 60);
#endif
}


static void int1a(u_char i)
{
    debug("int 0x1a, ah=0x%x\n", HI(ax));

    switch (HI(ax)) {

    case 0:			/* read time counter */
    {
	u_int ticks;
	ticks = get_cur_ticks();
	debug("returning current ticks as %u\n", ticks);
	LWORD(ecx) = (ticks >> 16) & 0xffff;
	LWORD(edx) = ticks & 0xffff;
	LO(ax) = 0;   /* FIXME  set if ticks rolled over since last read */
	break;
    }
    case 1:			/* write time counter */
    {
	u_int ticks = 0;
	ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
	offset_ticks = get_cur_ticks() - ticks;
	trace("set time to %u (%04x:%04x)\n", ticks, LWORD(ecx), LWORD(edx));
	break;
    }
    case 2:			/* get time */
    {
	u_int secs;
	u_int hours;
	u_int mins;

	u_int ticks = get_cur_ticks();
	secs = ticks/18;

	hours = secs / (60*60);
	secs %= (60*60);
	mins = secs / 60;
	secs %= 60;

        HI(cx) = hours % 10;
        hours /= 10;
        HI(cx) |= hours << 4;
        LO(cx) = mins % 10;
        mins /= 10;
        LO(cx) |= mins << 4;
        HI(dx) = secs % 10;
        secs /= 10;
        HI(dx) |= secs << 4;
	LO(dx) = 0;  /* FIXME */
	NOCARRY;
	break;
    }
    case 4:			/* get date */
    {
	time_t time_val;
	struct tm *t;

	time(&time_val);
	t = localtime(&time_val);
	t->tm_year += 1900;
	t->tm_mon++;
	debug("get date %02d.%02d.%04d\n", t->tm_mday, t->tm_mon, t->tm_year);
	LWORD(ecx) = t->tm_year % 10;
	t->tm_year /= 10;
	LWORD(ecx) |= (t->tm_year % 10) << 4;
	t->tm_year /= 10;
	LWORD(ecx) |= (t->tm_year % 10) << 8;
	t->tm_year /= 10;
	LWORD(ecx) |= (t->tm_year) << 12;
	LO(dx) = t->tm_mday % 10;
	t->tm_mday /= 10;
	LO(dx) |= t->tm_mday << 4;
	HI(dx) = t->tm_mon % 10;
	t->tm_mon /= 10;
	HI(dx) |= t->tm_mon << 4;
	NOCARRY;
	break;
    }
    case 3:			/* set time */
    case 5:			/* set date */
	debug("WARNING: timer: can't set time/date\n");
	break;

    case 0xb1:			/* Intel PCI BIOS v 2.0c */
	switch (LO(ax)) {
	case 1:
	case 0x81:		/* installation check */
	    NOCARRY;		/* AH==0 if installed */
	    break;
	case 2:
	case 0x82:		/* find PCI device */
	    debug("FIND PCI device %d id=%04x vend=%04x\n", LWORD(esi),
		     LWORD(ecx), LWORD(edx));
	case 3:
	case 0x83:		/* find PCI class code */
	    HI(ax) = 0x86;	/* not found */
	default:
	    CARRY;
	    break;
	}
	break;

    default:
	dprintf("int 0x1a ax=0x%04x unimplemented\n", LWORD(eax));
	leaveemu(ERR_UNIMPL);

	CARRY;
	return;
    }
}


static void int05(u_char i)
{
    debug("BOUNDS exception\n");
    default_interrupt(i);
    return;
}

void int_a_b_c_d_e_f(u_char i)
{
    debug("IRQ->interrupt %x\n", i);
    show_regs(__FILE__, __LINE__);
    default_interrupt(i);
    return;
}

/* IRQ1, keyb data ready */
static void int09(u_char i)
{
    debug("IRQ->interrupt %x\n", i);
    ASSERT(0);  /* not ready to run_int */
    run_int(0x09);
    return;
}

/* CONFIGURATION */
static void int11(u_char i)
{
    LWORD(eax) = configuration;
    return;
}

/* MEMORY */
static void int12(u_char i)
{
    LWORD(eax) = config.mem_size;
    return;
}

/* KEYBOARD */
static void int_16(u_char i)
{
    static char key_queue[1];
    static int keys = 0;

    debug("int 0x16, ah=0x%x\n", HI(ax));

    switch (HI(ax)) {
    case 0:   /* get keystroke */
    {
	for (; keys==0; /* yield(-1) */ ) {
	    keys += sys_cgets (key_queue, 1);
	}
	if (key_queue[0] == '~')
	    leaveemu(0);
	HI(ax) = key_queue[0];  /* FIXME: should be BIOS scan code. */
	LO(ax) = key_queue[0];
	keys--;
	break;
	}
    case 1:   /* check for keystroke */
	keys += sys_cgets (key_queue, 1);
	if (keys==0)
	    REG(eflags) |= ZF;
	else {
	    REG(eflags) &= ~ZF;
	    HI(ax) = key_queue[0];  /* FIXME: should be BIOS scan code */
	    LO(ax) = key_queue[0];
	}
	
	break;
    case 2:  /* INT 16 - KEYBOARD - GET SHIFT FLAGS */
	break;
    case 3:  /* INT 16 - KEYBOARD - SET TYPEMATIC RATE AND DELAY */
    case 4:  /* INT 16 - KEYBOARD - SET KEYCLICK (PCjr only) */
	break;
    default:
	break;
    }
    return;
    
    ASSERT(0);  /* not ready to run_int */
    run_int(0x16);
}

/* BASIC */
static void int18(u_char i)
{
    k_printf("BASIC interrupt being attempted.\n");
}

/* reboot */
static void int19(u_char i)
{
    debug("int 0x19; shutting down.\n");
    leaveemu(ERR_UNIMPL);

    iodev_reset();
}

#ifdef CATCH_HARD_INTS
/*
 * Called to queue a hardware interrupt - will call "callstart"
 * just before the interrupt occurs and "callend" when it finishes
 */
void queue_hard_int(int i, void (*callstart), void (*callend))
{
    cli();

    int_queue[int_queue_end].interrupt = i;
    int_queue[int_queue_end].callstart = callstart;
    int_queue[int_queue_end].callend = callend;
    int_queue_end = (int_queue_end + 1) % IQUEUE_LEN;

    h_printf("int_queue: (%d,%d) ", int_queue_start, int_queue_end);

    i = int_queue_start;
    while (i != int_queue_end) {
	h_printf("%x ", int_queue[i].interrupt);
	i = (i + 1) % IQUEUE_LEN;
    }
    h_printf("\n");

    if (int_queue_start == int_queue_end)
	leaveemu(56);
    sti();
}
#endif

/* Called by vm86() loop to handle queueing of interrupts */
void int_queue_run(void)
{
    static int current_interrupt;
    static unsigned char *ssp;
    static unsigned long sp;
    static u_char vif_counter = 0;	/* Incase someone don't clear things */

    if (int_queue_start == int_queue_end) {
#if 0
	REG(eflags) &= ~(VIP);
#endif
	return;
    }
    debug("Still using int_queue_run()\n");

    if (!*OUTB_ADD) {
	if (++vif_counter > 0x08) {
	    debug("OUTB interrupts renabled after %d attempts\n", vif_counter);
	} else {
	    REG(eflags) |= VIP;
	    debug("OUTB_ADD = %d , returning from int_queue_run()\n", *OUTB_ADD);
	    return;
	}
    }
    if (!(REG(eflags) & VIF)) {
	if (++vif_counter > 0x08) {
	    debug("VIF interrupts renabled after %d attempts\n", vif_counter);
	} else {
	    REG(eflags) |= VIP;
	    debug("interrupts disabled while int_queue_run()\n");
	    return;
	}
    }
    vif_counter = 0;
    current_interrupt = int_queue[int_queue_start].interrupt;

    ssp = (unsigned char *) (REG(ss) << 4);
    sp = (unsigned long) LWORD(esp);

#if 0
#ifndef NEW_KBD_CODE
    if (current_interrupt == 0x09) {
	k_printf("Int9 set\n");
	/* If another program does a keyboard read on port 0x60, we'll know */
	read_next_scancode_from_queue();
    }
#endif
#endif

    /* call user startup function...don't run interrupt if returns -1 */
    if (int_queue[int_queue_start].callstart) {
	if (int_queue[int_queue_start].callstart(current_interrupt) == -1) {
	    debug("Callstart NOWORK\n");
	    return;
	}
	if (int_queue_running + 1 == NUM_INT_QUEUE)
	    leaveemu(55);

	int_queue_head[++int_queue_running].int_queue_ptr = int_queue[int_queue_start];
	int_queue_head[int_queue_running].in_use = 1;

	/* save our regs */
	int_queue_head[int_queue_running].saved_regs = *REGS;

	/* push an illegal instruction onto the stack */
	/*  pushw(ssp, sp, 0xffff); */
	pushw(ssp, sp, 0xe8cd);

	/* this is where we're going to return to */
	int_queue_head[int_queue_running].int_queue_return_addr = (unsigned long) ssp + sp;
	pushw(ssp, sp, vflags);
	/* the code segment of our illegal opcode */
	pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr >> 4);
	/* and the instruction pointer */
	pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr & 0xf);
	LWORD(esp) -= 8;
    } else {
	pushw(ssp, sp, vflags);
	/* the code segment of our iret */
	pushw(ssp, sp, LWORD(cs));
	/* and the instruction pointer */
	pushw(ssp, sp, LWORD(eip));
	LWORD(esp) -= 6;
    }

    if (current_interrupt < 0x10)
	*OUTB_ADD = 0;
    else
	*OUTB_ADD = 1;

    LWORD(cs) = ((u_short *) 0)[(current_interrupt << 1) + 1];
    LWORD(eip) = ((u_short *) 0)[current_interrupt << 1];

    /* clear TF (trap flag, singlestep), IF (interrupt flag), and
     * NT (nested task) bits of EFLAGS
     */
#if 0
    REG(eflags) &= ~(VIF | TF | IF | NT);
#endif
    if (int_queue[int_queue_start].callstart)
	REG(eflags) |= VIP;

    int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;
    debug("int_queue: running int %x if applicable, return_addr=%x\n",
	     current_interrupt,
	     int_queue_head[int_queue_running].int_queue_return_addr);
}


typedef struct {
    Bit32u offset;
    Bit16u selector;
    Bit8u dpl;
    Bit8u p;
} simple_desc;

simple_desc int_table[0x100];

#if 0
void uidt_interupt(int trapno, u_int err)
{
    /* check perms */

    REG(cs) = 
}
#endif

#if 0
void uidt_chandler(int trapno, u_int eip)
{
    /* stack as set by uidt.c:
       tt_eflags
       tt_cs
       tt_eip
       eax
       edx
       ecx
       trapno
       eip
    */

    /* we only get here via an "int n" ... does the guest expect ebx
       to be saved across that?
    */



    dprintf("[uidt trap %d received. err %d]  unimplemented\n", trapno, err);
    exit(0);
}
#endif


int merge_idt(void)
{
/*
   There's really nothing much we do with the IDT, now that I've
   thought this all out.  All ints, exceptions, etc from the guest get
   funnelled back to us anyway.  We don't need to play any tricks
   here.
   
   The only thing which I _might_ want to do here is cache some
   often-used data from the guest IDT, so I don't have to parse it
   again and again.  Might not save much, and it assumes the IDT won't
   change (it won't, but still...)
*/
#if 0
    int i;
    Bit32u base = vmstate.g_idt_base;
    Bit16u limit = vmstate.g_idt_limit;

    trace("in merge_idt 0x%x:0x%x\n", base, limit);

    if (IS_PE) {
	if (verify_lina_range(base, ~PG_W, limit)) {
	    dprintf("merge_idt: can't read guest IDT 0x%08x-0x%08x\n", base, base+limit);
	    leaveemu(ERR_DESCR);
	}

	for (i=0; i<limit/8; i++) {
	    /* ... */
	}
    }

    trace("out merge_idt\n");
#endif
    return 0;
}

/* set up real mode interrupt vector table, pointing to BIOS.
   set up interrupt redirection bitmap
   set up room for protected mode multiplexed guest interrupt descriptors
*/
void int_vector_setup(void)
{
    unsigned int i;

    ASSERT(verify_lina_range(0, PG_W, 256*4) == 0);

    memset(& vm86s.int_byuser[0], 0x00, sizeof(vm86s.int_byuser));

    for (i=0; i<256; i++) {
	interrupt_function[i] = default_interrupt;

	if ((i & 0xf8) == 0x60) {	/* user interrupts 0x60-0x67 */
	    SETIVEC(i, 0, 0);
	} else {
	    SETIVEC(i, BIOSSEG, 16 * i);
	}
    }

    /* SETIVEC(0x16, INT16_SEG, INT16_OFF); */
    /* SETIVEC(0x10, INT10_SEG, INT10_OFF); */
    /* SETIVEC(0x09, INT09_SEG, INT09_OFF); */
    /* SETIVEC(0x08, INT08_SEG, INT08_OFF); */
    SETIVEC(0x1D, INT1D_SEG, INT1D_OFF);
    SETIVEC(0x1E, INT1E_SEG, INT1E_OFF);


    interrupt_function[5] = int05;	/* prntscrn      bounds */
    interrupt_function[8] = int08;
    interrupt_function[9] = int09;
    interrupt_function[0xa] = int_a_b_c_d_e_f;
    interrupt_function[0xb] = int_a_b_c_d_e_f;
    interrupt_function[0xc] = int_a_b_c_d_e_f;
    interrupt_function[0xd] = int_a_b_c_d_e_f;
    interrupt_function[0xe] = int_a_b_c_d_e_f;
    interrupt_function[0xf] = int_a_b_c_d_e_f;
    interrupt_function[0x10] = int10;	/* video                */
    interrupt_function[0x11] = int11;	/* equipment            */
    interrupt_function[0x12] = int12;	/* memory               */
    interrupt_function[0x13] = int13;	/* disk                 */
    /* interrupt_function[0x14] = int14; *//* serial               */
    interrupt_function[0x15] = int15;   /* misc                 */
    interrupt_function[0x16] = int_16;
    /* interrupt_function[0x17] = int17; */
    interrupt_function[0x18] = int18;
    interrupt_function[0x19] = int19;
    interrupt_function[0x1a] = int1a;
}
