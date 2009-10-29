/* GUS PnP support: I added code here and in cpu.c which allows you to
   initialize the GUS under dosemu, while the Linux support is still in
   the works. It is clearly a better solution than the loadlin/warmboot
   method.
   You have to use -DGUSPNP in the local Makefile to activate it.
   WARNING: the HW addresses are hardcoded; I obtained them by running
   isapnptools. On your system they can change depending on the other
   PnP hardware you have; so check them before - AV
*/

#include <xok_include/errno.h>

#include "i386.h"
#include "bios.h"
#include "cmos.h"
#include "config.h"
#include "debug.h"
#include "dma.h"
#include "emu.h"
#include "hgc.h"
#include "int.h"
#include "memory.h"
#include "pic.h"
#include "port.h"
#include "priv.h"
#include "termio.h"
#include "timers.h"
#include "vc.h"
#include "video.h"
/* #include "config.h" */
/* #include "dosio.h" */
/* #include "dpmi.h" */
/* #include "ipx.h" */
/* #include "keyboard.h" */
/* #include "mouse.h" */
/* #include "serial.h" */
/* #include "vgaemu.h" */
/* #include "xms.h" */
#ifdef USE_SBEMU
#include "sound.h"
#endif
#ifdef NEW_KBD_CODE
#include "keyb_server.h"
#endif

extern Bit8u spkr_io_read(Bit32u port);
extern void spkr_io_write(Bit32u port, Bit8u value);
static void set_bitmap(unsigned long *bitmap, short base, short extent, int new_value);
int ioperm(unsigned int startport, unsigned int howmany, int onoff);


/* ====================================================================== */

#if 0
static long nyb2bin[16] = {
	0x30303030, 0x31303030, 0x30313030, 0x31313030,
	0x30303130, 0x31303130, 0x30313130, 0x31313130,
	0x30303031, 0x31303031, 0x30313031, 0x31313031,
	0x30303131, 0x31303131, 0x30313131, 0x31313131
};

static char *p2bin (unsigned char c)
{
    static char s[16] = "   [00000000]";
    
    ((long *)s)[1]=nyb2bin[(c>>4)&15];
    ((long *)s)[2]=nyb2bin[c&15];
    
    return s+3;
}
#endif


/* ====================================================================== */
/* PORT_DEBUG is to specify whether to record port writes to debug output.
* 0 means disabled.
* 1 means record all port accesses to 0x00 to 0xFF
* 2 means record ANY port accesses!  (big fat debugfile!)
* 3 means record all port accesses from 0x100 to 0x3FF
*/ 
#define PORT_DEBUG 3


/*                              3b0         3b4         3b8         3bc                     */
const unsigned char ATIports[]={ 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, /* 3b0-3bf */
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, /* 3c0-3cf */
                                 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0  /* 3d0-3df */
                               };

int isATIport(int port)
{
    return ((port & 0x3ff) == 0x102
	    || (port & 0x3fe) == 0x1ce
	    || (port & 0x3fc) == 0x2ec
	    || ((port & 0x3ff) >= 0x3b0 && (port & 0x3ff) < 0x3e0 && ATIports[(port & 0x3ff)-0x3b0])
	    || port == 0x46e8);
}


in_func_t in_funcs[5] = {(in_func_t)0, &inb, &inw, (in_func_t)0, &ind};

unsigned int inb(unsigned int port)
{
    static unsigned int cga_r = 0;
    static unsigned char r;

    ASSERT(port <= 0xffff);

/* it is a fact of (hardware) life that unused locations return all
   (or almost all) the bits at 1; some software can try to detect a
   card basing on this fact and fail if it reads 0x00 - AV */
    r = 0xff;

    /* reading from 0x80 is a NOP for delay */
    if (port == 0x80)
	return r;
    
    if (port_readable((u_int)port)) {
	r = read_port((u_int)port);
	return r;
    }
    
#if X_GRAPHICS
    if ( (config.X) &&
	 ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
	   ((port>=0x3c4) && (port<=0x3c5)) || /* erik@zaphod sequencer */
	   ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
	   ((port>=0x3D0) && (port<=0x3DD)) ) )
    {
        r=VGA_emulate_inb((u_int)port);
    }
