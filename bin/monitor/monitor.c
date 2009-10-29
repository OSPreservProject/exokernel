#include <exos/process.h>
#include <xok/sys_ucall.h>
#include <xok/env.h>

#include <unistd.h>
#include <stdio.h>

#include "config.h"
#include "cpu.h"
#include "debug.h"
#include "emu.h"
#include "handler.h"
#include "init.h"
#include "int.h"
#include "iodev.h"
#include "logue.h"
#include "pagetable.h"
#include "test_trap.h"
#include "types.h"
#include "uidt.h"
#include "vm_layout.h"


void need_support(void)
{
    printf("The monitor and perhaps xok were compiled without \"host\" support.\n");
    printf("Be sure both are compiled with EXO_HOST defined.\n");
    exit(ERR);
}

void launch_guest(void)
{
    int erc;

    trace("going to launch guest\n");

#ifdef TRAP_TEST
    if (config.boot_protected == 0) {
	dprintf("Trap tests are meant to run in protected mode.\nCheck the ini file.\n");
	leaveemu(ERR);
    }
    trap_test();
    exit(0);
#endif

    info("Booting guest in %s mode; eip=0x%08x\n",
	 config.boot_protected ? "PROTECTED" : "V86", REG(eip));
    
    if (config.boot_protected) {
	register_prologue();
    }

    if (config.exitearly) {
	info("Giving you a chance to examine the system before booting...\n");
	debugger();
    }

    erc = sys_run_guest(0, REGS, vmstate.cr[0], (u_int)&exc_C_handler_header);
    dprintf("sys_run_guest failed; erc=%d\n", erc);
    if (erc == -E_INVAL)
	need_support();
}

void setup(void)
{
    trace("doing stdio_init\n");
    stdio_init();
    uidt_initialize();
    register_uidt_handlers();
    /* io_select_init(); */
    trace("doing module_init\n");
    module_init();
    trace("doing cpu_setup\n");
    cpu_setup();
    trace("doing low_mem_init\n");
    low_mem_init();
    trace("doing int init\n");
    int_vector_setup();
    /* time_setting_init(); */
    /* signal_init(); */
    trace("doing memory_init\n");
    memory_init();
    trace("doing device_init\n");
    device_init();
    /* hardware_setup(); */
    /* timer_interrupt_init(); */
}

void teardown(void)
{
    unregister_prologue();
    uidt_initialize();
    unmap_pages(0, vmstate.ppages);
    iodev_term();
    if (dbg_fd)
	fclose(dbg_fd);
}


int main(int argc, char **argv)
{
    /* set up exception stack - see include/vm_layout.h */
    map_pages(XSTACK/NBPG-XSTACK_PAGES, XSTACK_PAGES);
    REGS = (tfp_vm86)(REGS_BASE);

    /* gas doesn't have sizeof, I guess.
       Check include/vm_layout.h if this asserts. */
    ASSERT(HOST_TF_SIZE == sizeof(struct Trapframe_vm86));

    if (FREE_GDT == 1) {
	need_support();
    }

    /*
      if (! root) {
      die;
      }
    */

    config_init(argc, argv);

    setup();
    boot(config.bootdisk);
    yield(-1);  /* This guarantees that screen writes get written,
		   including the memset of "physical" memory to 0,
		   before starting the guest so boot msgs don't get
		   obliterated. */
    launch_guest();

    error("back from launch_guest\n");
    leaveemu(ERR_ASSERT);
    return -1;
}
