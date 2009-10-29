
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

#ifndef _SMP_H_
#define _SMP_H_
#ifdef __SMP__

/*
 * SMP module: stuff related to SMP booting and operation
 * 
 * Reader should refer to Intel SMP spec v1.4 and volume 3
 * of the PPro Family Developer's Manual (OS writers manual).
 */

#include <machine/param.h>
#include <xok/cpu.h>
#include <xok/apic.h>  /* SMP operation may need APIC */
#include <xok/capability.h>
#include <xok/queue.h>
#include <xok/kqueue.h>
#include <xok/env.h>


/* CMOS locations related to booting */
/* locations */
#define CMOS_RAM_SHUTDOWN_REG  0xf
/* control sequences */
#define CMOS_WARM_RESET        0xa
/* bios reset vector */
#define BIOS_RESET_VECTOR_OFF      0x467
#define BIOS_RESET_VECTOR_SEG      0x469


/* trampoline code - smptramp.S */
void smp_trampoline_start            __P((void));
void smp_trampoline_end              __P((void));
void smp_tramp_gdt_48                __P((void));
void smp_tramp_gdt_48_base_addr      __P((void));
void smp_tramp_gdt_base_addr         __P((void));
void smp_tramp_lgdt_arg              __P((void));
void smp_tramp_boot_data_seg         __P((void));
void smp_tramp_boot_code	     __P((void));



/* MP configuration: ID, table, entries */

/* ID */
#define SMP_MAGIC_IDENT (('_'<<24)|('P'<<16)|('M'<<8)|'_')

/* floating table */
struct mp_floating
{
  char signature[4];          /* "_MP_"                       */
  unsigned long physptr;      /* Configuration table address  */
  unsigned char length;       /* Our length (paragraphs)      */
  unsigned char specification;/* Specification version        */
  unsigned char checksum;     /* Checksum (makes sum 0)       */
  unsigned char feature1;     /* Standard or configuration ?  */
  unsigned char feature2;     /* Bit7 set for IMCR|PIC        */
  unsigned char feature3;     /* Unused (0)                   */
  unsigned char feature4;     /* Unused (0)                   */
  unsigned char feature5;     /* Unused (0)                   */
};

/* configuration table */
struct mp_config_table
{
  char signature[4];
#define MPC_SIGNATURE "PCMP"
  unsigned short length;      /* Size of table */
  char  spec;                 /* 0x01 */
  char  checksum;
  char  oem[8];
  char  productid[12];
  unsigned long oemptr;       /* 0 if not present */
  unsigned short oemsize;     /* 0 if not present */
  unsigned short oemcount;
  unsigned long lapic;        /* APIC address */
  unsigned long reserved;
};

#define	MP_PROCESSOR	0
#define	MP_BUS		1
#define	MP_IOAPIC	2
#define	MP_INTSRC	3
#define	MP_LINTSRC	4

/* processor configuration */
struct mpc_config_processor
{
  unsigned char type;
  unsigned char apicid;	/* Local APIC number */
  unsigned char apicver;	/* Its versions */
  unsigned char cpuflag;
#define CPU_ENABLED		1	/* Processor is available */
#define CPU_BOOTPROCESSOR	2	/* Processor is the BP */
  unsigned long cpufeature;		
#define CPU_STEPPING_MASK 0x0F
#define CPU_MODEL_MASK	0xF0
#define CPU_FAMILY_MASK	0xF00
  unsigned long featureflag;	/* CPUID feature value */
  unsigned long reserved[2];
};

/* bus configuration */
struct mpc_config_bus
{
  unsigned char type;
  unsigned char busid;
  unsigned char bustype[6] __attribute((packed));
};

#define BUSTYPE_EISA	"EISA"
#define BUSTYPE_ISA	"ISA"
#define BUSTYPE_INTERN	"INTERN"	/* Internal BUS */
#define BUSTYPE_MCA	"MCA"
#define BUSTYPE_VL	"VL"		/* Local bus */
#define BUSTYPE_PCI	"PCI"
#define BUSTYPE_PCMCIA	"PCMCIA"

/* IOAPIC configuration */
struct mpc_config_ioapic
{
  unsigned char type;
  unsigned char apicid;
  unsigned char apicver;
  unsigned char flags;
#define MPC_APIC_USABLE		0x01
  unsigned long apicaddr;
};

/* interrupt source configuration (w IOAPIC) */
struct mpc_config_intsrc
{
  unsigned char type;
  unsigned char irqtype;
  unsigned short irqflag;
  unsigned char srcbus;
  unsigned char srcbusirq;
  unsigned char dstapic;
  unsigned char dstirq;
};

#define MP_INT_VECTORED		0
#define MP_INT_NMI		1
#define MP_INT_SMI		2
#define MP_INT_EXTINT		3

#define MP_IRQDIR_DEFAULT	0
#define MP_IRQDIR_HIGH		1
#define MP_IRQDIR_LOW		3

/* locally connected interrupts */
struct mpc_config_intlocal
{
  unsigned char type;
  unsigned char irqtype;
  unsigned short irqflag;
  unsigned char srcbusid;
  unsigned char srcbusirq;
  unsigned char destapic;	
#define MP_APIC_ALL	0xFF
  unsigned char destapiclint;
};



/* data sturctures and routines */

extern int smp_cpu_count; 			/* number of cpus alive */
extern int smp_mode;				/* smp mode togle */
extern int smp_boot_cpu_id;			/* id of BSP */
extern volatile int smp_commenced; 		/* simple barrier */
extern unsigned long ap_boot_addr_base;		/* base addr of tramp code */

extern void smp_scan(void);  		/* looks for SMP */
extern void smp_boot(void);		/* boot APs */
extern void smp_callin(void);		/* AP reporting in that it's up */
extern void smp_init_done(void);	/* AP reporting in that init is done */
extern void smp_commence(void);	        /* BSP let APs run sched */
extern void smp_waitfor_commence(void);	/* APs waiting for commencement */

#endif /* __SMP__ */

#endif /* _SMP_H_ */
