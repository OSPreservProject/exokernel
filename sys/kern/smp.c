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
 * in particularly appendix B - benjie
 */

#ifdef __SMP__


#include <xok/defs.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/cpu.h>
#include <xok/smp.h>
#include <xok/trap.h>
#include <xok/locore.h>
#include <xok/kclock.h>
#include <xok/printf.h>

/*
 * SMP hardware states - the following data structures
 * are used during the runtime of the operating system 
 */

int apic_version[NR_CPUS];	/* APIC version number */
volatile int smp_cpu_count = 0;	/* total count of live CPU's */
volatile int smp_mode = 0;	/* smp mode toggle */
volatile int smp_boot_cpu_id = 0;	/* boot cpu ID */

volatile int smp_commenced = 0;	  /* barrier: everyone up */
volatile unsigned long smp_start_initialize = 0;  /* barrier: start init */

/*
 * SMP specific states - mainly for initialisation of APs 
 */

volatile int cpu_callin_map = 0;	/* APs signal they are up */
volatile int cpu_doneinit_map = 0;	/* APs signal inits are done */

/*
 * AP Bootstraping 
 */

unsigned long ap_boot_addr_base = 0;	/* base addr of trampoline code */
unsigned long *p0pdir_smp = 0;	/* address for AP's pdir */
volatile unsigned long p0cr3_smp = 0;	/* address for AP's pdir */


/*
 * local static functions
 */

static int smp_scan_config (unsigned long, unsigned long);
static void smp_boot_APs ();
static void smp_apic_test ();
static void smp_map_apic ();


/*
 * CPU layout information:
 *
 * CPU are refered to by both the APIC ID, or the cpu count. That is, a
 * CPU with APIC ID being 4 can be the only AP in the system, therefore
 * its id, by count, is 1. From now on, we refer to id as APIC id, and
 * count as id by count. 
 *
 * Outside of smp.c and apic.c, all other kernel code uses id by count.
 * Thus whenever you see cpu_id, it is id by count.
 *
 * cpu_count2id and cpu_id2count keep mappings between APIC id and id by
 * count. Along with cpu_present_map_by_id and cpu_present_map_by_count, 
 * these four have accurate information on what CPUs are UP and RUNNING. 
 */ 

int cpu_count2id[NR_CPUS];	/* maps cpu count to the apic_id */
int cpu_id2count[NR_CPUS];	/* reverse mapping */
static unsigned long cpu_present_map_by_id = 0;	
static unsigned long cpu_present_map_by_count = 0;


/*
 * bus type: keep types of bus by bus id
 */
#define NR_BUSES 64
static char *bus_type[NR_BUSES];


/**********************************
 * globally exported SMP routines *
 **********************************/



void 
smp_scan ()
  /* 
   * initialize SMP by detecting if we are in SMP mode:
   * scan for smp support at three different addresses
   */
{
  smp_mode = 0;
  smp_scan_config (0x0, 0x400);
  if (!smp_mode)
    smp_scan_config (639 * 0x400, 0x400);
  if (!smp_mode)
    smp_scan_config (0xF0000, 0x10000);
  smp_map_apic ();
}




void 
smp_boot ()
  /* 
   * start SMP AP processors
   * if no smp mode found, do nothing and return
   */
{
  /* during smp scan, we obtained the ID of a boot processor (BSP),
   * which at this point is the only processor up and running.
   * the following code, executed by the BSP, will send STARTUP
   * interrupts to the rest of the application processors (APs).
   */
  if (!smp_mode)
    return;
  smp_boot_APs ();
}




void 
smp_callin ()
  /* AP reports to BSP that it is up */
{
  int cpuid = GET_APIC_ID (apic_read (APIC_ID));

  /* tell BSP that we are powered up */
  cpu_callin_map |= (1 << cpuid);
}


void
smp_init_done()
  /* AP reports to BSP that it is done with initialization */
{
  cpu_doneinit_map |= (1 << cpu_id);
}


void 
smp_commence ()
  /* tell APs to start their schedulers and going operational */
{
  if (smp_mode) {
    smp_commenced = 1;
  }
}


void
smp_waitfor_commence()
{
  while((volatile int)smp_commenced == 0)
    asm volatile("" ::: "memory");
  return;
}


