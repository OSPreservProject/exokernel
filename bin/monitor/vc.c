/* globals */

/*

 * Here's all the calls to the code to try and properly save & restore
 * the video state between VC's and the attempts to control updates to
 * the VC whilst the user is using another. We map between the real
 * screen address and that used by DOSEMU here too.
 *
 * Attempts to use a cards own bios require the addition of the parameter
 * "graphics" to the video statement in "/etc/dosemu.conf". This will make
 * the emulator try to execute the card's initialization routine which is
 * normally located at address c000:0003. This can now be changed as an
 * option.
 *
 *
 * vc.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 */

#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <xok/malloc.h>
#include <xok/pmap.h>
#include <stdlib.h>

#include "config.h"
#include "debug.h"
#include "emu.h"
#include "memoryemu.h"
#include "termio.h"
#include "bios.h"
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "s3.h"
#include "trident.h"
#include "et4000.h"
#include "priv.h"
#include "memutil.h"

struct video_save_struct linux_regs, all_dosemu_regs[MAX_OSS];
struct video_save_struct *dosemu_regs;
/* #define DOSEMU_REGS all_dosemu_regs[current_console] */
/* static struct debug_flags save_debug_flags; */
static int color_text;
#define MAXDOSINTS 032
#define MAXMODES 34


static void restore_current_video();
void put_video_ram();
/* extern int dosemu_sigaction(int sig, struct sigaction *, struct sigaction *); */
/* static void SIGRELEASE_call (void); */
static void SIGACQUIRE_call(void);
#ifdef WANT_DUMP_VIDEO
static void dump_video();
#endif
void get_video_ram(int);


#if 1
#undef SETSIG
#define SETSIG(sa, sig, fun)	sa.sa_handler = fun; \
sa.sa_flags = 0; \
sa.sa_mask = 0; \
sigaction(sig, &sa, NULL);
#endif


/* kind of like mmap, only it maps from virtual addresses to physical addresses */
/* Returns 0 on success */
int memmap(void *va, size_t length, int prot, void *pa)
{
    u_int numcompleted=0, i;
    u_int num_pages = PGNO(PGROUNDUP(length));
    int err;
    Pte *ptes = alloca(num_pages * sizeof(Pte));

    for (i = 0; i < num_pages; i++)
	ptes[i] = ((int)pa + i*NBPG) | prot | PG_GUEST;
    
    err = sys_insert_pte_range(0, ptes, num_pages, (u_int)va, &numcompleted,
			       0 /*  u_int ke FIXME */, vmstate.eid);
    
    if (err || numcompleted!=num_pages)
	return -1;
    else
	return 0;
}



#if 0
/* check whether we are running on the console; initialise
 * config.console, console_fd and SCR_STATE.console_no
 */
void check_console()
{
    config.console = 0;
    console_fd = STDIN_FILENO;
    SCR_STATE.console_no = current_console;
    dosemu_regs = &all_dosemu_regs[current_console];
}
#endif

static inline void forbid_switch(void)
{
    SCR_STATE.vt_allow = 0;
}

void allow_switch(void)
{
    SCR_STATE.vt_allow = 1;
    if (SCR_STATE.vt_requested) {
	debug_vid("VID: clearing old vt request\n");
	/* SIGRELEASE_call (); */
    }
}

static inline void SIGACQUIRE_call(void)
{
    if (config.console_video) {
	get_video_ram(WAIT);
	restore_current_video();
    }
}


static inline void get_permissions(void)
{
    DOS_SYSCALL(set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 1));
}

static inline void giveup_permissions(void)
{
    DOS_SYSCALL(set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 0));
}

static void restore_current_video(void)
{
    if (!config.vga)
	return;

    /* After all that fun up there, get permissions and save/restore states */
    if (video_initialized) {
	debug_vid("Restoring dosemu_regs[%d]\n", current_console);
	get_perm();
#ifdef WANT_DUMP_VIDEO
	dump_video();
#endif
	restore_vga_state(dosemu_regs);
    }
}

int set_current_console(int i)
{
    if (i < 0 || i >= MAX_OSS) {
	return -1;
    }
    SCR_STATE.current = 0;
    save_vga_state(dosemu_regs);
    current_console = i;
    dosemu_regs = &all_dosemu_regs[current_console];
    restore_vga_state(dosemu_regs);
    SCR_STATE.current = 1;

    return 0;
}





