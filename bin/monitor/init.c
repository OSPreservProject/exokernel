#include <xok/sys_ucall.h>
#include <xok/mmu.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

#include "bios.h"
#include "bitops.h"
#include "cmos.h"
#include "config.h"
#include "debug.h"
#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "memutil.h"
#include "memoryemu.h"
#include "pagetable.h"
#include "port.h"
#include "uidt.h"
#include "vc.h"
#include "video.h"


void module_init(void)
{
    /* version_init(); *//* Check the OS version */
    /* vm86plus_init(); *//* emumodule support */
    /* SIG_init(); *//* silly int generator support */
    memcheck_init(); /* lower 1M memory map support */
}


/*
 * description:
 *  Initialize stdio, open debugging output file if user specified one
 */
void stdio_init(void)
{
    ASSERT(dbg_fd==0);

    if (config.debugout) {
	dbg_fd = fopen(config.debugout, "w");
	if (!dbg_fd) {
	    dprintf("fopen: %s: %s\n", config.debugout, strerror(errno));
	    leaveemu(ERR_CONFIG);
	}
    }
    sync();			/* for safety */
    setbuf(stdout, NULL);
}

/* load <msize> bytes of file <name> into memory at <mstart>
 */
static int load_file(char *name, u_int mstart, u_int msize)
{
    int fd;

    fd = open(name, O_RDONLY);
    if (fd == -1) {
	dprintf("open: %s: %s\n", name, strerror(errno));
	leaveemu(ERR);
    }
    ASSERT(verify_lina_range(mstart, PG_W, msize) == 0);
    read(fd, (void*)mstart, msize);
    close(fd);

    return 0;
}

/*
 *  Map the video bios into main memory
 * 
 */
static inline void map_video_bios(void)
{
    trace("in map_video_bios\n");

    if (config.mapped_bios) {
	if (config.vbios_file) {
	    debug("loading VBIOS %s into mem at 0x%X (0x%X bytes)\n",
		 config.vbios_file, VBIOS_START, VBIOS_SIZE);
	    load_file(config.vbios_file, VBIOS_START, VBIOS_SIZE);
	} else {
	    dprintf("no video bios file defined\n");
	    leaveemu(ERR);
	}

	/* copy graphics characters from system BIOS */
	memmap((void *)GFX_CHARS, GFXCHAR_SIZE, PROT_READ | PROT_WRITE, (void*)GFX_CHARS);

	memcheck_addtype('V', "Video BIOS");
	memcheck_reserve('V', VBIOS_START, VBIOS_SIZE);
    }
}

/*
 * DANG_BEGIN_FUNCTION map_hardware_ram
 *
 * description:
 *  Initialize the hardware direct-mapped pages
 * 
 * DANG_END_FUNCTION
 */
static inline void map_hardware_ram(void)
{
    int i, j;
    unsigned int addr, size;

    trace("in map_hardware_ram\n");

    if (!config.must_spare_hardware_ram)
	return;
    i = 0;
    do {
	if (config.hardware_pages[i++]) {
	    j = i - 1;
	    while (config.hardware_pages[i])
		i++;		/* NOTE: last byte is always ZERO */
	    addr = HARDWARE_RAM_START + (j << 12);
	    size = (i - j) << 12;
	    if (memmap((void *)addr, (size_t) size, PROT_READ | PROT_WRITE, (void*)addr) == -1) {
		error("memap error in map_hardware_ram errno %d\n", errno);
		break;
	    }
	    debug("mapped hardware ram at 0x%05x .. 0x%05x\n", addr, addr + size - 1);
	}
    } while (i < sizeof(config.hardware_pages) - 1);
}


/* 
   Set up all memory areas as would be present on a typical i86 during
   the boot phase.
 */
static inline void bios_mem_setup(void)
{
    int b;

    trace("in bios_mem_setup\n");

    /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
     * configured number of floppy disks
     */
    CONF_NFLOP(configuration, config.fdisks);
    CONF_NSER(configuration, config.num_ser);
    CONF_NLPT(configuration, config.num_lpt);
#if 0  /* XXX mouse */
    if ((mice->type == MOUSE_PS2) || (mice->intdrv))
	configuration |= CONF_MOUSE;
#endif
    if (config.mathco)
	configuration |= CONF_MATHCO;

    debug("bios configuration: ");
    for (b = 15; b >= 0; b--)
	debug("%s%s", (configuration & (1 << b)) ? "1" : "0", (b % 4) ? "" : " ");
    debug("\n");

    bios_configuration = configuration;
    bios_memory_size = config.mem_size;		/* size of memory */
}

static inline void map_custom_bios(void)
{
    trace("in map_custom_bios\n");

    ASSERT(config.bios_file);
    load_file(config.bios_file, (BIOSSEG<<4), BIOSSIZE);

    bios_mem_setup();		/* setup values in BIOS area */
}