/*******************
 * static routines *
 *******************/


static void 
smp_apic_test ()
  /*
   * test smp AP's APIC mapping
   */
{
  if (smp_mode) {
    int reg;

    /*
     *  This is to verify that we're looking at
     *  a real local APIC.  Check these against
     *  your board if the CPUs aren't getting
     *  started for no apparent reason.
     */

    printf ("The following two values should be the same, and nonzero\n");
    reg = apic_read (APIC_VERSION);
    printf ("Getting VERSION: %x\n", reg);

    apic_write (APIC_VERSION, 0);
    reg = apic_read (APIC_VERSION);
    printf ("Getting VERSION: %x\n", reg);

    if (reg == 0) {
      printf ("SMP initialization failed\n");
      return;
    }

    /*
     *  The two version reads above should print the same
     *  NON-ZERO!!! numbers.  If the second one is zero,
     *  there is a problem with the APIC write/read
     *  definitions.
     *
     *  The next two are just to see if we have sane values.
     *  They're only really relevant if we're in Virtual Wire
     *  compatibility mode, but most boxes are anymore.
     */

    reg = apic_read (APIC_LVT0);
    printf ("Getting LVT0: %x\n", reg);

    reg = apic_read (APIC_LVT1);
    printf ("Getting LVT1: %x\n", reg);
  }
}



static void 
smp_map_apic ()
  /*
   * memory map the local APIC and IO APIC
   */
{
  if (smp_mode) {
    int pfaddr, j;
    int lim;
    int curpdn;
    Pte *lapic_upt;
    Pte *ioapic_upt;

    /* For SMP, map local APIC to kernel space at va LAPIC.
     * since we already have a page directory, we need to 
     * manually modify the PTEs here */

    lim = PTENO (LAPIC + (2 << 19));	/* 1 meg */
    curpdn = PDENO (LAPIC);
    lapic_upt = (Pte *) ptov ((p0pdir_boot[curpdn] >> PGSHIFT) << PGSHIFT);
    pfaddr = local_apic_addr >> PGSHIFT;
    for (j = PTENO (LAPIC); j < lim; j++) {
      lapic_upt[j] = (pfaddr << PGSHIFT) | PG_P | PG_W;
      pfaddr++;
    }

    /* same thing must be done for IOAPIC */
    lim = PTENO (IOAPIC + (2 << 19));	/* 1 meg */
    curpdn = PDENO (IOAPIC);
    ioapic_upt = (Pte *) ptov ((p0pdir_boot[curpdn] >> PGSHIFT) << PGSHIFT);
    pfaddr = io_apic_addr >> PGSHIFT;
    for (j = PTENO (IOAPIC); j < lim; j++) {
      ioapic_upt[j] = (pfaddr << PGSHIFT) | PG_P | PG_W;
      pfaddr++;
    }

  }
  else {
    smp_cpu_count = 1;
  }
}



static int 
mpf_compute_checksum (unsigned char *mp, int len)
  /* compute checksum of a MP configuration block */
{
  int sum = 0;
  while (len--)
    sum += *mp++;
  return sum & 0xFF;
}



static char *
mpc_family (int family, int model)
  /* return processor encoding in a MP configuration block */
{
  static char n[32];
  static char *model_defs[] =
  {
    "80486DX", "80486DX", "80486SX", "80486DX/2 or 80487",
    "80486SL", "Intel5X2(tm)", "Unknown", "Unknown", "80486DX/4"
  };
  if (family == 0x6)
    return ("Pentium(tm) Pro");
  if (family == 0x5)
    return ("Pentium(tm)");
  if (family == 0x0F && model == 0x0F)
    return ("Special controller");
  if (family == 0x04 && model < 9)
    return model_defs[model];
  sprintf (n, "Unknown CPU [%d:%d]", family, model);
  return n;
}