/* allows remapping even if memory is mapped in...this is useful, as it
 * remembers where the video mem *was* mapped, unmaps from that, and then
 * remaps it to where the text page number says it should be
 */
void get_video_ram(int waitflag)
{
    char *graph_mem;
    char *sbase;
    size_t ssize;
    char *textbuf = NULL, *vgabuf = NULL;

    debug_vid("get_video_ram STARTED\n");
    if (config.vga) {
	ssize = GRAPH_SIZE;
	sbase = (char *) GRAPH_BASE;
    } else {
	ssize = TEXT_SIZE;
	sbase = PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    }

#if 0
    if (waitflag == WAIT) {
	config.console_video = 0;
	debug_vid("VID: get_video_ram WAITING\n");
	/* XXX - wait until our console is current (mixed signal functions) */
	do {
	    if (!wait_vc_active ())
		break;
	    debug_vid("Keeps waiting...And\n");
	}
	while (errno == EINTR);
    }
#endif

    if (config.vga) {
	debug("config.vga\n");
	if (READ_BYTE(BIOS_VIDEO_MODE) == 3 && READ_BYTE(BIOS_CURRENT_SCREEN_PAGE) < 8) {
	    textbuf = malloc(TEXT_SIZE * 8);
	    if (!textbuf)
		leaveemu(ERR_MEM);
	    memcpy(textbuf, PAGE_ADDR(0), TEXT_SIZE * 8);
	}
	if (SCR_STATE.mapped) {
	    vgabuf = (char *)malloc(GRAPH_SIZE);
	    if (!vgabuf)
		leaveemu(ERR_MEM);
	    memcpy(vgabuf, (caddr_t) GRAPH_BASE, GRAPH_SIZE);
	    if (memmap(graph_mem = (void *)GRAPH_BASE, GRAPH_SIZE, PG_U | PG_W | PG_P,
		       (void *)GRAPH_BASE) != 0) {
		error("memmap failed for 0x%08x\n", GRAPH_BASE);
		leaveemu(ERR_PT);
	    }		
	    memcpy((caddr_t) GRAPH_BASE, vgabuf, GRAPH_SIZE);
	}
    } else {
	textbuf = (char *)malloc(TEXT_SIZE);
	if (!textbuf)
	    leaveemu(ERR_MEM);
	memcpy(textbuf, SCR_STATE.virt_address, TEXT_SIZE);

	if (SCR_STATE.mapped) {
	    if (memmap(graph_mem = SCR_STATE.virt_address, TEXT_SIZE, PG_U | PG_W | PG_P,
		       SCR_STATE.virt_address) != 0) {
		error("memmap failed for 0x%08x\n", (u_int)SCR_STATE.virt_address);
		leaveemu(ERR_PT);
	    }		
	    memcpy(SCR_STATE.virt_address, textbuf, TEXT_SIZE);
	}
    }
    SCR_STATE.mapped = 0;

    if (config.vga) {
	if (READ_BYTE(BIOS_VIDEO_MODE) == 3) {
	    if (dosemu_regs->mem && textbuf)
		memcpy(dosemu_regs->mem, textbuf, dosemu_regs->save_mem_size[0]);
	    /*      else error("ERROR: no dosemu_regs->mem!\n"); */
	}
	debug("mapping GRAPH_BASE\n");
	if (memmap(graph_mem = (void *) GRAPH_BASE, GRAPH_SIZE, PG_U | PG_W | PG_P,
		   (void *) GRAPH_BASE) != 0) {
	    error("memmap failed for 0x%08x\n", GRAPH_BASE);
	    leaveemu(ERR_PT);
	}		
	/* the code below is done by the video save/restore code */
	get_permissions();
    } else {
	/* this is used for page switching */
	if (PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)) != SCR_STATE.virt_address)
	    memcpy(textbuf, PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)), TEXT_SIZE);

	debug("mapping PAGE_ADDR\n");

	if (memmap(graph_mem = PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)),
		   TEXT_SIZE, PG_U | PG_W | PG_P, (void*)phys_text_base) != 0) {
	    error("memmap failed for 0x%08x\n", GRAPH_BASE);
	    leaveemu(ERR_PT);
	}		

