/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


#ifndef __APIC_H__
#define __APIC_H__

/* 
 * APIC module: includes information on both APIC and IOAPIC
 *
 * Reader should refer to 82093AA IOAPIC specification 
 * from Intel, and the volume 3 of PPro Family Developer's
 * Manual (OS writers manual). Page number below refers to 
  * the page number from those specifications.
 */



/* 
 * First, we have definitions for local APIC
 */

/* in high memory, default addr for the read/write local APIC 
 * memory, mapped to va LAPIC (see mmu.h) */
#define LAPIC_ADDR_DEFAULT   0xFEE00000

/* in high memory, default addr for the read/write IO APIC
 * memory, mapped to va IOAPIC (see mmu.h) */
#define IOAPIC_ADDR_DEFAULT  0xFEC00000


/* with in the local APIC */
#define APIC_ID			0x20
#define	GET_APIC_ID(x)		(((x)>>24)&0x0F)
#define	APIC_VERSION		0x30
#define	APIC_TASKPRI		0x80
#define	APIC_TPRI_MASK		0xFF
#define	APIC_ARBPRI		0x90
#define	APIC_PROCPRI		0xA0
#define	APIC_EOI		0xB0
#define	APIC_EIO_ACK		0x0  /* write this to the EOI register */
#define	APIC_RRR		0xC0
#define	APIC_LDR		0xD0
#define	GET_APIC_LOGICAL_ID(x)	(((x)>>24)&0xFF)
#define	APIC_DFR		0xE0
#define	GET_APIC_DFR(x)		(((x)>>28)&0x0F)
#define	SET_APIC_DFR(x)		((x)<<28)
#define	APIC_SPIV		0xF0
#define	APIC_ISR		0x100
#define	APIC_TMR		0x180
#define APIC_IRR		0x200
#define APIC_ESR		0x280
#define	APIC_ESR_SEND_CS	0x00001
#define	APIC_ESR_RECV_CS	0x00002
#define	APIC_ESR_SEND_ACC	0x00004
#define	APIC_ESR_RECV_ACC	0x00008
#define	APIC_ESR_SENDILL	0x00020
#define	APIC_ESR_RECVILL	0x00040
#define	APIC_ESR_ILLREGA	0x00080
#define	APIC_ICR		0x300
#define	APIC_ICR2		0x310
#define	APIC_DEST_FIELD		0x00000
#define	APIC_DEST_SELF		0x40000
#define	APIC_DEST_ALLINC	0x80000
#define	APIC_DEST_ALLBUT	0xC0000
#define	APIC_DEST_RR_MASK	0x30000
#define	APIC_DEST_RR_INVALID	0x00000
#define	APIC_DEST_RR_INPROG	0x10000
#define	APIC_DEST_RR_VALID	0x20000
#define	APIC_DEST_LEVELTRIG	0x08000
#define	APIC_DEST_ASSERT	0x04000
#define	APIC_DEST_BUSY		0x01000
#define	APIC_DEST_LOGICAL	0x00800
#define	APIC_DM_FIXED		0x00000
#define	APIC_DM_LOWEST		0x00100
#define	APIC_DM_SMI		0x00200
#define	APIC_DM_REMRD		0x00300
#define	APIC_DM_NMI		0x00400
#define	APIC_DM_INIT		0x00500
#define	APIC_DM_STARTUP		0x00600
#define	APIC_DEST_VECTOR_MASK	0x000FF
#define APIC_DELSTAT_MASK       0x00001000
#define APIC_DELSTAT_IDLE       0x00000000
#define APIC_DELSTAT_PEND       0x00001000
#define	GET_APIC_DEST_FIELD(x)	(((x)>>24)&0xFF)
#define	SET_APIC_DEST_FIELD(x)	((x)<<24)
#define	APIC_LVTT		0x320
#define	APIC_LVT0		0x350
#define	APIC_LVT_TIMER_PERIODIC	(1<<17)
#define	APIC_LVT_MASKED		(1<<16)
#define	APIC_LVT_LEVEL_TRIGGER	(1<<15)
#define	APIC_LVT_REMOTE_IRR	(1<<14)
#define	APIC_INPUT_POLARITY	(1<<13)
#define	APIC_SEND_PENDING	(1<<12)
#define	GET_APIC_DELIVERY_MODE(x)	(((x)>>8)&0x7)
#define	SET_APIC_DELIVERY_MODE(x,y)	(((x)&~0x700)|((y)<<8))
#define	APIC_MODE_FIXED		0x0
#define	APIC_MODE_NMI		0x4
#define	APIC_MODE_EXINT		0x7
#define APIC_LVT1		0x360
#define	APIC_LVERR		0x370
#define	APIC_TMICT		0x380
#define	APIC_TMCCT		0x390
#define	APIC_TDCR		0x3E0
#define	APIC_TDR_DIV_1		0xB
#define	APIC_TDR_DIV_2		0x0
#define	APIC_TDR_DIV_4		0x1
#define	APIC_TDR_DIV_8		0x2
#define	APIC_TDR_DIV_16		0x3
#define	APIC_TDR_DIV_32		0x8
#define	APIC_TDR_DIV_64		0x9
#define	APIC_TDR_DIV_128	0xA

#define APIC_SPIV_ADDR		LAPIC+APIC_SPIV
#define APIC_EOI_ADDR		LAPIC+APIC_EOI


#ifndef __ASSEMBLER__
#ifdef KERNEL

#include <xok/mmu.h>	    /* memory map addr (IOAPIC and LAPIC) */

#ifdef __SMP__


/* The distributed APIC system is made of two parts,
 * the local APICs that are on each of the CPUs, and the
 * central I/O APIC
 */