static int 
smp_read_mpc (struct mp_config_table *mpc)
  /* 
   * read the MP configuration table.
   * returns 1 if reading of mp table failed,
   * else return the number of processors.
   */
{
  char str[16];
  int count = sizeof (*mpc);
  int apics = 0;
  unsigned char *mpt = ((unsigned char *) mpc) + count;
  int i;

  for(i=0; i<NR_BUSES; i++)
    bus_type[i] = 0L;


  /*
   * check to make sure our table is not corrupted
   */

  if (memcmp (mpc->signature, MPC_SIGNATURE, 4) != 0) {
    printf ("Bad signature [%c%c%c%c].\n", mpc->signature[0],
	    mpc->signature[1], mpc->signature[2], mpc->signature[3]);
    return 1;
  }

  if (mpf_compute_checksum ((unsigned char *) mpc, mpc->length)) {
    printf ("Checksum error.\n");
    return 1;
  }

  if (mpc->spec != 0x01 && mpc->spec != 0x04) {
    printf ("Bad Config Table version (%d)!!\n", mpc->spec);
    return 1;
  }


  /*
   * some hardware details
   */

  memcpy (str, mpc->oem, 8);
  str[8] = 0;
  // memcpy (ioapic_oem_id, str, 9);
  printf ("OEM ID: %s ", str);

  memcpy (str, mpc->productid, 12);
  str[12] = 0;
  // memcpy (ioapic_prod_id, str, 13);
  printf ("Product ID: %s ", str);
  printf ("Local APIC at: 0x%lx\n", mpc->lapic);

  /* set the local APIC address */
  local_apic_addr = mpc->lapic;

  /*
   * now process the configuration blocks, traversing
   * each entity
   */

  while (count < mpc->length) {
    switch (*mpt) {

      /* go and find each processor that's available to us */
    case MP_PROCESSOR:{
	struct mpc_config_processor *m = (struct mpc_config_processor *) mpt;

	if (m->cpuflag & CPU_ENABLED) {

	  printf ("Processor #%d %s APIC version %d",
		  m->apicid,
		  mpc_family ((m->cpufeature & CPU_FAMILY_MASK) >> 8,
			      (m->cpufeature & CPU_MODEL_MASK) >> 4),
		  m->apicver);
	  if (m->cpuflag & CPU_BOOTPROCESSOR) {
	    printf (" - BSP");
	    smp_boot_cpu_id = m->apicid;
	    cpu_id2count[smp_boot_cpu_id] = 0;
	    cpu_count2id[0] = smp_boot_cpu_id;
	    cpu_present_map_by_count = 1; // BSP is always 1 by count
	  }
	  else {		/* Boot CPU already counted */
	    printf (" - AP");
	  }

	  if (m->apicid > NR_CPUS)
	    printf ("Processor #%d unused. (Max %d processors).\n",
		    m->apicid, NR_CPUS);
	  else {
	    printf ("\n");
	    cpu_present_map_by_id |= (1 << m->apicid);
	    apic_version[m->apicid] = m->apicver;
	    smp_cpu_count++;
	  }

	}			/* CPU_ENABLED */

	mpt += sizeof (*m);
	count += sizeof (*m);
	break;
      }				/* case processor */

    case MP_BUS:{
	struct mpc_config_bus *m = (struct mpc_config_bus *) mpt;
	char str[7];
	mpt += sizeof (*m);
	count += sizeof (*m);
	memcpy (str, m->bustype, 6);
	str[6] = '\0';
	if (strncmp(str, BUSTYPE_ISA, strlen(BUSTYPE_ISA))==0)
	  bus_type[m->busid] = BUSTYPE_ISA;
	else if (strncmp(str, BUSTYPE_PCI, strlen(BUSTYPE_PCI))==0)
	  bus_type[m->busid] = BUSTYPE_PCI;
	else if (strncmp(str, BUSTYPE_EISA, strlen(BUSTYPE_EISA))==0)
	  bus_type[m->busid] = BUSTYPE_EISA;
	else if (strncmp(str, BUSTYPE_INTERN, strlen(BUSTYPE_INTERN))==0)
	  bus_type[m->busid] = BUSTYPE_INTERN;
	else if (strncmp(str, BUSTYPE_PCMCIA, strlen(BUSTYPE_PCMCIA))==0)
	  bus_type[m->busid] = BUSTYPE_PCMCIA;
	else if (strncmp(str, BUSTYPE_VL, strlen(BUSTYPE_VL))==0)
	  bus_type[m->busid] = BUSTYPE_VL;
	else if (strncmp(str, BUSTYPE_MCA, strlen(BUSTYPE_MCA))==0)
	  bus_type[m->busid] = BUSTYPE_MCA;
	// printf ("BUS %d is %s %s\n", m->busid, str, bus_type[m->busid]);
	break;
      }				/* case bus */

    case MP_IOAPIC:{
	struct mpc_config_ioapic *m = (struct mpc_config_ioapic *) mpt;
	if (m->flags & MPC_APIC_USABLE) {
	  apics++;
	  printf ("I/O APIC #%d Version %d at 0x%lx\n",
		  m->apicid, m->apicver,
		  m->apicaddr);
	  io_apic_addr = m->apicaddr;
	}
	mpt += sizeof (*m);
	count += sizeof (*m);
	break;
      }				/* case ioapic */

    case MP_INTSRC:{
	struct mpc_config_intsrc *m = (struct mpc_config_intsrc *) mpt;
	mpt += sizeof (*m);
	count += sizeof (*m);
	
	/*
	   printf("INTERRUPT SRC: type %d, flag 0x%x\n",m->irqtype,m->irqflag);
	   printf("source bus ID %d, IRQ %d, IOAPIC ID %d, pin %d\n",
	   m->srcbus, m->srcbusirq, m->dstapic, m->dstirq);
	 */
      
	if (m->dstirq < MAX_IRQS && m->srcbusirq < MAX_IRQS) 
	{
	  ioapic_irq_entries[ioapic_irq_entries_count] = *m;
	  ioapic_irq_entries_count++;
	}
	break;
      }				/* case intsrc */

    case MP_LINTSRC:{
	struct mpc_config_intlocal *m = (struct mpc_config_intlocal *) mpt;
	mpt += sizeof (*m);
	count += sizeof (*m);
	break;
      }				/* case lintsrc */

    }				/* switch */
  }				/* while */

  return smp_cpu_count;
}