#if 0
	/* Map CGA, etc text memory to HGA memory.
	   Useful for debugging systems with HGA or MDA cards.
	 */
	graph_mem = (char *) mmap((caddr_t) 0xb8000,
				  TEXT_SIZE,
				  PROT_READ | PROT_WRITE,
				  MAP_SHARED | MAP_FIXED,
				  mem_fd,
				  phys_text_base);

	if ((long) graph_mem < 0) {
	    error("ERROR: mmap error in get_video_ram (text): %x, errno %d\n",
		  (Bit32u) graph_mem, errno);
	    return;
	} else
#endif
	    debug_vid("CONSOLE VIDEO address: %p %p %p\n", (void *) graph_mem,
		     (void *) phys_text_base, (void *) PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)));

	get_permissions();
	/* copy contents of page onto video RAM */
	memcpy((caddr_t) PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)), textbuf, TEXT_SIZE);
    }

    if (vgabuf)
	free(vgabuf);
    if (textbuf)
	free(textbuf);

    SCR_STATE.pageno = READ_BYTE(BIOS_CURRENT_SCREEN_PAGE);
    SCR_STATE.virt_address = PAGE_ADDR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    SCR_STATE.phys_address = graph_mem;
    SCR_STATE.mapped = 1;
}


void put_video_ram(void)
{
    char *putbuf = (char *) malloc(TEXT_SIZE);
    char *graph_mem;

    if (SCR_STATE.mapped) {
	debug_vid("put_video_ram called\n");

	if (config.vga) {
	    if (memmap(graph_mem = (void*)GRAPH_BASE, GRAPH_SIZE, PG_U | PG_W | PG_P,
		       (void *)GRAPH_BASE) != 0) {
		error("put_video_ram:  memmap failed for 0x%08x\n", GRAPH_BASE);
		leaveemu(ERR_PT);
	    }		

	    if (dosemu_regs->mem && READ_BYTE(BIOS_VIDEO_MODE) == 3 && READ_BYTE(BIOS_CURRENT_SCREEN_PAGE) < 8) {
		memcpy((caddr_t) PAGE_ADDR(0), dosemu_regs->mem, dosemu_regs->save_mem_size[0]);
	    }
	} else {
	    memcpy(putbuf, SCR_STATE.virt_address, TEXT_SIZE);
	    if (memmap(graph_mem = (void*)SCR_STATE.virt_address, TEXT_SIZE,
		       PG_U | PG_W | PG_P, (void *)SCR_STATE.virt_address) != 0) {
		error("put_video_ram:  memmap failed for 0x%08x\n", (u_int)SCR_STATE.virt_address);
		leaveemu(ERR_PT);
	    }		
	    memcpy(SCR_STATE.virt_address, putbuf, TEXT_SIZE);
	}

	giveup_permissions();
	SCR_STATE.mapped = 0;
    } else
	warn("VID: put_video-ram but not mapped!\n");


    if (putbuf)
	free(putbuf);
    debug_vid("put_video_ram completed\n");
}
#if 0
/* this puts the VC under process control */
void set_process_control(void)
{
    struct vt_mode vt_mode;
    struct sigaction sa;

    vt_mode.mode = VT_PROCESS;
    vt_mode.waitv = 0;
    vt_mode.relsig = SIG_RELEASE;
    vt_mode.acqsig = SIG_ACQUIRE;
    vt_mode.frsig = 0;

    SCR_STATE.vt_requested = 0;	/* a switch has not been attempted yet */
    allow_switch();

/*      NEWSETQSIG (SIG_RELEASE, release_vt); */
/*      NEWSETQSIG (SIG_ACQUIRE, acquire_vt); */

    if (do_ioctl(console_fd, VT_SETMODE, (int) &vt_mode))
	debug_vid("initial VT_SETMODE failed!\n");
    debug_vid("VID: Set process control\n");
}

void clear_process_control(void)
{
    struct vt_mode vt_mode;

    vt_mode.mode = VT_AUTO;
    enter_priv_on();
    ioctl(console_fd, VT_SETMODE, (int) &vt_mode);
    leave_priv_setting();
    signal(SIG_RELEASE, SIG_IGN);
    signal(SIG_ACQUIRE, SIG_IGN);
}

#endif


