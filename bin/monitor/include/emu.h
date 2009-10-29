#ifndef EMU_H_
#define EMU_H_

#include "types.h"
#include "pagetable.h"
#include "cpu.h"
#include "disks.h"
#include "debug.h"
#include "set.h"
#include "descriptors.h"
#include "handler_utils.h"



#if 1 /* Set to 1 to use Silly Interrupt generator */
#define SIG 1
typedef struct { int fd; int irq; } SillyG_t;
#endif

extern fd_set fds_sigio, fds_no_sigio;
extern unsigned int use_sigio;
extern unsigned int not_use_sigio;
extern int terminal_pipe;
extern int terminal_fd;
extern int running_kversion;

#define i_am_root 1  /* neadless to have it variable, it will be allways 1
                      * If dosemu isn't suid root, it will terminate
                      */

extern int screen, screen_mode;

extern char *cl,		/* clear screen */
*le,				/* cursor left */
*cm,				/* goto */
*ce,				/* clear to end */
*sr,				/* scroll reverse */
*so,				/* stand out start */
*se,				/* stand out end */
*md,				/* hilighted */
*mr,				/* reverse */
*me,				/* normal */
*ti,				/* terminal init */
*te,				/* terminal exit */
*ks,				/* init keys */
*ke,				/* ens keys */
*vi,				/* cursor invisible */
*ve;				/* cursor normal */

/* the fd for the keyboard */ 
extern int console_fd;

/* the file descriptor for /dev/mem when mmap'ing the video mem */
extern int mem_fd;
extern int in_readkeyboard;

/* X-pipes */
extern int keypipe;
extern int mousepipe;

extern int li, co;	/* lines, columns */
extern int scanseq;
extern int cursor_row;
extern int cursor_col;

extern void run_vm86(void);
extern void     vm86_GP_fault();

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

void char_out(u_char, int);

struct ioctlq {
    int fd, req, param3;
    int queued;
};

void do_queued_ioctl(void);
int queue_ioctl(int, int, int), do_ioctl(int, int, int);
void keybuf_clear(void);

int set_ioperm(int, int, int);

/* one-entry queue ;-( for ioctl's */
extern struct ioctlq iq;
/* extern u_char in_ioctl;
extern struct ioctlq curi;


/* this macro can be safely wrapped around a system call with no side
 * effects; using a feature of GCC, it returns the same value as the
 * function call argument inside.
 *
 * this is best used in places where the errors can't be sanely handled,
 * or are not expected...
 */
#define DOS_SYSCALL(sc) ({ int s_tmp = (int)sc; \
  if (s_tmp == -1) \
    error("SYSCALL ERROR: %d in file %s, line %d: expr=\n\t%s\n", \
	  s_tmp, __FILE__, __LINE__, #sc); \
  s_tmp; })

#define RPT_SYSCALL2(sc) ({ int s_tmp; \
   do { \
	  s_tmp = sc; \
	  s_err = errno; \
	  if (errno == EINTR) {\
	    debug("Recursive run_irqs() RPT_SYSCALL2()\n"); \
	    handle_signals(); \
	/*    run_irqs(); */ \
	  } \
      } while ((s_tmp == -1) ); \
  s_tmp; })

/* would like to have this in memory.h (but emu.h is included before memory.h !) */
#define HARDWARE_RAM_START 0xc8000
#define HARDWARE_RAM_STOP  0xf0000

typedef struct vesamode_type_struct {
  struct vesamode_type_struct *next;
  unsigned width, height, color_bits;
} vesamode_type;


#define SPKR_OFF	0
#define SPKR_NATIVE	1
#define SPKR_EMULATED	2

/*
 * Right now, dosemu only supports two serial ports.
 */
#define SIG_SER		SIGTTIN

#define SIG_TIME	SIGALRM
#define TIMER_TIME	ITIMER_REAL

#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)

extern int port_readable(unsigned short);
int ports_writeable(unsigned short port, unsigned short bytes);
extern unsigned char read_port(unsigned short);
extern int write_port(unsigned int, unsigned short);
extern void cpu_setup(void);
extern void real_run_int(int);
#ifdef USE_NEW_INT
  #define run_int do_int
#else
  #define run_int real_run_int