#endif
#if 0
    else if (config.usesX) {
	debug_vid("HGC Portread: %d\n", (int) port);
	switch ((u_int)port) {
	case 0x03b8:		/* mode-reg */
	    r = safe_port_in_byte((u_int)port);
	    r = (r & 0x7f) | (hgc_Mode & 0x80);
	    break;
	case 0x03ba:		/* status-reg */
	    r = safe_port_in_byte((u_int)port);
	    break;
	case 0x03bf:		/* conf-reg */
	    set_ioperm(port, 1, 1);
	    r = port_in((u_int)port);
	    set_ioperm(port, 1, 0);
	    r = (r & 0xfd) | (hgc_Konv & 0x02);
	    break;
	case 0x03b4:		/* adr-reg */
	case 0x03b5:		/* data-reg */
	    r = safe_port_in_byte((u_int)port);
	    break;
	}
    }
#endif
    if (config.chipset && (port > 0x3b3) && (port < 0x3df) && config.mapped_bios)
	r = (video_port_in((u_int)port));
    else if (v_8514_base && ((port & 0x03fe) == v_8514_base) && (port & 0xfc00)) {
	enter_priv_on();
	iopl(3);
	r = port_in((u_int)port) & 0xff;
	iopl(0);
	leave_priv_setting();
	debug_vid("8514 inb [0x%04x] = 0x%02x\n", port, r);
    }
    else if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
	enter_priv_on();
	iopl(3);
	r = port_in(port) & 0xff;
	iopl(0);
	leave_priv_setting();
	debug_vid("ATI inb [0x%04x] = 0x%02x\n", port, r);
    }
    else switch ((u_int)port) {
    case 0x20:
    case 0x21:
	r = read_pic0((u_int)port);
	return r;
    case 0xa0:
    case 0xa1:
	r = read_pic1((u_int)port);
	return r;
    case 0x60:
    case 0x64:
	if (0) {   /* FIXME: if current console */
	    r = read_port((u_int)port);  /* FIXME: this isn't right... returning ff! */
	} else {
	    r = 0;
	}		  
	return r;
    case 0x61:
	r = spkr_io_read((u_int)port);
	return r;
    case 0x70:
    case 0x71:
	r = cmos_read((u_int)port);
	break;
    case 0x40:
    case 0x41:
    case 0x42:
	r = pit_inp((u_int)port);
	return r;
    case 0x43:
	r = port_inb((u_int)port);
	return r;
    case 0x3ba:
    case 0x3da:
	/* graphic status - many programs will use this port to sync with
	 * the vert & horz retrace so as not to cause CGA snow */
	debug_io("3ba/3da port inb\n");
	r = (cga_r ^= 1) ? 0xcf : 0xc6;
	break;
	
    case 0x3bc:
	debug_io("printer port inb [0x3bc] = 0\n");    /* 0 by default */
	break;
	
    case 0x3db:			/* light pen strobe reset, 0 by default */
	break;
	
#ifdef GUSPNP
    case 0x203:
    case 0x207:
    case 0x20b:
    case 0x220 ... 0x22f:
    case 0x320 ... 0x32f:
/*  case 0x279: */
	r = safe_port_in_byte (port);
	return r;
	
    case 0xa79:
	enter_priv_on();
	iopl (3);
	r = port_in (port);
	iopl (0);
	leave_priv_setting();
	break;
#endif
    default:
    {
	/* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
#if 0  /* XXX serial */
	unsigned int tmp = 0;
	for (tmp = 0; tmp < config.num_ser; tmp++)
	    if ((port & ~7) == com[tmp].base_port) {
		r = do_serial_in(tmp, port);
		return r;
	    }
#endif
	
#ifdef USE_SBEMU
	/* Sound I/O */
	if ((port & SOUND_IO_MASK) == config.sb_base) {
	    r=sb_io_read(port);
	    return r;
	}
	/* It seems that we might need 388, but this is write-only, at least in the
	   older chip... */
	/* MPU401 */
	if ((port & ~1) == config.mpu401_base) {
	    r=mpu401_io_read(port);
	    return r;
	}
#endif /* USE_SBEMU */
	
	/* DMA I/O */
	if ( ((port & ~15) == 0) || ((port & ~15) == 0x80) || ((port & ~31) == 0xC0) ) {
	    r=dma_io_read(port);
	}
	
	/* The diamond bug */
	else if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
	    enter_priv_on();
	    iopl(3);
	    r = port_in(port);
	    iopl(0);
	    leave_priv_setting();
	    debug_io(" Diamond inb [0x%x] = 0x%x\n", port, r);
	}
	else {
	    debug_io("default inb [0x%x] = 0x%02x\n", port, r);
	    debug_io("read port 0x%x dummy return 0xff because not in access list", port);
	}
    }
    }
    