static int 
smp_scan_config (unsigned long base, unsigned long length)
  /* 
   * scans for smp configuration table. returns 0 if scan failed 
   * or found a structure but the table is corrupted, else return 1
   * and set smp_mode to 1.
   */
{
  unsigned long *bp = (unsigned long *) ptov (base);
  struct mp_floating *mpf;
  int i;

  while (length > 0) {
    if (*bp == SMP_MAGIC_IDENT) {
      mpf = (struct mp_floating *) bp;

      /* these values are given in Intel MP spec */
      if (mpf->length == 1 &&
	  !mpf_compute_checksum ((unsigned char *) bp, 16) &&
	  (mpf->specification == 1 || mpf->specification == 4)) {

	printf ("Intel MultiProcessor Specification v1.%d\n",
		mpf->specification);
	if (mpf->feature2 & (1 << 7))
	  printf ("    IMCR and PIC compatibility mode.\n");
	else
	  printf ("    Virtual Wire compatibility mode.\n");

	if (mpf->feature1 != 0) {
	  printf ("Some default configuration is detected,");
	  printf (" but I know nothing about it, ignored.\n");
	  return 0;
	}

	for (i = 0; i < NR_CPUS; i++)
	  cpu_count2id[i] = -1;
	for (i = 0; i < NR_CPUS; i++)
	  cpu_id2count[i] = -1;

	if (mpf->physptr) {
	  if (smp_read_mpc ((void *) ptov (mpf->physptr)) == 1)
	    return 0;
	  /* reading of the mp table failed... we don't attempt to
	   * try to discover mp mode anyother way right now... so
	   * proceed as uniprocessor */
	}

	printf ("Total processors found: %d\n", smp_cpu_count);
	smp_mode = 1;
	return 1;

      }				/* found a valid mpf */
    }				/* found SMP identification */

    bp += 4;
    length -= 16;

  }				/* keep on searching */

  return 0;
}