#endif
extern int mfs_redirector(void);
extern void int10(void);
extern void int13(u_char);
extern void int14(u_char);
extern void int17(u_char);
extern void io_select(fd_set);
extern int pd_receive_packet(void);
extern int printer_tick(u_long);
extern int printer_tick(u_long);
extern void floppy_tick(void);
extern void open_kmem(void);
extern void close_kmem(void);
extern void CloseNetworkLink(int);
extern void disk_init(void);
extern void serial_init(void);
extern void close_all_printers(void);
extern void release_ports(void);
extern void serial_close(void);
extern void disk_close_all(void);
extern void init_all_printers(void);
extern int mfs_inte6(void);
extern void pkt_helper(void);
/* typedef struct fault_context state_t; */
/* extern short pop_word(struct fault_context *); */
extern void ems_init(void);
extern int GetDebugFlagsHelper(char *, int);
extern int SetDebugFlagsHelper(char *);
extern void add_to_io_select(int, unsigned char);
extern void remove_from_io_select(int, unsigned char);
extern void sigquit(int);
/* extern void sigalrm(int, int, struct sigcontext *); */
/* extern void sigio(int, int, struct sigcontext *); */

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGWINCH
#define SIG_ACQUIRE     SIGUSR2


/* DANG_BEGIN_FUNCTION NEWSETQSIG
 *
 * arguments:
 * sig - the signal to have a handler installed to.
 * fun - the signal handler function to install
 *
 * description:
 *  All signals that wish to be handled properly in context with the
 * execution of vm86() mode, and signals that wish to use non-reentrant
 * functions should add themselves to the ADDSET_SIGNALS_THAT_QUEUE define
 * and use SETQSIG(). To that end they will also need to be set up in an
 * order such as SIGIO.
 *
 * DANG_END_FUNCTION
 *
 */
#define ADDSET_SIGNALS_THAT_QUEUE(x) \
do { \
       sigaddset(x, SIGIO); \
       sigaddset(x, SIG_TIME); \
       sigaddset(x, SIG_RELEASE); \
       sigaddset(x, SIG_ACQUIRE); \
} while(0)

extern inline void SIGNAL_save( void (*signal_call)(void) );
extern inline void handle_signals(void);

extern void signal_init(void);
extern void device_init(void);
extern void hardware_setup(void);
extern void memory_init(void);
extern void timer_interrupt_init(void);
extern void keyboard_flags_init(void);
extern void video_config_init(void);
extern void video_close(void);
extern void serial_helper(void);
extern void mouse_helper(void);
extern void cdrom_helper(void);
extern int pkt_int(void);
extern void read_next_scancode_from_queue (void);
extern unsigned short detach (void);




typedef int (*emu_func_t)(unsigned char *lina);
extern emu_func_t opcode_table[0x100];
void unknown(unsigned char *lina);






typedef struct {
    Bit32u cr[5];		/* virtualized control registers */
    Set    cr3;			/* set of linear addresses mapping guest page table directory */
				/* vmstate.cr[3] is physical; vmstate.cr3 are virtual */
    Bit32u dr[8];		/* virtualized control registers */

    /* char fpu[108]; */	/* FPU state takes max 108 bytes; not currently virtualized */

    Bit16u tr_sel;		/* task register selector */
    Bit32u tr_base;
    Bit32u tr_limit;
    Bit8u  tr_attribs;		/* P DPL 0 1 0 B 1 */
    Bit16u tr_loaded;

    Bit32u g_ldt_pbase;		/* GUEST physical address of GUEST'S ldt */
    Set    g_ldt_base;		/* set of linear addresses mapping guest ldt */
    Bit16u g_ldt_limit;
    /* attribs */
    Bit16u g_ldt_loaded;

    Bit32u g_gdt_pbase;		/* GUEST physical address of GUEST'S gdt */
    Set    g_gdt_base;		/* set of linear addresses mapping guest gdt */
    Bit16u g_gdt_limit;
    Bit16u g_gdt_loaded;

    Bit32u g_idt_pbase;		/* GUEST physical address of GUEST'S idt */
    Bit32u g_idt_base;		/* linear address mapping guest idt */
    Bit16u g_idt_limit;
    Bit16u g_idt_loaded;

    Bit32u h_gdt_base;		/* virtual address of HOST'S gdt */
    Bit32u h_idt_base;		/* virtual address of HOST'S idt */
    Bit16u h_gdt_limit;
    Bit16u h_idt_limit;

    Bit32u *gp2hp;		/* mapping from guest physical page to trust host physical page; ppages elems */
    Bit32u db_addr;		/* for debugging; we expect to break here */
    Bit8u  db_data[4];		/* data to restore after #db */

    Bit32u ppages;		/* # of pages of (fake) RAM */
    u_int  eid;			/* cache to avoid extra syscalls */

    Bit32u exc_erc;
    Bit8u  exc;			/* boolean, guest caused exception? */
    Bit8u  exc_vect;

    Bit8u  prot_transition;
} vmstate_t;

extern vmstate_t vmstate;
#define IS_PE     (vmstate.cr[0] & 1)

#endif