#define NR_IOAPIC_IRQS  24

/* the real local apic address as detected in 
 * the configuration table, see smp.c */
extern unsigned long local_apic_addr;		/* local apic addr */


/* local APIC read and write routines: according to Intel,
 * you should put reads between APIC writes.
 * Intel Pentium processor specification update [11AP, pg 64]
 * "Back to Back Assertions of HOLD May Cause Lost APIC Write Cycle" */

extern __inline void apic_write(unsigned long reg, unsigned long v)
{
  *((volatile unsigned long *)(LAPIC+reg))=v;
}

extern __inline unsigned long apic_read(unsigned long reg)
{
  return *((volatile unsigned long *)(LAPIC+reg));
}



/*
 * centralized IO APIC definitions
 */


/* the real io apic address, detected in the configuration
 * table. see smp.c for detail */
extern unsigned long io_apic_addr;		/* IO apic addr */

/* IO APIC register IO registers - pg 9 of IOAPIC spec */
#define IOAPIC_REGSEL IOAPIC
#define IOAPIC_IOWIN  IOAPIC_REGSEL+0x10
#define IOAPIC_ID     0x00
#define IOAPIC_VER    0x01
#define IOAPIC_ARB    0x02
#define IOAPIC_RTBL_W1 0x10
#define IOAPIC_RTBL_W2 0x11

/* IO APIC registers - pg 9-10 of IOAPIC spec */

struct ioapic_reg_0 {
  unsigned int 
        reserved2 : 24,
     	       id : 4,
        reserved1 : 4;
};

struct ioapic_reg_1 {
  unsigned int
        version : 8,
      reserved2 : 8,
        entries : 8,
      reserved1 : 8;
};

struct ioapic_reg_2 {
  unsigned int 
        reserved2 : 24,
      arbitration : 4,
        reserved1 : 4;
};


/* IO APIC redirection table entry - pg 11 of IOAPIC spec */

#define IOAPIC_INTR_VECTOR(irq) (IRQ_OFFSET+(irq))

struct ioapic_rtbl_entry {
  unsigned int
  	       vector : 8,
	delivery_mode : 3,  /* 000 fixed, 001 lowest priority, 111 ExtINI */
	    dest_mode : 1,  /* 0 physical, 1 logical */
      delivery_status : 1,
             polarity : 1,
	          irr : 1,
	      trigger : 1,  /* 0 edge, 1 level */
	     itr_mask : 1,  /* 0 enabled, 1 disabled */
	     reserved : 15;
  union {
    struct {
      unsigned int
            reserved1 : 24,
        physical_dest : 4,
	    reserved2 : 4;
    } physical;
    
    struct {
      unsigned int
            reserved1 : 24,
	 logical_dest : 8;
    } logical;
  } dest;
};


/* constants related to the redirection table entry */

/* delivery mode - pg 13 of IOAPIC spec */
#define DEL_MODE_FIXED  0
#define DEL_MODE_LOWPRI 1
#define DEL_MODE_SMI    2
#define DEL_MODE_RESRV1 3
#define DEL_MODE_NMI    4
#define DEL_MODE_INIT   5
#define DEL_MODE_RESRV2 6
#define DEL_MODE_EXTINT 7

/* designation mode - pg 12 of IOAPIC spec */
#define DEST_MODE_PHY   0
#define DEST_MODE_LOG   1

/* designations */
#define DEST_PHY_ALL	15
#define DEST_LOG_ALL	0xff

/* polarity - pg 12 of IOAPIC spec */
#define POLARITY_ACTIVE_HIGH 0
#define POLARITY_ACTIVE_LOW  1

/* trigger mode - pg 12 of IOAPIC spec */
#define TRIGGER_LEVEL 1
#define TRIGGER_EDGE  0

/* constants related to interrupt types -- MP spec v1.4, pg4-15 */
#define INTR_INT    0
#define INTR_NMI    1
#define INTR_SMI    2
#define INTR_ExtINT 3


extern int ioapic_irq_entries_count;

/* IOAPIC read, write, sync routines, see IOAPIC spec */

extern __inline unsigned int io_apic_read (unsigned int reg)
{
  *((volatile unsigned int *)IOAPIC_REGSEL) = reg;
  return *(volatile unsigned int*)(IOAPIC_IOWIN);
}

extern __inline void io_apic_write (unsigned int reg, unsigned int value)
{
  *((volatile unsigned int *)IOAPIC_REGSEL) = reg;
  *((volatile unsigned int *)(IOAPIC_IOWIN)) = value;
}

extern __inline void io_apic_sync(void)
  /* sync CPU and IOAPIC by doing a read - stolen from linux SMP 
   * not sure how this works and why */
{
  *((volatile unsigned int *)(IOAPIC_IOWIN));
}


extern inline void
localapic_ACK () /* signal EOI to local APIC to allow further interrupts */
{
  apic_read (APIC_SPIV);
  apic_write (APIC_EOI, 0);
}


/* routines */

extern void localapic_enable();
extern void localapic_disable();
extern void ioapic_init();

extern int lapic_ipi_gen(int dest_type, int vector, int delivery_mode);
extern int lapic_ipi_cpu(int cpu, int vector);

extern void ioapic_mask_irq (int irq);
extern void ioapic_unmask_irq (int irq);


#include <xok/picirq.h>	    /* MAX_IRQS definition */
/* these are mappings of IRQs to/from IOAPIC pins, extracted 
 * from the MP configuration table (see smp.c) */
extern struct mpc_config_intsrc ioapic_irq_entries[MAX_IRQS];


#endif  /* __SMP__ */

#endif  /* KERNEL */
#endif  /* __ASSEMBLER__ */

#endif  /* __APIC_H__ */

