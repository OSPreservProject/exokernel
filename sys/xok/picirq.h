
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


#ifndef _XOK_PICIRQ_H_
#define _XOK_PICIRQ_H_

/* this may be a bit conservative: 
 * 16 for traditional interrupts, and
 * then 8 additional ones for APICs */
#define MAX_IRQS        16

/* I/O Addresses of the two 8259A programmable interrupt controllers */
#define IO_PIC1 0x20     /* Master (IRQs 0-7) */
#define IO_PIC2 0xa0     /* Slave (IRQs 8-15) */
#define IRQ_SLAVE 0x2    /* IRQ at which slave connects to master */
#define IRQ_OFFSET 0x20  /* IRQ 0 corresponds to int IRQ_OFFSET */

#define IRQ_CLK 0	/* CLK irq is ExtINT */
#define IRQ_RTC 8	/* RTC irq is a bit tricky */

#ifndef __ASSEMBLER__

#ifdef KERNEL

#include <xok/types.h>
#include <xok/xoktypes.h>

#include <machine/pio.h>
#include <xok/kerrno.h>

#ifdef __SMP__
#include <xok/apic.h>
#endif

extern u16 irq_mask_8259A;
void pic_init (void);

void random_intr (int irq);
extern unsigned int *irqhandlers[];
/* Set the interrupt handler for an irq */
static inline int
irq_sethandler (u_int irq, int (*handler)(u_int))
{
  if (irq >= 0x10) return -E_INVAL;

  if (*irqhandlers[irq] != ((unsigned char *)random_intr
			    - (unsigned char *)irqhandlers[irq] - 4))
    return -E_EXISTS;

  *irqhandlers[irq] = ((unsigned char *) handler
		       - (unsigned char *)irqhandlers[irq] - 4);
  return 0;
}

#ifdef __SMP__

extern u16 irq_mask_IOAPIC;

/* Set the interrupt mask for IOAPIC, but just mark it since IOAPIC
 * don't work with masks */
static inline void
irq_mrkmask_ioapic (u16 mask)
{
  irq_mask_IOAPIC = mask;
}

#endif

/* Set the interrupt mask in the 8259A PICs */
static inline void
irq_setmask_8259A (u16 mask)
{
  irq_mask_8259A = mask;
  outb(IO_PIC1+1, (char)mask);
  outb(IO_PIC2+1, (char)(mask >> 8));
}

/* A byte of the form 0x6n written to the first byte of a PIC's
 * address space goes to the 8259A's "OCW2" register and designates a
 * specific EOI for IRQ n.
 */
static inline void
irq_eoi_8259A (u_int irqnum)
{
  if (irqnum < 0x10) {                     /* Ignore invalid arg */
    if (irqnum >= 8) {                     /* Interrupt from slave PIC */
      outb(IO_PIC2, 0x60 | (irqnum - 8));  /* First clear slave PIC */
      outb(IO_PIC1, 0x60 | IRQ_SLAVE);     /* Then clear master */
    } else
      outb(IO_PIC1, 0x60 | irqnum);        /* Interrupt from master PIC */
  }
}

#ifdef __SMP__

/* Send EOI command to IOAPIC by writing in the EOI 
 * register of local APIC
 */
static inline void
irq_eoi_apic ()
{
  apic_write(APIC_EOI,0);
  apic_read(APIC_EOI);   // in between successive writes must be a read
}

#endif /* __SMP__ */

extern void irq_eoi (int irqnum);
extern void irq_finished (int irqnum);


#endif /* KERNEL */

#endif /* !__ASSEMBLER__ */

#endif /* _XOK_PICIRQ_H_ */