static void 
smp_install_trampoline (void *trampoline_ptr)
  /* install the trampoline code at trampoline_ptr.
   * trampoline_ptr should be below 1 meg according to 
   * MP spec. the trampoline code is used to bootstrap
   * APs.
   */
{
  unsigned long smp_tramp_lgdt_arg_pos = 0;
  unsigned long smp_tramp_lgdt_arg_val = 0;
  unsigned long smp_tramp_gdt_arg_pos = 0;
  unsigned long smp_tramp_gdt_arg_val = 0;
  unsigned long smp_tramp_gdt_filltramp_pos = 0;
  u_int8_t *dst8;
  u_int16_t *dst16;
  unsigned long tramp_size = 0;

  tramp_size = smp_trampoline_end - smp_trampoline_start;

  /* copy the smp trampoline code to somewhere below 640K */
  /* APs can only execute somewhere below 1meg, see pmap.c for
   * additional comments */
  memcpy (ptov (trampoline_ptr), smp_trampoline_start, tramp_size);

  /* copy new positions into the code we just moved */
  smp_tramp_gdt_arg_pos =
    ((unsigned long) trampoline_ptr) +
    (smp_tramp_gdt_48_base_addr - smp_trampoline_start);
  smp_tramp_gdt_arg_val =
    ((unsigned long) trampoline_ptr) +
    (smp_tramp_gdt_base_addr - smp_trampoline_start);

  smp_tramp_lgdt_arg_pos =
    ((unsigned long) trampoline_ptr) +
    (smp_tramp_lgdt_arg - smp_trampoline_start);
  smp_tramp_lgdt_arg_val =
    smp_tramp_gdt_48 - smp_trampoline_start;

  /* modify the gdt arg in the gdt table */
  *((volatile unsigned long *) ptov (smp_tramp_gdt_arg_pos)) =
    (unsigned long) smp_tramp_gdt_arg_val;

  /* modify the lgdt arg in lgdt instruction */
  *((volatile unsigned long *) ptov (smp_tramp_lgdt_arg_pos)) =
    (unsigned long) smp_tramp_lgdt_arg_val;

  /* writing the following two is trickier because we need to
   * write a 16 byte and then a 8 byte for the whole thing */

  /* modify the target for boot data segment */
  smp_tramp_gdt_filltramp_pos =
    ((unsigned long) trampoline_ptr) +
    (smp_tramp_boot_data_seg - smp_trampoline_start);
  dst16 = (u_int16_t *) ptov (smp_tramp_gdt_filltramp_pos);
  dst8 = (u_int8_t *) (dst16 + 1);
  *dst16 = (u_int) trampoline_ptr & 0xffff;
  *dst8 = ((u_int) trampoline_ptr >> 16) & 0xff;

  /* modify the target for boot code segment */
  smp_tramp_gdt_filltramp_pos =
    ((unsigned long) trampoline_ptr) +
    (smp_tramp_boot_code - smp_trampoline_start);
  dst16 = (u_int16_t *) ptov (smp_tramp_gdt_filltramp_pos);
  dst8 = (u_int8_t *) (dst16 + 1);
  *dst16 = (u_int) trampoline_ptr & 0xffff;
  *dst8 = ((u_int) trampoline_ptr >> 16) & 0xff;
}



