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


/* NOTE: readers of this code should also read Intel MP Spec v1.4, 
 * and the Intel 82093 I/O APIC specification on IO APIC 
 */



#ifdef __SMP__

#include <xok/defs.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/mplock.h>
#include <xok/apic.h>
#include <xok/smp.h>	// for the MP interrupt assignments from MP table
#include <xok/trap.h>
#include <xok/printf.h>
#include <xok/locore.h>
#include <xok/sysinfo.h>
#include <dev/isa/isareg.h>
#include <machine/pio.h>



/*********************************************
 * IRQ/Interrupts
 * IRQ/Interrupt assignments for IOAPIC pins
 * can be obtained from the MP configuration 
 * table. In smp.c we read that table and
 * save all pin/irq assignment information
 * in the array ioapic_irq_entries.
 *********************************************/


u16 irq_mask_IOAPIC;

struct mpc_config_intsrc ioapic_irq_entries[MAX_IRQS];
int ioapic_irq_entries_count = 0;
int irq2pin[NR_IOAPIC_IRQS];
int irq2trigger[NR_IOAPIC_IRQS];

volatile unsigned long io_apic_addr = IOAPIC_ADDR_DEFAULT;	/* IO apic */
volatile unsigned long local_apic_addr = LAPIC_ADDR_DEFAULT;	/* local apic */


void debug_lapic (void);
void debug_ioapic (void);
static void ioapic_symio_init (void);
static void ioapic_setup_irqs (void);


#define RDTSC(x) \
  asm volatile \
    ("rdtsc" :"=a" (((unsigned long*)&x)[0]), "=d" (((unsigned long*)&x)[1]))


static inline unsigned int get_8254_timer_cnt()
{
  unsigned int count;
  outb(0x00, 0x43);
  count = inb(0x40);
  count |= inb(0x40) << 8;
  return count;
}

static inline void zero_8254()
{
  int diff;
  unsigned int ccnt, pcnt;
  pcnt = 0;
  ccnt = get_8254_timer_cnt();
  do {
    pcnt = ccnt;
    ccnt = get_8254_timer_cnt();
    diff = ccnt-pcnt;
  } while (diff<300);
}

void
localapic_disable ()
  /* disable local APIC */
{
  unsigned long cfg;
  cfg = apic_read(APIC_SPIV);
  cfg &= ~(1 << 8);		/* soft enable APIC */
  apic_write (APIC_SPIV, cfg);
}

void
localapic_enable ()
  /* enable local APIC */
{
  unsigned long cfg;
  long apic_t1,apic_t2, bus_speed;
  int num_bus_calib_loops = 100;
  int i;

  cfg = apic_read (APIC_SPIV);
  cfg |= (1 << 8);		/* soft enable APIC */
  cfg |= 0xff;			/* set spurious IRQ vector to 0xff */
  apic_write (APIC_SPIV, cfg);
  delay (10);

  cfg = apic_read (APIC_TASKPRI);
  cfg &= ~APIC_TPRI_MASK;	/* zero TPR */
  apic_write (APIC_TASKPRI, cfg);
  delay (10);

  localapic_ACK ();
  delay (10);

  apic_read(APIC_LVTT);
  cfg = APIC_LVT_TIMER_PERIODIC | IRQ_OFFSET+0; 
  apic_write(APIC_LVTT, cfg);

  /* set up the apic timer to have frequency at 16th of bus clock */
  cfg = apic_read(APIC_TDCR);
  cfg = (cfg & ~APIC_TDR_DIV_1) | APIC_TDR_DIV_16;
  apic_write(APIC_TDCR, cfg);

  /* tmp set this to be huge so we can calibrate bus */
  apic_read(APIC_TMICT);
  apic_write(APIC_TMICT, 1000000000/16);

  /* get the bus speed */
  zero_8254();
  apic_t1=apic_read(APIC_TMCCT);
  // each time 8254 zeros, that's 1 SI_HZ
  for (i=0; i<num_bus_calib_loops; i++) zero_8254();  
  apic_t2=apic_read(APIC_TMCCT);

  /* number of bus clock ticks per 8254 timer counter wraparound */
  bus_speed = (apic_t1-apic_t2)*16/num_bus_calib_loops;
  /* each 8254 timer counter wraparound takes 1/SI_HZ */
  bus_speed *= SI_HZ;
  printf ("  CPU mhz %d, ", SYSINFO_GET(si_mhz));
  printf ("calibrating APIC timer at bus speed %ldMhz\n",bus_speed/1000000);

  /* APIC timer counter decrements at 16th of bus frequency, and to be
   * consistent with uniprocessor mode, we want SI_HZ ticks per second, so we
   * divide bus speed by SI_HZ then by 16, and use that number as the timer
   * initial counter. When timer counter decrements to 0, it is reset back to
   * the initial counter and a timer interrupt is generated at IRQ 0's
   * interrupt vector.
   */

  apic_read(APIC_TMICT);
  apic_write(APIC_TMICT, (bus_speed/SI_HZ)/16);

  if (!smp_commenced) // BP
  {
    // unmask the external timer - we don't need it anymore 
    irq_setmask_8259A (irq_mask_8259A | (1 << 0));
    printf("  Timer interrupt transfered to APIC #%ld\n", 
	   GET_APIC_ID(apic_read(APIC_ID)));
  }

  localapic_ACK ();
  delay (10);
}


