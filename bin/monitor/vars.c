#include "config.h"
#include "debug.h"
#include "emu.h"
#include "init.h"
#include "int.h"
#include "memoryemu.h"
#include "memutil.h"
#include "pagetable.h"
#include "termio.h"
#include "test_trap.h"
#include "types.h"
#include "uidt.h"
#include "vc.h"
#include "video.h"
#include "vm86.h"
#include "opa.h"

/* EVERYTHING uses this.  x86 is so register deficient, I hated
   passing it to every function.  The other globals should, however,
   be cleaned up. */
op_attr_t opa;
vmstate_t vmstate;



struct vm86_struct vm86s;

FILE *dbg_fd = 0;

/* FIXME: replace with pic stuff */
int irq0_pending = 0;

/* a simple gdt for testing purposes */
Bit8u default_gdt[24] = {0x00,   0,   0,   0,   0,   0,   0,   0,
 			 0xff,0xff,0x00,0x00,0x00,0x9a,0xcf,0x00,
			 0xff,0xff,0x00,0x00,0x00,0x92,0xcf,0x00};

ushort *prev_screen;  /* pointer to currently displayed screen   */

#ifndef NDEBUG
char *assert_file = 0;
u_int assert_line = 0;
#endif


/* debug stuff */
int debug_flags = 0;

/* config */
config_t config;
u_char permissions = 0;

/* bios */
unsigned int configuration = 0;

int virt_text_base = 0;
int phys_text_base = 0;
int CRT_I, CRT_D, IS1_R, FCR_W;
int terminal_fd = -1;
int terminal_pipe = 0;

/* video */
int video_mode = 0;
int v_8514_base = 0;            /* ports.c video/cirrus.c video/s3.c */
int char_blink = 0;             /* unused? */
int cursor_blink = 0;		/* unused? */
ushort cursor_shape = 0xe0f;
int cursor_col = 0;
int cursor_row = 0;
int video_combo = 0;		/* video.h */
int co = 80;
int li = 25;
int font_height = 16;
int vga_font_height = 16;
int text_scanlines = 400;
int video_page = 0;
unsigned int screen_mask = 0;
int video_subsys = 0;
struct screen_stat scr_state[MAX_OSS];	/* vc.h */
ushort *screen_adr = 0;		/* video.h */
int current_console = 0;
int console_fd = -1;
unsigned char video_initialized = 0;
u_char(*ext_video_port_in) (int port);
void (*ext_video_port_out) (unsigned char value, int port);
void (*set_bank_read) (unsigned char bank);
void (*set_bank_write) (unsigned char bank);
void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