int vc_active(void)
{				/* return 1 if our VC is active */
#ifdef __linux__
    struct vt_stat vtstat;

    debug("VC_ACTIVE!\n");
    ioctl(console_fd, VT_GETSTATE, &vtstat);
    debug("VC_ACTIVE: ours: %d, active: %d\n", SCR_STATE.console_no, vtstat.v_active);
    return ((vtstat.v_active == SCR_STATE.console_no));
#elseif defined(__NetBSD___)
    int active;

    debug("VC_ACTIVE!\n");
    ioctl(console_fd, VT_GETACTIVE, &active);
    debug("VC_ACTIVE: ours: %d, active: %d\n", SCR_STATE.console_no, active);
    return ((active == SCR_STATE.console_no));
#else
    return 1;
#endif
}

void set_vc_screen_page(int page)
{
    WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, page);

    /* okay, if we have the current console, and video ram is mapped.
     * this has to be "atomic," or there is a "race condition": the
     * user may change consoles between our check and our remapping
     */
    forbid_switch();
    if (SCR_STATE.mapped)	/* should I check for foreground VC instead? */
	get_video_ram(WAIT);
    allow_switch();
}


/* get ioperms to allow havoc to occur */
int get_perm(void)
{
    permissions++;
    if (permissions > 1) {
	return 0;
    }
    if (config.vga) {		/* hope this will not lead to problems with ega/cga */
	/* get I/O permissions for VGA registers */
	if (set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 1)) {
	    debug_vid("VGA: can't get I/O permissions \n");
	    leaveemu(ERR);
	}
	if (((config.chipset == S3) || (config.chipset == CIRRUS)) &&
	    (set_ioperm(0x102, 1, 1) || set_ioperm(0x2ea, 4, 1))) {
	    debug_vid("S3/CIRRUS: can't get I/O permissions \n");
	    leaveemu(ERR);
	}
	if (config.chipset == ATI && (set_ioperm(0x102, 1, 1) || set_ioperm(0x1ce, 2, 1) || set_ioperm(0x2ec, 4, 1))) {
	    debug_vid("ATI: can't get I/O permissions \n");
	    leaveemu(ERR);
	}
	/* color or monochrome text emulation? */
	color_text = port_in(MIS_R) & 0x01;

	/* chose registers for color/monochrome emulation */
	if (color_text) {
	    CRT_I = CRT_IC;
	    CRT_D = CRT_DC;
	    IS1_R = IS1_RC;
	    FCR_W = FCR_WC;
	} else {
	    CRT_I = CRT_IM;
	    CRT_D = CRT_DM;
	    IS1_R = IS1_RM;
	    FCR_W = FCR_WM;
	}
    } else if (config.usesX || (config.console_video && (config.cardtype == CARD_MDA))) {
	if (set_ioperm(0x3b4, 1, 1) ||
	    set_ioperm(0x3b5, 1, 1) ||
	    set_ioperm(0x3b8, 1, 1) ||
	    set_ioperm(0x3ba, 1, 1) ||
	    set_ioperm(0x3bf, 1, 1)) {
	    debug_vid("HGC: can't get I/O permissions \n");
	    leaveemu(ERR);
	}
    }
    debug_vid("Permission allowed\n");
    return 0;
}

/* Stop io to card */
/* FIXME:  why not merge this into getperm? */
int release_perm(void)
{
    if (permissions > 0) {
	permissions--;
	if (permissions > 0) {
	    return 0;
	}
	if (config.vga) {	/* hope this will not lead to problems with ega/cga */
	    /* get I/O permissions for VGA registers */
	    /* release I/O permissions for VGA registers */
	    if (set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 0)) {
		debug_vid("VGA: can't release I/O permissions \n");
		leaveemu(-1);
	    }
	    if (((config.chipset == S3) || (config.chipset == CIRRUS)) &&
		(set_ioperm(0x102, 1, 0) || set_ioperm(0x2ea, 4, 0))) {
		debug_vid("S3/CIRRUS: can't release I/O permissions\n");
		leaveemu(-1);
	    }
	    if (config.chipset == ATI &&
		(set_ioperm(0x102, 1, 0) || set_ioperm(0x1ce, 2, 0) || set_ioperm(0x2ec, 4, 0))) {
		debug_vid("ATI: can't release I/O permissions \n");
		leaveemu(-1);
	    }
	} else if (config.usesX || (config.console_video && (config.cardtype == CARD_MDA))) {
	    if (set_ioperm(0x3b4, 1, 0) ||
		set_ioperm(0x3b5, 1, 0) ||
		set_ioperm(0x3b8, 1, 0) ||
		set_ioperm(0x3ba, 1, 0) ||
		set_ioperm(0x3bf, 1, 0)) {
		debug_vid("HGC: can't release I/O permissions \n");
		leaveemu(ERR);
	    }
	}
	debug_vid("Permission disallowed\n");
    } else
	debug_vid("Permissions already at 0\n");
    return 0;

}

