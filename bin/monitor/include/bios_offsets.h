#ifndef _BIOS_OFFSETS_H
#define _BIOS_OFFSETS_H

/* split segment 0xf000 into two region, 0xf0000 to 0xf7fff is read-write */
/*                                       0xf8000 to 0xfffff is read-only  */
/* so put anything need read-write into BIOSSEG and anything read-only */
/* to ROMBIOSSEG  */
#define BIOSSEG		0xf000
#define ROMBIOSSEG	0xf800
#define BIOSSIZE        0x10000
#define BIOSSIG         0xfc

#define INT41_SEG       BIOSSEG
#define INT41_OFF       0xe401
#define INT41_ADD	((INT41_SEG << 4) + INT41_OFF)
#define INT46_SEG       INT41_SEG
#define INT46_OFF       INT41_OFF+0x10
#define INT46_ADD	INT41_ADD+0x10

/* For int15 0xc0 */
#define ROM_CONFIG_SEG  BIOSSEG
#define ROM_CONFIG_OFF  0xe6f5
#define ROM_CONFIG_ADD	((ROM_CONFIG_SEG << 4) + ROM_CONFIG_OFF)

#define INT1D_SEG	BIOSSEG
#define INT1D_OFF	0xf0a4
#define INT1D_ADD	((INT1D_SEG << 4) + INT1D_OFF)

#define INT1E_SEG	BIOSSEG
#define INT1E_OFF	0xefc7
#define INT1E_ADD	((INT1E_SEG << 4) + INT1E_OFF)



#endif