/* Now record the port and the read value to debugfile if needed */
#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
    if (port < 0x100)
#elif PORT_DEBUG == 3
	if (port >= 0x100)
#endif
	    debug_io ("PORT: Rd 0x%04x -> 0x%02x\n", port, r);
#endif
    
    return r;    /* Return with port read value */
}

unsigned int inw(unsigned int port)
{
  if (v_8514_base && ((port & 0x03fd) == v_8514_base) && (port & 0xfc00)) {
    int value;

    enter_priv_on();
    iopl(3);
    value = port_in_w(port) & 0xffff;
    iopl(0);
    leave_priv_setting();
    debug_vid("8514 inw [0x%04x] = 0x%04x\n", port, value);
    return value;
  }
  else if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
    int value;

    enter_priv_on();
    iopl(3);
    value = port_in_w(port) & 0xffff;
    iopl(0);
    leave_priv_setting();
    debug_vid("ATI inw [0x%04x] = 0x%04x\n", port, value);
    return value;
  }
  return( read_port_w(port) );
}

unsigned int ind(unsigned int port)
{
  int v=read_port_w(port) & 0xffff;
  return (read_port_w(port+2)<< 16) | v;
}


out_func_t out_funcs[5] = {0, &outb, &outw, 0, &outd};

void outb(unsigned int port, unsigned int byte)
{
    static int lastport = 0;
    
    port &= 0xffff;
    if (port == 0x80)
	return;   /* used by linux, djgpp.. see above */
    byte &= 0xff;
    
#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
    if (port < 0x100)
#elif PORT_DEBUG == 3
	if (port >= 0x100)
#endif
	    debug_io("PORT: Wr 0x%04x <- 0x%02x\n", port, byte);
#endif
    
    if (port_writeable(port)) {
	debug_io("writing 0x%02x directly to port 0x%x\n", byte, port);
	write_port(byte, port);
    } else {
        debug_io("can't write 0x%02x directly to port 0x%x\n", byte, port);
	
	if (port > 0x3b3 && port < 0x3df && config.chipset && config.mapped_bios) {
	    video_port_out(byte, port);
	} else if (v_8514_base && ((port & 0x03fe) == v_8514_base) && (port & 0xfc00)) {
	    enter_priv_on();
	    iopl(3);
	    port_out(byte, port);
	    iopl(0);
	    leave_priv_setting();
	    debug_vid("8514 outb [0x%04x] = 0x%02x\n", port, byte);
	} else if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
	    enter_priv_on();
	    iopl(3);
	    port_out(byte, port);
	    iopl(0);
	    leave_priv_setting();
	    debug_vid("ATI outb [0x%04x] = 0x%02x\n", port, byte);
	    return;
	} else if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
	    /* The diamond bug */
	    enter_priv_on();
	    iopl(3);
	    port_out(byte, port);
	    iopl(0);
	    leave_priv_setting();
	    debug_io(" Diamond outb [0x%x] = 0x%x\n", port, byte);
	    return;
	} else
	    switch (port) {

		/* Port writes for enable/disable blinking character mode */
	    case 0x03C0:
	    {
		static int last_byte = -1;
		static int last_index = -1;
		static int flip_flop = 1;
		
		flip_flop = !flip_flop;
		if (flip_flop) {
		    if (last_index == 0x10)
			char_blink = (byte & 8) ? 1 : 0;
		    last_byte = byte;
		} else {
		    last_index = byte;
		}
		break;
	    }
    
	    case 0x3f0 ... 0x3f7:
		debug_disk("ignoring request to write 0x%02x to disk port 0x%x\n", byte, port);
		break;
		
	    case 0x20:
	    case 0x21:
		write_pic0(port, byte);
		break;
	    case 0x60:
	    case 0x64:  /* keyboard read status */
		write_port(byte, port);  /* FIXME: may want to virtualize _some_ of this
					    when we are not the current console */
		break;
	    case 0x61:  /* speaker control */
		spkr_io_write((u_int)port, byte);
		break;
	    case 0x70:
	    case 0x71:  /* CMOS */
		cmos_write(port, byte);
		break;
	    case 0xa0:
	    case 0xa1:
		write_pic1(port, byte);
		break;
	    case 0x40:
	    case 0x41:
	    case 0x42:
		pit_outp((u_int)port, byte);
		break;
	    case 0x43:
		pit_control_outp(port, byte);
		break;
		
	    case 0xf0:  /* (8087..80387) math coprocessor clear busy latch (write 00h) */
	    case 0xf1:  /* (8087..80387) math coprocessor reset (write 00h) */
		debug_disk("ignoring request to write 0x%02x to math copro port 0x%x\n", byte, port);
		/* I'll assume we have > 80387 so this isn't relevant? */
		break;
		
#ifdef GUSPNP
	    case 0x203:
	    case 0x207:
	    case 0x20b:
	    case 0x220 ... 0x22f:
	    case 0x320 ... 0x32f:
	    case 0x279:
		set_ioperm (port, 1, 1);
		port_out (byte, port);
		set_ioperm (port, 1, 0);
		break;
		
	    case 0xa79:
		enter_priv_on();
		iopl (3);
		port_out (byte, port);
		iopl (0);
		leave_priv_setting();
		break;
#endif
		
	    default:
	    {
		/* Port writes for cursor position */
		if ((port & 0xfffe) == READ_WORD(BIOS_VIDEO_PORT)) {
		    /* Writing to the 6845 */
		    static int last_port;
		    static int last_byte;
		    static int hi = 0, lo = 0;
		    int pos;
		    
		    debug_vid("Video Port outb [0x%04x]\n", port);
		    if (config.usesX) {
			leaveemu(ERR_ASSERT);  /* not ready for X... */
			set_ioperm(port, 1, 1);
			port_out(byte, port);
			set_ioperm(port, 1, 0);
		    } else {
			if ((port == READ_WORD(BIOS_VIDEO_PORT) + 1) && (last_port == READ_WORD(BIOS_VIDEO_PORT))) {
			    /* We only take care of cursor positioning for now. */
			    /* This code should work most of the time, but can
			       be defeated if i/o permissions change (e.g. by a vt
			       switch) while a new cursor location is being written
			       to the 6845. */
			    if (last_byte == 14) {
				hi = (unsigned char) byte;
				pos = (hi << 8) | lo;
				cursor_col = pos % 80;
				cursor_row = pos / 80;
#if 0   /* XXX */
				if (config.usesX)
				    poshgacur(cursor_col, cursor_row);
#endif
			    } else if (last_byte == 15) {
				lo = (unsigned char) byte;
				pos = (hi << 8) | lo;
				cursor_col = pos % 80;
				cursor_row = pos / 80;
#if 0  /* XXX */
				if (config.usesX)
				    poshgacur(cursor_col, cursor_row);
#endif
			    }
			}
			last_port = port;
			last_byte = byte;
		    }
		    break;
		} else {
#if X_GRAPHICS
		    if ( (config.X) &&
			 ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
			   ((port>=0x3c4) && (port<=0x3c5)) || /* erik@zaphod sequencer */
			   ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
			   ((port>=0x3D0) && (port<=0x3DD)) ) ) {
			VGA_emulate_outb(port, (unsigned char)byte);
		    } 
#endif
#if 0   /* XXX */
		    if (config.usesX) {
			debug_vid("HGC Portwrite: %d %d\n", (int) port, (int) byte);
			switch (port) {
			case 0x03b8:		/* mode-reg */
			    if (byte & 0x80)
				set_hgc_page(1);
			    else
				set_hgc_page(0);
			    set_ioperm(port, 1, 1);
			    port_out(byte & 0x7f, port);
			    set_ioperm(port, 1, 0);
			    break;
			case 0x03ba:		/* status-reg */
			    set_ioperm(port, 1, 1);
			    port_out(byte, port);
			    set_ioperm(port, 1, 0);
			    break;
			case 0x03bf:		/* conf-reg */
			    if (byte & 0x02)
				map_hgc_page(1);
			    else
				map_hgc_page(0);
			    set_ioperm(port, 1, 1);
			    port_out(byte & 0xFD, port);
			    set_ioperm(port, 1, 0);
			    break;
			}
		    }
#endif
		    break;
		}
		
		/* DMA I/O */
		if (((port & ~15) == 0) || ((port & ~15) == 0x80) || ((port & ~31) == 0xC0)) { 
		    dma_io_write(port, byte);
		}
		/* SERIAL PORT I/O.  Avoids port==0 for safety.  */
		/* The base serial port must be a multiple of 8. */
#if 0
		unsigned int tmp = 0;
		for (tmp = 0; tmp < config.num_ser; tmp++) {
		    if ((port & ~7) == com[tmp].base_port) {
			do_serial_out(tmp, port, byte);
			lastport = port;
			break;
		    }
		}
#endif
#ifdef USE_SBEMU
		/* Sound I/O */
		if ((port & SOUND_IO_MASK) == config.sb_base) {
		    sb_io_write(port, byte);
		    break;
		}
		else if ((port & ~3) == 388) {
		    adlib_io_write(port, byte);
		    break;
		}
		/* MPU401 */
		if ((port & ~1) == config.mpu401_base) {
		    mpu401_io_write(port, byte);
		    break;
		}
#endif /* USE_SBEMU */
		
		debug_io("default outb [0x%x] 0x%02x\n", port, byte);
		debug_io("write port 0x%x denied value %02x because not in access list\n",
			 port, (byte & 0xff));
	    }
	    }
    }
    
    lastport = port;
}