void
ioapic_init ()
  /* initialize I/O APIC */
{
  /* initialize to Symmetric IO mode */
  irq_mask_IOAPIC = 0xffff;
  ioapic_symio_init ();
  ioapic_setup_irqs ();

#if 0
  debug_ioapic ();
  debug_lapic ();
#endif
}


static inline int
ioapic_irq_2_pin (int irq)
  /* find pin associated with the irq */
{
  if (irq > NR_IOAPIC_IRQS - 1)
    return -1;
  return irq2pin[irq];
}


static inline int
ioapic_pin_2_irq (int pin, int type, int *idx)
  /* find the irq from a pin and a type */
{
  int i;
  for (i = 0; i < ioapic_irq_entries_count; i++) {
    struct mpc_config_intsrc m = ioapic_irq_entries[i];
    if (m.dstirq == pin && m.irqtype == type) 
    {
      *idx = i;
      return m.srcbusirq;
    }
  }
  return -1;
}


extern int pci_bus(int);
extern int isa_bus(int);

static inline int
ioapic_get_pin_trigger(int idx)
{
  struct mpc_config_intsrc m = ioapic_irq_entries[idx];
  unsigned char fl = (m.irqflag & 12)>>2;

  switch(fl)
  {
    case 0:
      /* need to look at the type of the bus */
      if (pci_bus(m.srcbus))
	return TRIGGER_LEVEL;
      else if (isa_bus(m.srcbus))
	return TRIGGER_EDGE;
      break;
    case 1:
      return TRIGGER_EDGE;
    case 2:
      break;
    case 3:
      return TRIGGER_LEVEL;
  }
  assert(0);
  return 0;
}

static inline int
ioapic_get_pin_polarity(int idx)
{
  struct mpc_config_intsrc m = ioapic_irq_entries[idx];
  unsigned char fl = m.irqflag & 3;

  switch(fl)
  {
    case 0:
      /* need to look at the type of the bus */
      if (pci_bus(m.srcbus))
	return POLARITY_ACTIVE_LOW;
      else if (isa_bus(m.srcbus))
	return POLARITY_ACTIVE_HIGH;
      break;
    case 1:
      return POLARITY_ACTIVE_HIGH;
    case 2:
      break;
    case 3:
      return POLARITY_ACTIVE_LOW;
  }
  assert(0);
  return 0;
}


