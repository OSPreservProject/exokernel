/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 */

#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "vm86.h"

extern struct vm86_struct vm86s;

#define REGS  vm86s.regs
#define REG(reg) (REGS->tf_##reg)

#if 1
#define _AL      (*(Bit8u *)&vm86s.regs->tf_eax)
#define _BL      (*(Bit8u *)&vm86s.regs->tf_ebx)
#define _CL      (*(Bit8u *)&vm86s.regs->tf_ecx)
#define _DL      (*(Bit8u *)&vm86s.regs->tf_edx)
#define _AH      (*(((Bit8u *)&vm86s.regs->tf_eax)+1))
#define _BH      (*(((Bit8u *)&vm86s.regs->tf_ebx)+1))
#define _CH      (*(((Bit8u *)&vm86s.regs->tf_ecx)+1))
#define _DH      (*(((Bit8u *)&vm86s.regs->tf_edx)+1))
#define _AX      (*(Bit16u *)&vm86s.regs->tf_eax)
#define _BX      (*(Bit16u *)&vm86s.regs->tf_ebx)
#define _CX      (*(Bit16u *)&vm86s.regs->tf_ecx)
#define _DX      (*(Bit16u *)&vm86s.regs->tf_edx)
#define _SI      (*(Bit16u *)&vm86s.regs->tf_esi)
#define _DI      (*(Bit16u *)&vm86s.regs->tf_edi)
#define _BP      (*(Bit16u *)&vm86s.regs->tf_ebp)
#define _SP      (*(Bit16u *)&vm86s.regs->tf_esp)
#define _IP      (*(Bit16u *)&vm86s.regs->tf_eip)
#define _EAX     ((Bit32u)(vm86s.regs->tf_eax))
#define _EBX     ((Bit32u)(vm86s.regs->tf_ebx))
#define _ECX     ((Bit32u)(vm86s.regs->tf_ecx))
#define _EDX     ((Bit32u)(vm86s.regs->tf_edx))
#define _ESI     ((Bit32u)(vm86s.regs->tf_esi))
#define _EDI     ((Bit32u)(vm86s.regs->tf_edi))
#define _EBP     ((Bit32u)(vm86s.regs->tf_ebp))
#define _ESP     ((Bit32u)(vm86s.regs->tf_esp))
#define _EIP     ((Bit32u)(vm86s.regs->tf_eip))
#define _CS      ((Bit16u)(vm86s.regs->tf_cs))
#define _SS      ((Bit16u)(vm86s.regs->tf_ss))
#define _ES86    (*(Bit16u*)((u_int)REGS + sizeof(*REGS)     ))
#define _DS86    (*(Bit16u*)((u_int)REGS + sizeof(*REGS) +  4))
#define _FS86    (*(Bit16u*)((u_int)REGS + sizeof(*REGS) +  8))
#define _GS86    (*(Bit16u*)((u_int)REGS + sizeof(*REGS) + 12))
#define _EFLAGS  ((Bit32u)(vm86s.regs->tf_eflags))
#endif
#define _FLAGS   (*(Bit16u *)&vm86s.regs->tf_eflags)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)  (*(unsigned char *)&REG(e##reg))
#define HI(reg)  (*((unsigned char *)&REG(e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)	(*((unsigned short *)&REG(reg)))
#define HWORD(reg)	(*((unsigned short *)&REG(reg) + 1))

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type((LWORD(seg) << 4) + LWORD(e##reg))
#define ESEG_ADR(type, seg, reg) type((LWORD(seg) << 4) + REG(reg))

/* alternative SEG:OFF to linear conversion macro */
#define SEGOFF2LINEAR(seg, off)  ((((Bit32u)(seg)) << 4) + (off))

#define WRITE_FLAGS(val)    LWORD(eflags) = (val)
#define WRITE_FLAGSE(val)    REG(eflags) = (val)
#define READ_FLAGS()        LWORD(eflags)
#define READ_FLAGSE()        REG(eflags)

/*
 * nearly directly stolen from Linus : linux/kernel/vm86.c
 *
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Gcc makes a mess of it, so we do it inline and use non-obvious calling
 * conventions..
 */
#define pushw(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define popw(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb (%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popb(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#ifdef __linux__
static __inline__ void set_revectored(int nr, struct revectored_struct * bitmap)
{
    __asm__ __volatile__("btsl %1,%0"
			 : /* no output */
			 :"m" (*bitmap),"r" (nr));
}

static __inline__ void reset_revectored(int nr, struct revectored_struct * bitmap)
{
    __asm__ __volatile__("btrl %1,%0"
			 : /* no output */
			 :"m" (*bitmap),"r" (nr));
}
#endif

struct revectored_struct {
    unsigned long __map[8];			/* 256 bits */
};
static __inline__ void set_revectored(int nr, unsigned char * bitmap)
{
    __asm__ __volatile__("btsl %1,%0"
			 : /* no output */
			 :"m" (* (struct revectored_struct *)bitmap),"r" (nr));
}

static __inline__ void reset_revectored(int nr, unsigned char * bitmap)
{
    __asm__ __volatile__("btrl %1,%0"
			 : /* no output */
			 :"m" (* (struct revectored_struct *)bitmap),"r" (nr));
}


/* flags */
#define CF_MASK  (1 <<  0)
#define PF_MASK  (1 <<  2)
#define AF_MASK  (1 <<  4)
#define ZF_MASK  (1 <<  6)
#define SF_MASK  (1 <<  7)
#define DF_MASK  (1 << 10)
#define OF_MASK  (1 << 11)
#define RF_MASK  (1 << 16)
#define VIP_MASK	0x00100000
#define	VIF_MASK	0x00080000
#define	TF_MASK		0x00000100
#define	IF_MASK		0x00000200
#define	NT_MASK		0x00004000
#define	VM_MASK	        0x00020000
#define	AC_MASK		0x00040000
#define	ID_MASK		0x00200000

#define CF  CF_MASK
#define PF  PF_MASK
#define AF  AF_MASK
#define ZF  ZF_MASK
#define SF  SF_MASK
#define TF  TF_MASK
#define IF  IF_MASK
#define DF  DF_MASK
#define OF  OF_MASK
#define NT  NT_MASK
#define RF  RF_MASK
#define VM  VM_MASK
#define AC  AC_MASK
#define VIF VIF_MASK
#define VIP VIP_MASK
#define ID  ID_MASK

#define PE_MASK		0x00000001
#define MP_MASK		0x00000002
#define NE_MASK		0x00000020
#define TS_MASK		0x00000008
#define CD_MASK		0x40000000
#define PG_MASK		0x80000000

#define CR4_RESERVED_MASK	0xffffff00
#define CR0_WP_MASK	(1<<16)
#define IOPL_MASK	(3 << 12)

#define IS_VM86   (REG(eflags) & VM_MASK)
#define GUEST_CPL  (IS_VM86 ? 3 : (REG(cs) & 3))
#define GUEST_IOPL ((REG(eflags) & IOPL_MASK) >> 12)

/* Flag setting and clearing, and testing interrupt flag */
#define set_IF() ((REG(eflags) |= (VIF | IF)), (dpmiREG(eflags) |= IF), pic_sti())
#define clear_IF() ((REG(eflags) &= ~(VIF | IF)), (dpmiREG(eflags) &= ~IF), pic_cli())
#define isset_IF() ((REG(eflags) & VIF) != 0)
       /* carry flag */
#define set_CF() (REG(eflags) |= CF)
#define clear_CF() (REG(eflags) &= ~CF)
#define isset_CF() ((REG(eflags) & CF) != 0)
       /* direction flag */
#define set_DF() (REG(eflags) |= DF)
#define clear_DF() (REG(eflags) &= ~DF)
#define isset_DF() ((REG(eflags) & DF) != 0)
       /* nested task flag */
#define set_NT() (REG(eflags) |= NT)
#define clear_NT() (REG(eflags) &= ~NT)
#define isset_NT() ((REG(eflags) & NT) != 0)
       /* trap flag */
#define set_TF() (REG(eflags) |= TF)
#define clear_TF() (REG(eflags) &= ~TF)
#define isset_TF() ((REG(eflags) & TF) != 0)
       /* Virtual Interrupt Pending flag */
#define set_VIP()   (REG(eflags) |= VIP)
#define clear_VIP() (REG(eflags) &= ~VIP)
#define isset_VIP()   ((REG(eflags) & VIP) != 0)

#define setREG(eflags)(eflags) ((REG(eflags) = eflags), ((REG(eflags) & IF)? set_IF(): clear_IF()))
#define set_FLAGS(flags) ((_FLAGS = flags), ((_FLAGS & IF)? set_IF(): clear_IF()))
#define readREG(eflags)() (isset_IF()? (REG(eflags) | IF):(REG(eflags) & ~IF))
#define read_FLAGS()  (isset_IF()? (_FLAGS | IF):(_FLAGS & ~IF))

#define CARRY   set_CF()
#define NOCARRY clear_CF()

#define vflags read_FLAGS()

#if 0
  /* this is the array of interrupt vectors */
  struct vec_t {
      unsigned short offset;
      unsigned short segment;
  };

extern struct vec_t *ivecs;
#endif

#ifdef TRUST_VEC_STRUCT
 #define IOFF(i) ivecs[i].offset
 #define ISEG(i) ivecs[i].segment
#else
 #define IOFF(i) ((u_short *)0)[  (i)<<1    ]
 #define ISEG(i) ((u_short *)0)[ ((i)<<1) +1]
#endif

#define IVEC(i) ((ISEG(i)<<4) + IOFF(i))
#define SETIVEC(i, seg, ofs)	((u_short *)0)[ ((i)<<1) +1] = (u_short)seg; \
				((u_short *)0)[  (i)<<1    ] = (u_short)ofs

#define OP_IRET			0xcf

#ifndef USE_NEW_INT
 #define IS_REDIRECTED(i)	(ISEG(i) != BIOSSEG)
#else /* USE_NEW_INT */
 #include "memory.h" /* for INT_OFF */
 #define IS_REDIRECTED(i)	(IVEC(i) != SEGOFF2LINEAR(BIOSSEG, INT_OFF(i)))
#endif /* USE_NEW_INT */
#define IS_IRET(i)		(*(unsigned char *)IVEC(i) == OP_IRET)


#define sigcontext_struct sigcontext

#ifdef INIT_C2TRAP
/* convert from trap codes in trap.h back to source interrupt #'s
   (index into IDT) */
int c2trap[] = {
	0x06,				/* T_PRIVINFLT */
	0x03,				/* T_BPTFLT */
	0x10,				/* T_ARITHTRAP */
	0xff,				/* T_ASTFLT */
	0x0D,				/* T_PROTFLT */
	0x01,				/* T_TRCTRAP */
	0x0E,				/* T_PAGEFLT */
	0x11,				/* T_ALIGNFLT */
	0x00,				/* T_DIVIDE */
	0x02,				/* T_NMI */
	0x04,				/* T_OFLOW */
	0x05,				/* T_BOUND */
	0x07,				/* T_DNA */
	0x08,				/* T_DOUBLEFLT */
	0x09,				/* T_FPOPFLT */
	0x0A,				/* T_TSSFLT */
	0x0B,				/* T_SEGNPFLT */
	0x0C,				/* T_STKFLT */
};
 #else
  extern int c2trap[];
 #endif

#ifdef USE_INT_QUEUE
 __inline__ int do_hard_int(int);
#endif

unsigned int read_port_w(unsigned short port);
int write_port_w(unsigned int value,unsigned short port);
int do_soft_int(int intno);

#endif