/* 
 * DANG_BEGIN_FUNCTION memory_init
 * 
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 */
void memory_init(void)
{
#if 0
    /* make interrupt vectors read-write */
    mprotect((void *) (BIOSSEG << 4), 0x1000, PROT_WRITE | PROT_READ | PROT_EXEC);
    /* make memory area 0xf8000 to 0xfffff read write */
    mprotect((void *) (ROMBIOSSEG << 4), 0x8000, PROT_WRITE | PROT_READ | PROT_EXEC);
#endif

    map_video_bios();		/* map the video bios */
    map_hardware_ram();		/* map the direct hardware ram */
    map_custom_bios();		/* map the DOSEMU bios */

#ifdef IPX
    if (config.ipxsup)
	InitIPXFarCallHelper();	/* TRB - initialize IPX in boot() */
#endif

#ifdef USING_NET
    pkt_init(0x60);		/* Install the new packet driver interface */
#endif

#ifndef NEW_CMOS
    cmos_reset();
#endif

    if (config.vga) {
	debug("INITIALIZING VGA CARD BIOS!\n");
	init_vga_card();
    }

    /* shared_memory_init(); */

#if 0
    /* make interrupts vector read-only */
    mprotect((void *) (BIOSSEG << 4), 0x1000, PROT_READ | PROT_EXEC);
    /* make memory area 0xf8000 to 0xfffff read only */
    mprotect((void *) (ROMBIOSSEG << 4), 0x8000, PROT_READ | PROT_EXEC);
#endif
}


/* 
 * DANG_BEGIN_FUNCTION device_init
 * 
 * description:
 *  Calls all initialization routines for devices (keyboard, video, serial,
 *    disks, etc.)
 *
 * DANG_END_FUNCTION
 */
void device_init(void)
{
    port_init();

    /* keyboard_flags_init(); */

    if (!config.vga)
	config.allowvideoportaccess = 0;

    scr_state_init();
    video_config_init();
    map_disk_params();   /* final disk init, mapping config into bios */

    iodev_init();
    /* mouse_init(); */
    /* printer_init(); */
    /* disk_init(); */
}


int low_mem_init(void)
{
    int i, err;
    u_int pte, pa;

    trace_mem("in low_mem_init\n");
    ASSERT(vmstate.ppages == config.phys_mem_size*1024/NBPG);
    
    if (vmstate.ppages < 1024*1024/NBPG) {
	dprintf("Not enough physical memory defined.\n");
	leaveemu(ERR_CONFIG);
    }

    /* FIXME:  move this to higher memory and search down, when we get the full 4 gig */
    trace_mem("looking for %x bytes for gp2hp\n", vmstate.ppages * 4);
    vmstate.gp2hp = (Bit32u *)find_va(0x80000000, vmstate.ppages * 4);
    if (vmstate.gp2hp == (Bit32u *)-1) {
	dprintf("failed to get fake-physical-to-real-physical mapping\n");
	leaveemu(ERR_MEM);
    }

    /* init first meg */
    /* for (i = 0; i < 1024*1024/NBPG; i++) { */

    trace_mem("initing %x pages\n", vmstate.ppages);
    for (i = 0; i < vmstate.ppages; i++) {
	pa = 0;
	if (i==PGNO(0xb8000))
	    pa = 0xb8000;

	if ((err = sys_self_insert_pte (0, pa | PG_U | PG_W | PG_P | PG_GUEST, i*NBPG))) {
	    dprintf("sys_self_insert_pte(0, %x, %x) failed with %d\n", PG_U | PG_W | PG_P | PG_GUEST, i*NBPG, err);
	    leaveemu(ERR_MEM);
	}
 	memset((void*)(i*NBPG), 0, NBPG);
	err = get_host_pte(i*NBPG, &pte);
	ASSERT(err==0);
	ASSERT(pte & 1);
	vmstate.gp2hp[i] = PGNO(pte);
	trace_mem("%x->%x ", i*NBPG, pte);
    }
    trace_mem("\n");

#if 0
    /* init 64k wrap around */
    for (i = 0; i < 64*1024/NBPG; i++) {
	err = get_host_pte(i*NBPG, &pte);
	ASSERT(err==0);
	pte = (pte & ~PGMASK) | (PG_U | PG_W | PG_P | PG_GUEST);
	if ((err = sys_self_insert_pte (0, pte, 1024*1024+i*NBPG))) {
	    dprintf("sys_self_insert_pte(0, %x, %x) failed with %d\n", pte, 1024*1024+i*NBPG, err);
	    leaveemu(ERR_MEM);
	}
    }
#endif

    return 0;
}


PUBLIC void register_uidt_handlers(void)
{
    int i;
    extern int uidt_entry;

    for (i=0; i < 256; i++) {
	uidt_register_sfunc(i, (u_int)&uidt_entry + i*12);
    }
}