int set_regs(u_char regs[])
{
    int i;

    /* port_out(dosemu_regs->regs[FCR], FCR_W); */
    /* update misc output register */
    port_out(regs[MIS], MIS_W);

    /* write sequencer registers */
    /* synchronous reset on */
    port_out(0x00, SEQ_I);
    port_out(0x01, SEQ_D);
    port_out(0x01, SEQ_I);
    port_out(regs[SEQ + 1] | 0x20, SEQ_D);
    /* synchronous reset off */
    port_out(0x00, SEQ_I);
    port_out(0x03, SEQ_D);
    for (i = 2; i < SEQ_C; i++) {
	port_out(i, SEQ_I);
	port_out(regs[SEQ + i], SEQ_D);
    }

    /* deprotect CRT registers 0-7 */
    port_out(0x11, CRT_I);
    port_out(port_in(CRT_D) & 0x7F, CRT_D);

    /* write CRT registers */
    for (i = 0; i < CRT_C; i++) {
	port_out(i, CRT_I);
	port_out(regs[CRT + i], CRT_D);
    }

    /* write graphics controller registers */
    for (i = 0; i < GRA_C; i++) {
	port_out(i, GRA_I);
	port_out(regs[GRA + i], GRA_D);
    }

    /* write attribute controller registers */
    for (i = 0; i < ATT_C; i++) {
	port_in(IS1_R);		/* reset flip-flop */
	port_out(i, ATT_IW);
	port_out(regs[ATT + i], ATT_IW);
    }

    /* port_out(dosemu_regs->regs[CRTI], CRT_I);
       port_out(dosemu_regs->regs[GRAI], GRA_I);
       port_out(dosemu_regs->regs[SEQI], SEQ_I); */
    /* debug_vid("CRTI=0x%02x\n",dosemu_regs->regs[CRTI]);
       debug_vid("GRAI=0x%02x\n",dosemu_regs->regs[GRAI]);
       debug_vid("SEQI=0x%02x\n",dosemu_regs->regs[SEQI]); */
    return (0);
}


/* Reset Attribute registers */
static inline void reset_att(void)
{
    port_in(IS1_R);
}


/* Attempt to virtualize calls to video ports */

u_char att_d_index = 0;
static int isr_read = 0;