void
outw(unsigned int port, unsigned int value)
{
    if (v_8514_base && ((port & 0x03fd) == v_8514_base) && (port & 0xfc00)) {
	enter_priv_on();
	iopl(3);
	port_out_w(value, port);
	iopl(0);
	leave_priv_setting();
	debug_vid("8514 outw [0x%04x] = 0x%04x\n", port, value);
	return;
    }
    if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
	enter_priv_on();
	iopl(3);
	port_out(value, port);
	iopl(0);
	leave_priv_setting();
	debug_vid("ATI outb [0x%04x] = 0x%02x\n", port, value);
	return;
    }
    if(!write_port_w(value,port) ) {
	outb(port, value & 0xff);
	outb(port + 1, value >> 8);
    }
}

void
outd(unsigned int port, unsigned int value)
{
  port &= 0xffff;
  outw(port,value & 0xffff);
  outw(port+2,(unsigned int)value >> 16);
}


/* lifted from linux kernel, ioport.c */
/* Set EXTENT bits starting at BASE in BITMAP to value TURN_ON. */
static void
set_bitmap(unsigned long *bitmap, short base, short extent, int new_value)
{
	int mask;
	unsigned long *bitmap_base = bitmap + (base >> 5);
	unsigned short low_index = base & 0x1f;
	int length = low_index + extent;

	if (low_index != 0) {
		mask = (~0 << low_index);
		if (length < 32)
				mask &= ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
		length -= 32;
	}

	mask = (new_value ? ~0 : 0);
	while (length >= 32) {
		*bitmap_base++ = mask;
		length -= 32;
	}

	if (length > 0) {
		mask = ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
	}
}


int
ioperm(unsigned int startport, unsigned int howmany, int onoff)
{
    unsigned long bitmap[NIOPORTS/32];
    int err;

    if (startport + howmany > NIOPORTS)
	return ERANGE;

    if ((err = i386_get_ioperm(bitmap)) != 0)
	return err;
    debug_io("%sabling %x->%x\n", onoff ? "en" : "dis", startport, startport+howmany);
    /* now diddle the current bitmap with the request */
    set_bitmap(bitmap, startport, howmany, !onoff);

    return i386_set_ioperm(bitmap);
}


/* return status of io_perm call */
int
set_ioperm(int start, int size, int flag)
{
    int             tmp;

    if (!i_am_root)
	return -1;		/* don't bother */

    enter_priv_on();
    tmp = ioperm(start, size, flag);
    /* try i386_set_ioperm() ? */
    tmp = 0;
    leave_priv_setting();

    return tmp;
}
