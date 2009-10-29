#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "types.h"
#include "emu.h"  /* for vesamode_type, etc */

typedef struct config_info {
    int hdiskboot;
    int boot_protected;
    int exit_on_hlt;
#ifdef X86_EMULATOR
    boolean cpuemu;
    int emuspeed;
#endif
    /* for video */
    boolean console;
    boolean console_video;
    boolean graphics;
    boolean vga;
    boolean X;
    u_short cardtype;
    u_short chipset;
    boolean pci;
    boolean pci_video;
    u_short gfxmemsize;		/* for SVGA card, in K */
    /* u_short term_method; */	/* Terminal method: ANSI or NCURSES */
    u_short term_color;		/* Terminal color support on or off */
    /* u_short term_updatelines; */	/* Amount to update at a time */
    u_short term_updatefreq;		/* Terminal update frequency */
    u_short term_charset;		/* Terminal Character set */
    u_short term_esc_char;	        /* ASCII value used to access slang help screen */
    /* u_short term_corner; */       /* Update char at lower-right corner */
    u_short X_updatelines;           /* Amount to update at a time */
    u_short X_updatefreq;            /* X update frequency */
    char    *X_display;              /* X server to use (":0") */
    char    *X_title;                /* X window title */
    char    *X_icon_name;
    char    *X_font;
    char    *X_mgrab_key;		/* KeySym name to activate mouse grab */
    /* "" turns it of, NULL gives the default ("Home") */
    int     X_blinkrate;
    int     X_sharecmap;
    int     X_mitshm;                /* use MIT SHM extension */
    int     X_fixed_aspect;          /* keep initial aspect ratio while resizing windows */
    int     X_aspect_43;             /* set aspect ratio to 4:3 */
    int     X_lin_filt;              /* interpolate linear */
    int     X_bilin_filt;            /* dto, bilinear */
    int     X_winsize_x;             /* initial window width */
    int     X_mode13fact;            /* initial size factor for mode 0x13 */
    int     X_winsize_y;             /* initial window height */
    unsigned X_gamma;		/* gamma correction value */
    u_short vgaemu_memsize;		/* for VGA emulation */
    vesamode_type *vesamode_list;	/* chained list of VESA modes */
    int     X_lfb;			/* support VESA LFB modes */
    int     X_pm_interface;		/* support protected mode interface */
    boolean fullrestore;
    boolean force_vt_switch;         /* in case of console_video force switch to emu VT at start */
    int     dualmon;
    
    u_short usesX;  /* !=0 if dosemu owns an X window */
    
    boolean console_keyb;
    boolean kbd_tty;
    boolean X_keycode;	/* use keycode field of event structure */
    boolean exitearly;
    int     realcpu;
    boolean mathco, smp;
    boolean ipxsup;
    boolean vnet;
    boolean keybint;
    boolean dosbanner;
    boolean allowvideoportaccess;
    boolean rdtsc;
    boolean timers;
    boolean mouse_flag;
    boolean mapped_bios;	/* video BIOS */
    char *vbios_file;	/* loaded VBIOS file */
    char *bios_file;	/* loaded BIOS file */
    boolean vbios_copy;
    int vbios_seg;           /* VGA-BIOS-segment for mapping */
    int vbios_size;          /* size of VGA-BIOS (64K for vbios_seg=0xe000
				32K for vbios_seg=0xc000) */

    struct vdisk *bootdisk;
    int  fastfloppy;
    char *emusys;		/* map CONFIG.SYS to CONFIG.EMU */
    char *emubat;		/* map AUTOEXEC.BAT to AUTOEXEC.EMU */
    char *emuini;           /* map system.ini to  system.EMU */

    u_short speaker;		/* 0 off, 1 native, 2 emulated */
    u_short fdisks, hdisks;
    u_short num_lpt;
    u_short num_ser;
    u_short num_mice;
    
    int pktflags;		/* global flags for packet driver */
    
    unsigned int update, freq;	/* temp timer magic */
    unsigned int wantdelta, realdelta;
    unsigned long cpu_spd;		/* (1/speed)<<32 */
    unsigned long cpu_tick_spd;	/* (1.19318/speed)<<32 */
    
    unsigned int hogthreshold;
    
    int mem_size, phys_mem_size, xms_size, ems_size, dpmi, max_umb;
    int secure;
    unsigned int ems_frame;
    char must_spare_hardware_ram;
    char hardware_pages[ ((HARDWARE_RAM_STOP-HARDWARE_RAM_START) >> 12)+1 ];
    

    int sillyint;            /* IRQ numbers for Silly Interrupt Generator 
				(bitmask, bit3..15 ==> IRQ3 .. IRQ15) */
    
    struct keytable_entry *keytable;
    
    unsigned short detach;
    unsigned char *debugout;
    unsigned char *pre_stroke;  /* pointer to keyboard pre strokes */
    unsigned char debugger;

    /* Lock File business */
    char *tty_lockdir;	/* The Lock directory  */
    char *tty_lockfile;	/* Lock file pretext ie LCK.. */
    boolean tty_lockbinary;	/* Binary lock files ? */
    
    /* Sound emulation */
    __u16 sb_base;
    __u8 sb_dma;
    __u8 sb_irq;
    char *sb_dsp;
    char *sb_mixer;
    __u16 mpu401_base;

    Bit8u idle_int;
} config_t;


extern struct config_info config;

void config_init(int argc, char **argv);


#endif