u_char
video_port_in(int port)
{
    debug_vid("Video read on port 0x%04x.\n",port);
    switch (port) {
    case CRT_IC:
    case CRT_IM:
	debug_vid("Read Index CRTC 0x%02x\n", dosemu_regs->regs[CRTI]);
	return (dosemu_regs->regs[CRTI]);
    case CRT_DC:
    case CRT_DM:
	if (dosemu_regs->regs[CRTI] <= 0x18) {
	    debug_vid("Read Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs->regs[CRTI], dosemu_regs->regs[CRT + dosemu_regs->regs[CRTI]]);
	    return (dosemu_regs->regs[CRT + dosemu_regs->regs[CRTI]]);
	} else
	    return (ext_video_port_in(port));
	break;
    case GRA_I:
	debug_vid("Read Index GRAI 0x%02x\n", GRAI);
	return (dosemu_regs->regs[GRAI]);
    case GRA_D:
	if (dosemu_regs->regs[GRAI] < 0x0a) {
	    debug_vid("Read Data at GRA  Index 0x%02x = 0x%02x \n", dosemu_regs->regs[GRAI], dosemu_regs->regs[GRA + dosemu_regs->regs[GRAI]]);
	    return (dosemu_regs->regs[GRA + dosemu_regs->regs[GRAI]]);
	} else
	    return (ext_video_port_in(port));
	break;
    case SEQ_I:
	debug_vid("Read Index SEQI 0x%02x\n", SEQI);
	return (dosemu_regs->regs[SEQI]);
    case SEQ_D:
	if (dosemu_regs->regs[SEQI] < 0x07) {
	    debug_vid("Read Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs->regs[SEQI], dosemu_regs->regs[SEQ + dosemu_regs->regs[SEQI]]);
	    return (dosemu_regs->regs[SEQ + dosemu_regs->regs[SEQI]]);
	} else
	    return (ext_video_port_in(port));
	break;
    case ATT_IW:
	debug_vid("Read Index ATTIW 0x%02x\n", att_d_index);
	return (att_d_index);
    case ATT_R:
	if (att_d_index > 20)
	    return (ext_video_port_in(port));
	else {
	    debug_vid("Read Index ATTR 0x%02x\n", dosemu_regs->regs[ATT + att_d_index]);
	    return (dosemu_regs->regs[ATT + att_d_index]);
	}
	break;
    case IS1_RC:
    case IS1_RM:
	debug_vid("Read ISR1_R 0x%02x\n", dosemu_regs->regs[ISR1]);
	isr_read = 1;		/* Next ATT write will be to the index */
	return (dosemu_regs->regs[ISR1]);
    case MIS_R:
	debug_vid("Read MIS_R 0x%02x\n", dosemu_regs->regs[MIS]);
	return (dosemu_regs->regs[MIS]);
    case IS0_R:
	debug_vid("Read ISR0_R 0x%02x\n", dosemu_regs->regs[ISR0]);
	return (dosemu_regs->regs[ISR0]);
    case PEL_IW:
	debug_vid("Read PELIW 0x%02x\n", dosemu_regs->regs[PELIW] / 3);
	return (dosemu_regs->regs[PELIW] / 3);
    case PEL_IR:
	debug_vid("Read PELIR 0x%02x\n", dosemu_regs->regs[PELIR] / 3);
	return (dosemu_regs->regs[PELIR] / 3);
    case PEL_D:
	debug_vid("Read PELIR Data 0x%02x\n", dosemu_regs->pal[dosemu_regs->regs[PELIR]]);
	return (dosemu_regs->pal[dosemu_regs->regs[PELIR]++]);
    case PEL_M:
	debug_vid("Read PELM  Data 0x%02x\n", dosemu_regs->regs[PELM] == 0 ? 0xff : dosemu_regs->regs[PELM]);
	return (dosemu_regs->regs[PELM] == 0 ? 0xff : dosemu_regs->regs[PELM]);
    default:
	return (ext_video_port_in(port));
    }
}

void video_port_out(u_char value, int port)
{
    debug_vid("Video write on port 0x%04x,byte 0x%04x.\n",port, value);
    switch (port) {
    case CRT_IC:
    case CRT_IM:
	debug_vid("Set Index CRTC 0x%02x\n", value);
	dosemu_regs->regs[CRTI] = value;
	break;
    case CRT_DC:
    case CRT_DM:
	if (dosemu_regs->regs[CRTI] <= 0x18) {
	    debug_vid("Write Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs->regs[CRTI], value);
	    dosemu_regs->regs[CRT + dosemu_regs->regs[CRTI]] = value;
	} else
	    ext_video_port_out(value, port);
	break;
    case GRA_I:
	debug_vid("Set Index GRAI 0x%02x\n", value);
	dosemu_regs->regs[GRAI] = value;
	break;
    case GRA_D:
	if (dosemu_regs->regs[GRAI] < 0x0a) {
	    debug_vid("Write Data at GRAI Index 0x%02x = 0x%02x \n", dosemu_regs->regs[GRAI], value);
	    dosemu_regs->regs[GRA + dosemu_regs->regs[GRAI]] = value;
	} else
	    ext_video_port_out(value, port);
	break;
    case SEQ_I:
	debug_vid("Set Index SEQI 0x%02x\n", value);
	dosemu_regs->regs[SEQI] = value;
	break;
    case SEQ_D:
	if (dosemu_regs->regs[SEQI] <= 0x05) {
	    debug_vid("Write Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs->regs[SEQI], value);
	    dosemu_regs->regs[SEQ + dosemu_regs->regs[SEQI]] = value;
	} else
	    ext_video_port_out(value, port);
	break;
    case ATT_IW:
	if (isr_read) {
	    debug_vid("Set Index ATTI 0x%02x\n", value & 0x1f);
	    att_d_index = value & 0x1f;
	    isr_read = 0;
	} else {
	    isr_read = 1;
	    if (att_d_index > 20)
		ext_video_port_out(value, port);
	    else {
		debug_vid("Write Data at ATT Index 0x%02x = 0x%02x \n", att_d_index, value);
		dosemu_regs->regs[ATT + att_d_index] = value;
	    }
	}
	break;
    case MIS_W:
	debug_vid("Set MISW 0x%02x\n", value);
	dosemu_regs->regs[MIS] = value;
	break;
    case PEL_IW:
	debug_vid("Set PELIW  to 0x%02x\n", value);
	dosemu_regs->regs[PELIW] = value * 3;
	break;
    case PEL_IR:
	debug_vid("Set PELIR to 0x%02x\n", value);
	dosemu_regs->regs[PELIR] = value * 3;
	break;
    case PEL_D:
	debug_vid("Put PEL_D[0x%02x] = 0x%02x\n", dosemu_regs->regs[PELIW], value);
	dosemu_regs->pal[dosemu_regs->regs[PELIW]] = value;
	dosemu_regs->regs[PELIW] += 1;
	break;
    case PEL_M:
	debug_vid("Set PEL_M to 0x%02x\n", value);
	dosemu_regs->regs[PELM] = value;
	break;
    case GR1_P:
	debug_vid("Set GR1_P to 0x%02x\n", value);
	dosemu_regs->regs[GR1P] = value;
	break;
    case GR2_P:
	debug_vid("Set GR2_P to 0x%02x\n", value);
	dosemu_regs->regs[GR2P] = value;
	break;
    default:
	ext_video_port_out(value, port);
    }
    return;
}



#ifdef WANT_DUMP_VIDEO
/* Generally I will request a video mode change switch quickly, and have the
   actuall registers printed out,as well as the dump of what dump_video
   received and fix the differences */

/* Dump what's in the dosemu_regs */
static void dump_video(void)
{
    u_short i;

    debug_vid("/* BIOS mode 0x%02X */\n", dosemu_regs->video_mode);
    debug_vid("static char regs[60] = {\n  ");
    for (i = 0; i < 12; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[CRT + i]);
    debug_vid("\n  ");
    for (i = 12; i < CRT_C; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[CRT + i]);
    debug_vid("\n  ");
    for (i = 0; i < 12; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[ATT + i]);
    debug_vid("\n  ");
    for (i = 12; i < ATT_C; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[ATT + i]);
    debug_vid("\n  ");
    for (i = 0; i < GRA_C; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[GRA + i]);
    debug_vid("\n  ");
    for (i = 0; i < SEQ_C; i++)
	debug_vid("0x%02X,", dosemu_regs->regs[SEQ + i]);
    debug_vid("\n  ");
    debug_vid("0x%02X", dosemu_regs->regs[MIS]);
    debug_vid("\n};\n");
    debug_vid("Extended Regs/if applicable:\n");
    for (i = 0; i < MAX_X_REGS; i++)
	debug_vid("0x%02X,", dosemu_regs->xregs[i]);
    debug_vid("0x%02X\n", dosemu_regs->xregs[MAX_X_REGS]);
    debug_vid("Extended 16 bit Regs/if applicable:\n");
    for (i = 0; i < MAX_X_REGS16; i++)
	debug_vid("0x%04X,", dosemu_regs->xregs16[i]);
    debug_vid("0x%04X\n", dosemu_regs->xregs16[MAX_X_REGS16]);
}
#endif


#if 0
/*
 * install_int_10_handler - install a handler for the video-interrupt (int 10)
 *                          at address INT10_SEG:INT10_OFFS. Currently
 *                          it's f100:4100.
 *                          The new handler is only installed if the bios
 *                          handler at f100:4100 is not the appropriate one.
 *                          (ie, if we don't use MDA with X)
 */

void install_int_10_handler(void)
{
    unsigned char *ptr;

    if (config.vbios_seg == 0xe000) {
	extern void bios_f000_int10ptr(), bios_f000();
	ptr = (u_char *) ((BIOSSEG << 4) + ((long) bios_f000_int10ptr - (long) bios_f000));
	*((long *) ptr) = 0xe0000003;
    }
}
#endif
