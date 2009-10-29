#include <string.h>
#include <xok/sys_ucall.h>
#include <xok/env.h>

#include "opa.h"
#include "types.h"
#include "cpu.h"
#include "debug.h"
#include "emu.h"
#include "handler_utils.h"
#include "handler.h"
#include "int.h"

int old_epilogue = 0;
int old_prologue = 0;

void unregister_prologue(void)
{
    UAREA.u_entepilogue = old_epilogue;
    UAREA.u_entprologue = old_prologue;
}

void register_prologue(void)
{
    extern int monitor_epilogue, monitor_prologue;

    ASSERT(IS_PE);

    old_epilogue = UAREA.u_entepilogue;
    old_prologue = UAREA.u_entprologue;

    UAREA.u_entepilogue = (u_int)&monitor_epilogue;
    UAREA.u_entprologue = (u_int)&monitor_prologue;
}


#ifdef TRACE_LOGUE
void intr_mon(void)
{
    printf("interrupted monitor\n");
}
void intr_guest(u_int cs, u_int eip)
{
    printf("interrupted guest at %x:%x\n", cs&0xffff, eip);
}
void ret_mon(u_int cs, u_int eip)
{
    printf("return to monitor at %x:%x\n", cs&0xffff, eip);
}
void ret_guest(u_int cs, u_int eip)
{
    printf("return to guest at %x:%x\n", cs&0xffff, eip);
}
void in_pro(void)
{
    printf("in prologue\n");
}
void in_epi(void)
{
    printf("in epilogue\n");
}
void print_yield(void)
{
    printf("yield\n");
}
#endif

void schedule_guest(void)
{
    ASSERT(REGS);
    ASSERT(IS_PE);			/* We don't load the custom prologue until we enter protected mode. */
    ASSERT(UAREA.u_cs != (GD_UT|3));	/* Keep the prologue honest: if we are here, we interrupted the guest, NOT the monitor. */
    ASSERT(REG(cs)/8 < FREE_GDT);	/* Again, must have come from guest. */

#ifdef TRACE_LOGUE
    printf("in schedule_guest; guest cs=%x, ss=%x, ds=%x, es=%x, eip=%x\n", REG(cs), REG(ss), REG(ds), REG(es), REG(eip));
    printf("                   uarea cs=%x, ss=%x, ds=%x, es=%x, eip=%x\n", UAREA.u_cs, UAREA.u_ss, UAREA.u_ds, UAREA.u_es, UAREA.u_ppc);
#endif

    get_guest_mode();
    opa.opb = opa.adb = (opa.pe ? 4 : 2);
    opa.seg = REGNO_DS;
    opa.repe = opa.repne = opa.prefixes = 0;

#if 1
    if (IS_PE && vmstate.g_idt_loaded && !irq_disabled(0)) {
	int erc;
	debug("sending guest irq 0...\n");
	erc = do_pe_int(hardware_irq_number(0));
	ASSERT(erc == 0);
	/* disable_irq(0); */		/* avoid nested IRQ 0!  guest will disable (again) and reenable. */
	debug("returning to eip=%x, cs=%x\n", REG(eip), REG(cs));
    }
#endif
    
    ASSERT(!IS_PE || (REG(cs)/8) < FREE_GDT);
    ASSERT(!IS_PE || (REG(ss)/8) < FREE_GDT);
    ASSERT(!IS_PE || (REG(ds)/8) < FREE_GDT);
    ASSERT(!IS_PE || (REG(es)/8) < FREE_GDT);

    return_to_guest();
    leaveemu(ERR_ASSERT);
}