void
debug_lapic ()
  /* dump local APIC information */
{
  long cfg;

  cfg = apic_read (APIC_EOI);
  printf ("   eoi register reads 0x%lx\n", cfg);
  delay (10);

  cfg = apic_read (APIC_ISR);
  printf ("   isr1 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 1 * 32);
  printf ("   isr2 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 2 * 32);
  printf ("   isr3 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 3 * 32);
  printf ("   isr4 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 4 * 32);
  printf ("   isr5 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 5 * 32);
  printf ("   isr6 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 6 * 32);
  printf ("   isr7 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_ISR + 7 * 32);
  printf ("   isr8 register reads 0x%lx\n", cfg);
  delay (10);

  cfg = apic_read (APIC_TMR);
  printf ("   tmr1 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 1 * 32);
  printf ("   tmr2 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 2 * 32);
  printf ("   tmr3 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 3 * 32);
  printf ("   tmr4 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 4 * 32);
  printf ("   tmr5 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 5 * 32);
  printf ("   tmr6 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 6 * 32);
  printf ("   tmr7 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_TMR + 7 * 32);
  printf ("   tmr8 register reads 0x%lx\n", cfg);
  delay (10);

  cfg = apic_read (APIC_IRR);
  printf ("   irr1 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 1 * 32);
  printf ("   irr2 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 2 * 32);
  printf ("   irr3 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 3 * 32);
  printf ("   irr4 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 4 * 32);
  printf ("   irr5 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 5 * 32);
  printf ("   irr6 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 6 * 32);
  printf ("   irr7 register reads 0x%lx\n", cfg);
  delay (10);
  cfg = apic_read (APIC_IRR + 7 * 32);
  printf ("   irr8 register reads 0x%lx\n", cfg);
  delay (10);

  cfg = apic_read (APIC_TASKPRI);
  printf ("   tpr register reads 0x%lx\n", cfg);
  delay (10);

  cfg = apic_read (APIC_PROCPRI);
  printf ("   ppr register reads 0x%lx\n", cfg);
  delay (10);
}



void
debug_ioapic ()
  /* dump IOAPIC information */
{
  int i;
  struct ioapic_reg_0 reg0;
  struct ioapic_reg_1 reg1;
  struct ioapic_reg_2 reg2;

  *((int *) &reg0) = io_apic_read (0);
  *((int *) &reg1) = io_apic_read (1);
  *((int *) &reg2) = io_apic_read (2);

  printf ("********** INTEL I/O APIC ***************\n");
  printf ("0x%x, 0x%x\n", irq_mask_IOAPIC, irq_mask_8259A);
  printf ("registers:\n");
  printf ("1: 0x%08lx, 2: 0x%08lx, 3: 0x%08lx\n",
	  (unsigned long) *((int *) &reg0),
	  (unsigned long) *((int *) &reg1),
	  (unsigned long) *((int *) &reg2));
  printf ("\n");
  for (i = 0; i < reg1.entries; i++) {	/* reg1.entries = nr of rtbl entries */
    struct ioapic_rtbl_entry entry;

    *(((int *) &entry) + 0) = io_apic_read (0x10 + i * 2);
    *(((int *) &entry) + 1) = io_apic_read (0x11 + i * 2);

    printf ("rtbl entry: %02x, %03x %02x\n", i,
	    entry.dest.logical.logical_dest,
	    entry.dest.physical.physical_dest);

    printf ("  %1d  %1d  %1d  %1d  %1d  %1d  %1d  %02d\n",
	    entry.itr_mask, entry.trigger, entry.irr,
	    entry.polarity, entry.delivery_status,
	    entry.dest_mode, entry.delivery_mode,
	    entry.vector);
  }
  printf ("*****************************************\n");
}



static inline int
ioapic_get_irq_entry (int irq, struct ioapic_rtbl_entry *entry)
  /* obtain current redirection table entry associated with irq.
   * requires that entry be a non-null, already allocated struct.
   * returns pin number.
   */
{
  int pin = ioapic_irq_2_pin (irq);
  if (pin != -1) {
    *(((int *) entry) + 0) = io_apic_read (IOAPIC_RTBL_W1 + pin * 2);
    *(((int *) entry) + 1) = io_apic_read (IOAPIC_RTBL_W2 + pin * 2);
  }
  return pin;
}



static inline void
ioapic_put_irq_entry (int irq, struct ioapic_rtbl_entry *entry)
  /* write the entry into the redirection table entry associated 
   * with irq 
   */
{
  int pin = ioapic_irq_2_pin (irq);
  if (pin != -1) {
    io_apic_write (IOAPIC_RTBL_W1 + 2 * pin, *(((int *) entry) + 0));
    io_apic_write (IOAPIC_RTBL_W2 + 2 * pin, *(((int *) entry) + 1));
    io_apic_sync ();
  }
}



void
ioapic_mask_irq (int irq)
  /* mask the pin associated with the irq */
{
  struct ioapic_rtbl_entry entry;

  MP_SPINLOCK_GET(GLOCK(PICIRQ_LOCK));
  if (ioapic_get_irq_entry (irq, &entry) != -1) 
  {
    entry.itr_mask = 1;
    ioapic_put_irq_entry (irq, &entry);
    irq_mrkmask_ioapic (irq_mask_IOAPIC | (1 << irq));
  }
  MP_SPINLOCK_RELEASE(GLOCK(PICIRQ_LOCK));

}



void
ioapic_unmask_irq (int irq)
  /* unmask the pin associated with the irq */
{
  struct ioapic_rtbl_entry entry;
 
  MP_SPINLOCK_GET(GLOCK(PICIRQ_LOCK));
  if (ioapic_get_irq_entry (irq, &entry) != -1) 
  {
    entry.itr_mask = 0;
    ioapic_put_irq_entry (irq, &entry);
    irq_mrkmask_ioapic (irq_mask_IOAPIC & ~(1 << irq));
  }
  MP_SPINLOCK_RELEASE(GLOCK(PICIRQ_LOCK));

}



static inline void
ioapic_change_irq_dest (int irq, int dest_mode, int dest)
  /* changes the destination of irq in ioapic */
{
  struct ioapic_rtbl_entry entry;
  if (ioapic_get_irq_entry (irq, &entry) != -1) {
    if (dest_mode == DEST_MODE_PHY)
      entry.dest.physical.physical_dest = dest;
    else if (dest_mode == DEST_MODE_LOG)
      entry.dest.logical.logical_dest = dest;
    ioapic_put_irq_entry (irq, &entry);
  }
}


static inline int
ioapic_steal_irq_control_from_8259 (int irq)
  /* take irq off the regular interrupt controler and
   * unmask the irq on the IOAPIC */
{
  if (irq > -1 && irq < NR_IOAPIC_IRQS) {
    int pin = ioapic_irq_2_pin (irq);
    if (pin != -1) {		// only enable if ioapic has it
      /* want first to unmask it on the IOAPIC 
       * before we turn off on 8259A 
       */
      ioapic_unmask_irq (irq);
      irq_setmask_8259A (irq_mask_8259A | (1 << irq));
      return 1;
    }
  }
  return 0;
}


static inline void
ioapic_zero_pin (int pin)
  /* clear all IOAPIC pins by zeroing redirection
   * table entry to 0 and turning off interrupt */
{
  struct ioapic_rtbl_entry entry;
  memset (&entry, 0, sizeof (struct ioapic_rtbl_entry));
  entry.itr_mask = 1;		/* disable interrupt for now */
  io_apic_write (IOAPIC_RTBL_W1 + 2 * pin, *(((int *) &entry) + 0));
  io_apic_write (IOAPIC_RTBL_W2 + 2 * pin, *(((int *) &entry) + 1));
  io_apic_sync ();
}


static void
ioapic_setup_irqs ()
  /* setup all irqs ioapic can handle */
{
  int i;
  int pin, irq, idx;
  struct ioapic_rtbl_entry entry;
  int extint = 0;

  /* build irq_2_pin table */
  for (i = 0; i < NR_IOAPIC_IRQS; i++)
  {
    irq2pin[i] = -1;
    irq2trigger[i] = -1;
  }

  for (pin = 0; pin < NR_IOAPIC_IRQS; pin++) {

    /* see if there is an irq associated with this pin, 
     * may be INT, or ExtINT 
     */
    extint = 0;
    idx = -1;
    irq = ioapic_pin_2_irq (pin, INTR_INT, &idx);
    if (irq == -1) {
      irq = ioapic_pin_2_irq (pin, INTR_ExtINT, &idx);
      if (irq != -1)
	extint = 1;
    }

    /* there is, set up the pin */
    if (irq != -1) {
      if (irq < NR_IOAPIC_IRQS)
	irq2pin[irq] = pin;
      else
	panic ("PIN->IRQ returned a mapping with IRQ out of range");

      /* for each irq that we have, enable it,
       * but first leave it masked for now */

      memset (&entry, 0, sizeof (struct ioapic_rtbl_entry));

      /* BLAH */
      assert(idx >= 0);
      entry.trigger = ioapic_get_pin_trigger(idx);
      irq2trigger[irq] = entry.trigger;
      entry.polarity = ioapic_get_pin_polarity(idx);

      entry.vector = IOAPIC_INTR_VECTOR (irq);
      entry.itr_mask = 1;

      if (extint == 1) {	/* ExtINT */
	entry.dest_mode = DEST_MODE_PHY;
	entry.dest.physical.physical_dest = 1; // DEST_PHY_ALL;
	entry.delivery_mode = DEL_MODE_EXTINT;
      }
      else {
	entry.dest_mode = DEST_MODE_PHY;
	entry.dest.physical.physical_dest = DEST_PHY_ALL;
	entry.delivery_mode = DEL_MODE_LOWPRI;
      }

      if (extint == 0) {
	ioapic_put_irq_entry (irq, &entry);

	/* if an INT irq is controled by the XT controller,
	 * take it off and put it on IOAPIC control
	 */
	if (!(irq_mask_8259A & (1 << irq))) {
	  ioapic_steal_irq_control_from_8259 (irq);
	}

      }
      else {
	/* for an ExtINT we just need to unmask it, but
	 * leave the 8259A unmasked because it will need
	 * to have come from there anyways...
	 */
	entry.itr_mask = 0;
	ioapic_put_irq_entry (irq, &entry);
      }
    }
  }
}



static void
ioapic_symio_init ()
  /* switch IO APIC from PIC/VirtualWire mode to SymIO mode */
{
  struct ioapic_reg_1 reg1;
  int pin_nr, i;

  outb (IO_RTC, 0x22);
  outb (0x01, 0x23);

  printf ("Symmetric IO mode enabled\n");

  /* zero each pin to start with.
   * the number of IOAPIC irq-registers 
   * is also the num of pins */
  *((int *) &reg1) = io_apic_read (1);
  pin_nr = reg1.entries;
  for (i = 0; i < pin_nr; i++)
    ioapic_zero_pin (i);
}



/*
 * send an IPI vector to dest_type via delivery_mode.
 *
 * destType is of: APIC_DEST_SELF, APIC_DEST_ALLINC, APIC_DEST_ALLBUT
 * vector is any valid interrupt vector
 * delivery_mode is of: APIC_DM_FIXED, APIC_DM_LOWPRIO
 */
int
lapic_ipi_gen(int dest_type, int vector, int delivery_mode)
{
  unsigned long cfg;

  asm volatile("cli" ::: "memory");
  cfg = apic_read(APIC_ICR) & ~0xFFF00000;
  cfg = cfg  | dest_type | delivery_mode | vector;
  apic_write(APIC_ICR, cfg);

  /* wait til IPI is no longer pending */
  while (cfg = apic_read(APIC_ICR), cfg & APIC_DELSTAT_MASK);
  return 0;
}


/* send an IPI vector to a CPU via delivery_mode.
 * 
 * cpu is logical: 0, 1, 2, etc. 
 * vector is any valid interrupt vector
 */
int
lapic_ipi_cpu(int cpu, int vector)
{
  unsigned long cfg;

  asm volatile("cli" ::: "memory");
  cfg = apic_read(APIC_ICR2) & 0x00FFFFFF;
  apic_write (APIC_ICR2, cfg | SET_APIC_DEST_FIELD(cpu_count2id[cpu]));

  cfg = apic_read(APIC_ICR) & ~0xFDFFF;
  cfg = cfg | APIC_DEST_FIELD | APIC_DM_FIXED | vector;
  apic_write(APIC_ICR, cfg);

  /* wait til IPI is no longer pending */
  while (cfg = apic_read(APIC_ICR), cfg & APIC_DELSTAT_MASK);
  return 0;
}


#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

#endif /* __SMP__ */
