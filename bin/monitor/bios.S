/* Virtual BIOS file

   preprocess as:
   gcc -Iinclude -I../../sys -E -P bios.S > bios.asm

   assemble as:
   nasm -o bios.bin -f bin bios.asm 

   This creates a 64k image which is to be mmap'ed into the address
   space at f000:0000.


*/

#include "bios_offsets.h"

; INT 41 - SYSTEM DATA - HARD DISK 0 PARAMETER TABLE ADDRESS [NOT A VECTOR!]
	times INT41_OFF-($-$$) db 0x00

	/* primary master */
	/* primary slave */
	/* secondary master */
	/* secondary slave */
%rep 4
	dw       0 /* number of cylinders */
	db       0 /* number of heads */
	dw       0 /* starting reduced write current cylinder (XT only, 0 for others) */
	dw  0xffff /* starting write precompensation cylinder number */
	db       0 /* maximum ECC burst length (XT only) */
	db       0 /* control byte (see #03197,#03198) */
	db       0 /* standard timeout (XT only, 0 for others) */
	db       0 /* formatting timeout (XT and WD1002 only, 0 for others) */
	db       0 /* timeout for checking drive (XT and WD1002 only, 0 for others) */
	dw  0xffff /* cylinder number of landing zone (AT and later only) */
	db       0 /* number of sectors per track (AT and later only) */
	db       0 /* reserved */
%endrep


; INT 15 C0 - SYSTEM - GET CONFIGURATION
; this is just a place to store the table; int 15 c0 returns a es:bx pointer to it.
	times ROM_CONFIG_OFF-($-$$) db 0x00

	dw	9	/* 9 byte table */
	db	0xfc	/* PC AT */
	dw	0x401	/* bios revision 4 */
	db	0x70	/* no mca, no ebios, no wat, keybint,
			   rtc, slave 8259, no dma 3 */
	dd	0

; INT 1E - SYSTEM DATA - DISKETTE PARAMETERS
; some data here was swiped from a running FreeDOS OS.
	times INT1E_OFF-($-$$) db 0x00

	db      0xdf    /* bits 7-4: step rate (Fh=2ms,Eh=4ms,Dh=6ms,etc.) */
	                /* bits 3-0: head unload time (0Fh = 240 ms) */
	db      0x02    /* bits 7-1: head load time (01h = 4 ms) */
	                /* bit 0: non-DMA mode (always 0) */
	db      0x25    /* delay until motor turned off (in clock ticks) */
	db      0x02    /* bytes per sector (00h = 128, 01h = 256, 02h = 512, 03h = 1024) */
	db      0x24    /* sectors per track */
	db      0x1b    /* length of gap between sectors (2Ah for 5.25", 1Bh for 3.5") */
	db      0xff    /* data length (ignored if bytes-per-sector field nonzero) */
	db      0x6c    /* gap length when formatting (50h for 5.25", 6Ch for 3.5") (freedos: 54) */
	db      0xf6    /* format filler byte (default F6h) */
	db      0x0f    /* head settle time in milliseconds */
	db      0x08    /* motor start time in 1/8 seconds */
	db      0x4f    /* maximum track number (freedos: e9) */
	db      0x89    /* data transfer rate (freedos: 89) */
	db      0x04    /* drive type in CMOS (freedos: 08) */


; INT 1D - SYSTEM DATA - VIDEO PARAMETER TABLES
; some values swiped from a running FreeDOS OS.
	times INT1D_OFF-($-$$) db 0x00

	                /* 6845 register values for modes 00h and 01h */
	db 0x38, 0x28, 0x2d, 0x0a, 0x1f, 0x06, 0x19, 0x1c, 0x02, 0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00
	                /* 6845 register values for modes 02h and 03h */
	db 0x71, 0x50, 0x5a, 0x0a, 0x1f, 0x06, 0x19, 0x1c, 0x02, 0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00
	                /* 6845 register values for modes 04h and 05h */
	db 0x38, 0x28, 0x2d, 0x0a, 0x7f, 0x06, 0x64, 0x70, 0x02, 0x01, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00
	                /* 6845 register values for modes 06h and 07h */
	db 0x61, 0x50, 0x52, 0x0f, 0x19, 0x06, 0x19, 0x19, 0x02, 0x0d, 0x0b, 0x0c, 0x00, 0x00, 0x00, 0x00
	dw       0x0800 /* bytes in video buffer for modes 00h and 01h (0800h) */
	dw       0x1000 /* bytes in video buffer for modes 02h and 03h (1000h) */
	dw       0x4000 /* bytes in video buffer for modes 04h and 05h (4000h) */
	dw       0x4000 /* bytes in video buffer for mode 06h (4000h) */
	                /* columns on screen for each of modes 00h through 07h */
	db 0x28, 0x28, 0x50, 0x50, 0x28, 0x28, 0x50, 0x50
	                /* CRT controller mode bytes for each of modes 00h through 07h */
	db 0x2c, 0x28, 0x2d, 0x29, 0x2a, 0x2e, 0x1e, 0x29


; end
	times 0xfff6-($-$$) db 0x00
	db	'02/25/93',0  /* our bios date */
	db	BIOSSIG       /* model byte = IBM AT */