static void 
smp_boot_APs (void)
  /*
   * cycle through the processors sending pentium IPI's to boot each.
   * according to the MP spec: all application processors at the start
   * of the OS are in halted condition with interrupts disable, and 
   * they also respond to INIT or STARTUP interprocessor interrupts.
   * in this routine, we attempt to get them up an running.
   *
   * MP spec v1.4, appendix B.4
   */
{
  unsigned long cfg;
  unsigned long oldbrvoff = 0;
  unsigned long oldbrvseg = 0;
  unsigned long oldshtreg = 0;
  unsigned long signal_addr = 0xf809fffc;
  int i, j;
  int cpucount = 0;
  void *trampoline_ptr = (void *) ap_boot_addr_base;

  if (!smp_mode)
    return;

  /* turn on local APIC */
  localapic_enable ();

  /* make a page directory that we can use */
  p0cr3_smp = ap_boot_addr_base - NBPG;
  p0pdir_smp = ptov (p0cr3_smp);

  for (j = 0; j < NLPG; j++) 
    p0pdir_smp[j] = p0pdir[j];
  
  smp_install_trampoline (trampoline_ptr);

  /* map the first 4 meg to VA 0, this allows each
   * AP to access its own text =) */
  p0pdir_smp[0] = p0pdir_smp[PDENO (KERNBASE)];
  
  {
    unsigned long pa = 0;
    Pte *p0_upt = (Pte *) ptov ((p0pdir_smp[0] >> PGSHIFT) << PGSHIFT);

    pa = 0 >> PGSHIFT;
    for (j = 0; j < NLPG; j++) {
      p0_upt[j] = (pa << PGSHIFT) | PG_P | PG_W;
      pa++;
    }
  }

  /* now scan the cpu present map and fire up the other CPUs */
  for (i = 0; i < NR_CPUS; i++) {

    /* ignore boot cpu, already up */
    if (i == smp_boot_cpu_id)
      continue;

    if (cpu_present_map_by_id & (1 << i)) {
      unsigned long send_status, accept_status;
      int num_starts;
      int timeout;

      /** temporarily set these **/
      cpucount++;
      cpu_id2count[i] = cpucount;
      cpu_count2id[cpucount] = i;
      cpu_present_map_by_count |= (1 << cpucount);

#if 0
      printf ("   CPU %d (apic id), ", i);
      printf ("stack @ 0x%lx:\n", (unsigned long) trampoline_ptr);
#endif

      *((volatile unsigned long *) signal_addr) = 0x00000000;

      /* the following block set us up to send APs booting interrupts
       * for detail refer to MP spec v1.4, B-3,B-4.
       * we need to do the following:
       * 1. set warm reset code in CMS RAM shutdown location
       * 2. set warm reset vector to point to AP startup code 
       */

      /* now set jump to startup code... */
      oldshtreg = mc146818_read (0L, CMOS_RAM_SHUTDOWN_REG);
      oldbrvoff = *((volatile unsigned short *) pa2kva (BIOS_RESET_VECTOR_OFF));
      oldbrvseg = *((volatile unsigned short *) pa2kva (BIOS_RESET_VECTOR_SEG));

      mc146818_write (0L, CMOS_RAM_SHUTDOWN_REG, CMOS_WARM_RESET);

      /* vector is 40:67, which is 0x467 */
      /* BIOS_RESET_VECTOR_OFF is 0x467 
       * BIOS_RESET_VECTOR_SEG is 0x469  
       * we right shift by 4 to produce the seg of the stack addr
       * and write that to BIOS_RESET_VECTOR_SEG 
       */
      *((volatile unsigned short *) pa2kva (BIOS_RESET_VECTOR_OFF)) = 0;
      *((volatile unsigned short *) pa2kva (BIOS_RESET_VECTOR_SEG)) =
	((unsigned long) trampoline_ptr) >> 4;


      /* be paranoid about clearing APIC errors */

      if (apic_version[i] & 0xF0) {
	apic_write (APIC_ESR, 0);
	accept_status = (apic_read (APIC_ESR) & 0xEF);
      }

      /* status is now clean */

      send_status = 0;
      accept_status = 0;

      /* starting actual IPI sequence... */

      /* asserting INIT, turn INIT on */

      cfg = apic_read (APIC_ICR2);
      cfg &= 0x00FFFFFF;
      apic_write (APIC_ICR2, cfg | SET_APIC_DEST_FIELD (i));	/* target chip */
      cfg = apic_read (APIC_ICR);
      cfg &= ~0xCDFFF;		/* clear bits */
      cfg |= (APIC_DEST_FIELD | APIC_DEST_LEVELTRIG
	      | APIC_DEST_ASSERT | APIC_DM_INIT);
      apic_write (APIC_ICR, cfg);	/* send IPI */

      delay (200);

      /* deasserting INIT */

      cfg = apic_read (APIC_ICR2);
      cfg &= 0x00FFFFFF;
      apic_write (APIC_ICR2, cfg | SET_APIC_DEST_FIELD (i));	/* target chip */
      cfg = apic_read (APIC_ICR);
      cfg &= ~0xCDFFF;		/* clear bits */
      cfg |= (APIC_DEST_FIELD | APIC_DEST_LEVELTRIG
	      | APIC_DM_INIT);
      apic_write (APIC_ICR, cfg);	/* send IPI */

      /*
       * Should we send STARTUP IPIs ?
       *
       * Determine this based on the APIC version.
       * If we don't have an integrated APIC, don't
       * send the STARTUP IPIs.
       */

      if (apic_version[i] & 0xF0)
	num_starts = 2;
      else
	num_starts = 0;

      /* run STARTUP IPI loop - see MP spec v1.4 B4 for algorithm */

      for (j = 1; !(send_status || accept_status)
	   && (j <= num_starts); j++) {

	apic_write (APIC_ESR, 0);

	/* startup IPI */

	cfg = apic_read (APIC_ICR2);
	cfg &= 0x00FFFFFF;
	apic_write (APIC_ICR2, cfg | SET_APIC_DEST_FIELD (i));
	cfg = apic_read (APIC_ICR);
	cfg &= ~0xCDFFF;
	cfg |= (APIC_DEST_FIELD | APIC_DM_STARTUP
		| (((int) trampoline_ptr) >> 12));
	apic_write (APIC_ICR, cfg);

	timeout = 0;
	do {
	  delay (10);
	} while ((send_status = (apic_read (APIC_ICR) & 0x1000))
		 && (timeout++ < 1000));
	delay (200);

	accept_status = (apic_read (APIC_ESR) & 0xEF);
      }

      if (send_status || accept_status) {
	if (send_status)		/* APIC never delivered?? */
	  printf ("APIC never delivered???\n");
	if (accept_status)	/* Send accept error */
	  printf ("APIC delivery error (%lx).\n", accept_status);

	cpu_id2count[i] = -1;
	cpu_count2id[cpucount] = -1;
	cpu_present_map_by_count &= ~(1 << cpucount);
	cpucount--;
      }

      else {
	for (timeout = 0; timeout < 1000000; timeout++) {
	  if (cpu_callin_map & (1 << i))
	    break;		/* it has booted */
	  delay (100);		/* Wait 10s total for a response */
	}

	if (cpu_callin_map & (1 << i)) {

	  /* now we wait til its done initializing...
	   *
	   * XXX - we really should have a timeout mechanism here
	   */
	  while ((cpu_doneinit_map & (1 << cpucount))==0);

	  /* number CPUs logically, starting from 1 (BSP is 0) */
	  //printf ("Processor %d: initialization complete\n", cpucount);
	}

	else {
	  if (*((volatile unsigned char *) signal_addr) != 0xE5) { 
	    int ii;
	    unsigned long reg;
	    int base = 0x34;

	    printf ("signal=0x%lx, ", 
		    (unsigned long)*((volatile unsigned long *) signal_addr));
	    printf ("not responding to IPIs or failed in assembly code\n");
	    for (ii = 0; ii < 6; ii++) {
	      reg = mc146818_read (0L, base + ii);
	      printf ("0x%lx\n", reg);
	    }
	  }
	  else
	    printf ("failed to finish initialisation sequence\n");

	  cpu_id2count[i] = -1;
	  cpu_count2id[cpucount] = -1;
	  cpu_present_map_by_count &= ~(1 << cpucount);
	  cpucount--;
	}
      }
    }

    /* Make sure we unmap all failed CPUs */
    if (cpu_id2count[i] == -1) {
      cpu_present_map_by_id &= ~(1 << i);
    }
  }

  /* paranoid: set warm reset code and vector here back
   * to default values */

  mc146818_write (0L, CMOS_RAM_SHUTDOWN_REG, oldshtreg);
  *((volatile long *) pa2kva (BIOS_RESET_VECTOR_SEG)) = oldbrvseg;
  *((volatile long *) pa2kva (BIOS_RESET_VECTOR_OFF)) = oldbrvoff;


  if (cpucount == 0) {
    printf ("SMP: only one processor found - back to uniprocessor mode.\n");
    cpu_present_map_by_id = (1 << smp_boot_cpu_id);
    smp_mode = 0;
  }
  else {
    smp_cpu_count = cpucount + 1;
    printf ("SMP: %d processors up and running.\n", smp_cpu_count);
  }

  localapic_ACK ();
  delay (500000);
}


int pci_bus(int busid)
{
  if (bus_type[busid] != 0L)
  {
    // printf("bus_type is %s\n", bus_type[busid]);
    if (strcmp(bus_type[busid], BUSTYPE_PCI)==0)
      return 1;
  }
  return 0;
}


int isa_bus(int busid)
{
  if (bus_type[busid] != 0L)
  {
    // printf("bus_type is %s\n", bus_type[busid]);
    if (strcmp(bus_type[busid], BUSTYPE_ISA)==0 ||
        strcmp(bus_type[busid], BUSTYPE_EISA)==0)
    return 1;
  }
  return 0;
}





#endif /* __SMP__ */
