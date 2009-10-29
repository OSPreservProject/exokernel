#ifndef _DECODE_H_
#define _DECODE_H_

#include "types.h"
#include "handler_utils.h"

#define MOD(modrm)         (((modrm)>>6)&3)   /*  */
#define REG_OPCODE(modrm)  (((modrm)>>3)&7)   /* extension to the instruction */
#define RM(modrm)          ((modrm)&7)        /* what reg we're talking about */
#define SCALE(sib)         (((sib)>>6)&3)
#define INDEX(sib)         (((sib)>>3)&7)
#define BASE(sib)          ((sib)&7)


int decode_rm(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp);
int decode_rm16(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp);
int decode_rm32(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp);
int decode_mov(unsigned char **csp, Bit32u *src, Bit32u *dst,
	       u_int *bytes, u_int *srcmem, u_int *dstmem);

int decode_mov_ObAL_OvAX(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);
int decode_mov_ALOb_AXOv(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);
int decode_mov_EG_GE_ES_SE(unsigned char **lina, Bit32u *src, Bit32u *dst,
			   u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);
int decode_mov_imm8_reg8(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);
int decode_mov_imm_reg(unsigned char **lina, Bit32u *src, Bit32u *dst,
		       u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);
int decode_mov_EbIb_EvIv(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op);

#endif
