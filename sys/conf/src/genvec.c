
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


/*
 * This file represents attempt number 5 at generating all the
 * trap/interrupt vectors in a vaguely pleasant way.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "linebuf.h"


#define MAX_VEC 0x100


/* EXOPC defs */
#include "../../xok/mmu.h"     /* LAPIC */
#include "../../xok/picirq.h"  /* IRQ defs */
#include "../../xok/trap.h"      /* IPITRAP_OFFSET */
#include "../../xok/cpu.h"      /* APIC_EOI, CPUID_OFFSET, etc */
#include "../../xok/sys_syms.h"  /* address for si_intr etc */
#ifndef NR_CPUS
#include "../../machine/param.h"
#endif


char *progname;

static int nerrs;
static Linebuf *lb;
const char * infile = "trap.conf";
const char * const cfile = "idt.c";
const char * const sfile = "vector.s";

void fatal (const char *, ...)
     __attribute__ ((noreturn, format (printf, 1, 2)));
void syntax (const char *, ...) __attribute__ ((format (printf, 1, 2)));

struct vector_info {
  unsigned int vi_type;
#define VT_VALID 0x1         /* "i" or "t" - valid gate */
#define VT_TGATE 0x2         /* "t" - trap gate */
#define VT_ECODE 0x4         /* "e" - trap pushes error code on stack */
#define VT_FINT 0x8          /* "f" - fast interrupt handler */
#define VT_USER 0x10         /* "u" - DPL 3 in gate */
#define VT_NOSAV 0x20        /* "s" - don't save caller-saved regs */
#define VT_EBX 0x40          /* "b" - pass %ebx as second argument */
#define VT_DIRECT 0x80       /* "N" - put pointer directly in idt.c */
#define VT_PUSHAL 0x100      /* "A" - push all registers on trap frame */
#define VT_ASH 0x200         /* "U" - DPL 2 in gate */
#define VT_UTEXT 0x400       /* "T" - user text segment descriptor */
#define VT_IMSK (VT_FINT)
  /* Mutually exclusive modes: */
#define VT_MODE (VT_NOSAV|VT_EBX|VT_DIRECT|VT_PUSHAL)
  char *vi_handler;          /* name of handler function to call */
};

void
fatal (const char *msg, ...)
{
  va_list ap;

  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  unlink (cfile);
  unlink (sfile);
  exit (1);
}

void
syntax (const char *msg, ...)
{
  va_list ap;

  fprintf (stderr, "%s:%d: ", infile, lb->lineno);
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  nerrs++;
}

static int
buildvi (struct vector_info *vi)
{
  char *l;

  nerrs = 0;
  while (l = getline (lb)) {
    unsigned int vn;
    char *cp;
    if (cp = strchr (l, '#'))      /* get rid of comments */
      *cp = '\0';

    /* Get trap number */
    cp = strtok (l, " \t");
    if (!cp || !*cp)               /* skip blank/comment lines */
      continue;
    vn = strtol (cp, &cp, 0);
    if (cp == l || *cp) {          /* no number or garbage left over */
      syntax ("invalid trap number `%s'\n", l);
      continue;
    }
    if (vn >= 0x100) {
      syntax ("trap number 0x%x is out of range\n", vn);
      continue;
    }
    if (vi[vn].vi_type) {
      syntax ("can't redefine trap 0x%x\n", vn);
    }

    /* Get trap type */
    cp = strtok (NULL, " \t");
    if (!cp || !*cp) {
      syntax ("missing vector type for trap 0x%x\n", vn);
      continue;
    }
    switch (*cp) {
    case 't':
      vi[vn].vi_type = VT_VALID | VT_TGATE;
      break;
    case 'i':
      vi[vn].vi_type = VT_VALID;
      break;
    default:
      syntax ("vector 0x%x type `%s' doesn't start `t' or `i'\n", vn, cp);
      goto nextline;
    }
    while (*++cp)
      switch (*cp) {
      case 'e':
	if (vi[vn].vi_type & (VT_NOSAV | VT_IMSK))
	  syntax ("vector 0x%x has more than one of e f and s options\n",
		  vn);
	vi[vn].vi_type |= VT_ECODE;
	break;
      case 'f':
	if (vi[vn].vi_type & (VT_NOSAV | VT_ECODE))
	  syntax ("vector 0x%x has more than one of e f and s options\n",
		  vn);
	vi[vn].vi_type |= VT_FINT;
	break;
      case 'u':
	vi[vn].vi_type |= VT_USER;
	break;
      case 'U':
	vi[vn].vi_type |= VT_ASH;
	break;
      case 's':
	if (vi[vn].vi_type & (VT_IMSK | VT_ECODE | VT_DIRECT | VT_ECODE))
	  syntax ("vector 0x%x has more than one of e f and s options\n",
		  vn);
	vi[vn].vi_type |= VT_NOSAV;
	break;
      case 'b':
	if (! (vi[vn].vi_type & VT_NOSAV))
	  syntax ("vector 0x%x must specify s before b in type\n", vn);
	vi[vn].vi_type |= VT_EBX;
	break;
      case 'N':
	if (vi[vn].vi_type & VT_MODE)
	  syntax ("vector 0x%x `N' combined with incompatible options", vn);
	vi[vn].vi_type |= VT_DIRECT;
	break;
      case 'A':
	if (vi[vn].vi_type & VT_MODE)
	  syntax ("vector 0x%x `A' combined with incompatible options", vn);
	vi[vn].vi_type |= VT_PUSHAL;
	break;
      case 'T':
	vi[vn].vi_type |= VT_UTEXT;
	break;
      default:
	syntax ("vector 0x%x contains invalid type character `%c'\n",
		vn, *cp);
	goto nextline;
      }

    if ((vi[vn].vi_type & VT_DIRECT)
	&& (vi[vn].vi_type & (VT_EBX|VT_NOSAV|VT_ECODE|VT_FINT|VT_PUSHAL))) {
      syntax ("vector 0x%x contains `N' with `e', `s', `b', or `f'\n", vn);
      continue;
    }
    if (vi[vn].vi_type & (VT_USER|VT_ASH) && vn<0x20 && vn!=1 && vn!=3) {
      syntax ("exception vector 0x%x contains a non-zero DPL\n", vn);
    }
    if ((vi[vn].vi_type & (VT_USER|VT_ASH)) == (VT_USER|VT_ASH)) {
      syntax ("vector 0x%x contains both `u' and `U' flags\n", vn);
    }

    /* Get function name */
    cp = strtok (NULL, " \t");
    if (!cp || !*cp) {
      syntax ("missing handler name for trap 0x%x\n", vn);
      continue;
    }
    vi[vn].vi_handler = strdup (cp);
    if (vi[vn].vi_handler[0] == '$' && !(vi[vn].vi_type & VT_DIRECT))
      syntax ("using fixed location requires N option, for trap 0x%x\n", vn);

    /* Chastise user for trailing garbage */
    cp = strtok (NULL, " \t");
    if (cp && *cp)
      syntax ("trailing garbage after trap 0x%x\n", vn);

  nextline:
  }
  return (nerrs);
}

/* Increment IRQ count */
static void
irq_incr_count (FILE *fp, int irqnum)
{
  fprintf (fp, "# increment interrupt count for IRQ %d\n", irqnum);
  fprintf (fp, 
           "\tmovl  $0x%x,%%eax\n"
	   "\timull $%d,%%eax\n"
#if 1
	   "\taddl  0x%lx,%%eax\n"
#else
	   "\txorl  %%ecx,%%ecx\n"
	   "\tstr   %%cx\n"
	   "\taddl  $-%d,%%ecx\n"
	   "\tshrl  $3,%%ecx\n"
	   "\taddl  %%ecx,%%eax\n"
#endif
	   "\tmovl  _si,%%ecx\n"
	   "\taddl  $1,%ld(%%ecx,%%eax,8)\n"
           "\tadcl  $0,%ld(%%ecx,%%eax,8)\n",
	   irqnum, NR_CPUS, 
#if 1
	   (unsigned long) __cpu_id_H_ADDR,
#else
	   GD_TSS, 
#endif
	   (unsigned long)__si_intr_stats_H_ADDR - SYSINFO,
	   (unsigned long)__si_intr_stats_H_ADDR - SYSINFO+4);
}

/* A byte of the form 0x6n written to the first byte of an PIC's
 * address space goes to the 8259A's "OCW2" register and designates a
 * specific EOI for IRQ n.
 */
static void
signal_irq_eoi (FILE *fp, int irqnum)
{
  fprintf (fp, "# 8259 EOI for IRQ %d\n", irqnum);
  if (irqnum >= 8)                     /* Interrupt from slave PIC */
    fprintf (fp,
	     "\tmovb $0x%x,%%eax\n"
	     "\toutb %%al,$0x%x\n"   /* First clear slave PIC */
	     "\tmovb $0x%x,%%eax\n"
	     "\toutb %%al,$0x%x\n",   /* Then clear master */
	     0x60 | (irqnum - 8), IO_PIC2,
	     0x60 | IRQ_SLAVE, IO_PIC1);
  else
    fprintf (fp,
	     "\tmovb $0x%x,%%eax\n"
	     "\toutb %%al,$0x%x\n",
	     0x60 | irqnum, IO_PIC1);
#ifdef __SMP__
#if 0
  /* in smp mode, also send EOI to IOAPIC */
  fprintf (fp, "# IOAPIC EOI for IRQ %d\n", irqnum);
  fprintf (fp,
           "\tmovl $0,%%eax\n"
	   "\tmovl %%eax,0x%x\n",LAPIC+APIC_EOI);
#else
  fprintf (fp, "# IOAPIC Level/Edge EOI for IRQ %d\n", irqnum);
  fprintf (fp, "\tpushl $%d\n",irqnum);
  fprintf (fp, "\tcall _irq_eoi\n");
  fprintf (fp, "\tpopl  %%eax\n");
#endif
#endif
}

static void
signal_irq_finished (FILE *fp, int irqnum)
{
#ifdef __SMP__
  fprintf (fp, "# unmask IRQ if it is level triggered\n");
  fprintf (fp, "\tpushl $%d\n",irqnum);
  fprintf (fp, "\tcall _irq_finished\n");
  fprintf (fp, "\tpopl  %%eax\n");
#endif
}


static void
signal_apic_eoi (FILE *fp)
{
  fprintf (fp, "# APIC EOI for IPI Trap\n");
  fprintf (fp,
	     "\tmovl (0x%x),%%eax\n"
	     "\tmovl $0,(0x%x)\n", APIC_SPIV_ADDR, APIC_EOI_ADDR);
}




#if 0
/* Set the interrupt mask in the 8259A PICs */
static void
irq_setmask (FILE *fp, int mask)
{
  fprintf (fp,
	   "# Set interrupt mask to 0x%04x\n"
	   "\tmovl $0x%x,%%eax\n"
	   "\toutb %%al,$0x%x\n"
	   "\tmovl $0x%x,%%eax\n"
	   "\toutb %%al,$0x%x\n",
	   mask & 0xffff, mask & 0xff, IO_PIC1, (mask >> 8) & 0xff,
	   IO_PIC2);
}
#endif

/* Generate code to save registers.  Certain types of traps cause an
 * error code to be pushed onto the stack after the old stack and PC
 * information (see figure 14-4 on p. 14-11 of the pentium book).
 * This error code must be poped off the stack before the iret
 * instruction can be handled.  Therefore, we pop it off the top of
 * the stack here and push it back on the bottom so that it can be
 * used as the second argument to whatever trap handler gets called.
 */
static void
trap_setup (FILE *fp, int type)
{
#ifdef __HOST__
    if (! (type & VT_TGATE) && (type & VT_USER) || (type & VT_ASH)) {
      /* interrupt with DPL of 2 or 3 */
      fprintf (fp, "\tcmpl $0x%x, -4(%%esp)\n"
	       "\tjl _fault_trampoline\n", GD_NULLS*8);
    }
#endif

  if (! (type & VT_NOSAV)) {
    /* First save registers */
    if (! (type & VT_ECODE))
      fprintf (fp, "# Trap setup without error code%s\n"
	       "\tpushl %%ds\n",
	       (type & VT_PUSHAL) ? " (pushal version)" : "");
    else
      fprintf (fp, "# Trap setup with error code%s\n",
	       (type & VT_PUSHAL) ? " (pushal version)" : "");
    fprintf (fp, "\tpushl %%es\n");
    fprintf (fp, (type & VT_PUSHAL) ?
	     "\tpushal\n" :
	     "\tpushl %%eax\n"
	     "\tpushl %%ecx\n"
	     "\tpushl %%edx\n");
    /* Push the error code on the stack, and replace it with the
     * saved value of %ds */
    if (type & VT_ECODE)
      fprintf (fp,
	       "\tmovl 0x%x(%%esp),%%eax\n"
	       "\tpushl %%eax\n"
	       "\tmovl %%ds,%%ax\n"
	       "\tmovl %%eax,0x%x(%%esp)\n",
	       (type & VT_PUSHAL) ? 0x24 : 0x10,
	       (type & VT_PUSHAL) ? 0x28 : 0x14);
  }
  else
    fprintf (fp, "# Trap setup without register saves%s\n"
	     "\tpushl %%ds\n"
	     "\tpushl %%es\n",
	     (type & VT_EBX) ? " (%%ebx holds arg)" : "");
  /* Now set up segment registers to have kernel privs. */
  if (type & VT_NOSAV)
    fprintf (fp,
	     "\tpushl $0x%x\n"
	     "\tpopl %%ds\n"
	     "\tpushl $0x%x\n"
	     "\tpopl %%es\n",
	     GD_KD, GD_KD);
  else
    fprintf (fp,
	     "\tmovl $0x%x,%%eax\n"
	     "\tmovl %%ax,%%ds\n"
	     "\tmovl %%ax,%%es\n",
	     GD_KD);
}

static void
trap_finish (FILE *fp, int type)
{
  if (type & VT_NOSAV)
    fprintf (fp, "# Trap finish without restore\n");
  else {
    fprintf (fp,
	     "# Trap finish\n"
	     "\tpopl %%edx\n"
	     "\tpopl %%ecx\n"
	     "\tpopl %%eax\n");
  }
  fprintf (fp,
	   "\tpopl %%es\n"
	   "\tpopl %%ds\n"
	   "\tiret\n\n");
}

int
main (int argc, char **argv)
{
  struct vector_info vi[MAX_VEC] = {};
  int i;
  FILE *cf, *sf;
  char hname[80];
  char segdesc[6];

  if (progname = strrchr (argv[0], '/'))
    progname++;
  else
    progname = argv[0];
  if (argc == 2)
    infile = argv[1];
  else if (argc > 2)
    fatal ("%s: garbage arguments\n", progname);

  unlink (cfile);
  unlink (sfile);

  lb = Linebuf_alloc (infile, fatal);
  if (buildvi (vi))
    fatal ("%s: %d parse errors\n", progname, nerrs);

  if (! (cf = fopen (cfile, "w")))
    fatal ("%s: %s\n", cfile, strerror (errno));
  if (! (sf = fopen (sfile, "w")))
    fatal ("%s: %s\n", sfile, strerror (errno));

  fprintf (sf, "\
# Interrupt and trap vectors.  This file was created automatically\n\
# by %s from the %s input file.\n\
#\n\
# DO NOT EDIT THIS FILE BY HAND.\n\
\n\
#include <xok/sys_syms.h>\n\
\n\
.text\n\
DBLC1:\n\
.ascii \"value DBLC1 0x%%x\\12\\0\"\n\
.align 2\n\
\t.text\n",
	   progname, infile);
  fprintf (cf, "\
/*\n\
 * IDT initialization code.  This file was created automatically\n\
 * by %s from the %s input file.\n\
 *\n\
 * DO NOT EDIT THIS FILE BY HAND.\n\
 */\n\
\n\
#include <xok/trap.h>\n\
#include <xok/picirq.h>\n\
#include <xok/idt.h>\n\
\n\
extern struct gate_desc idt[];\n\
unsigned int *irqhandlers[%d];\n\
\n",
	   progname, infile, MAX_IRQS);

  for (i = 0; i < 0x100; i++)
    if (vi[i].vi_type) {
      if (vi[i].vi_handler[0] == '$') /* jump directly to fixed location */
	continue;
      if (vi[i].vi_type & VT_DIRECT)
	fprintf (cf, "extern void %s (void);\n", vi[i].vi_handler);
      else
	fprintf (cf, "extern void V%02x (void);\n", i);
    }

  fprintf (cf, "\n");

  for (i=IRQ_OFFSET+0; i < IRQ_OFFSET+MAX_IRQS; i++) {
     fprintf (cf, "extern unsigned int Iproc%x;\n", i);
  }

  fprintf (cf, "\
\n\
void\n\
idt_init (void)\n\
{\n");/*"*/

  for (i = 0; i < 0x100; i++) {
    int tt = vi[i].vi_type;
    if (! (tt & VT_VALID))
      continue;

    if (tt & VT_UTEXT)
      strcpy (segdesc, "GD_UT");
    else
      strcpy (segdesc, "GD_KT");

    if (tt & VT_DIRECT)
      {
	if (vi[i].vi_handler[0] == '$')
	  sprintf (hname, "((void(*)(void))%s)", &vi[i].vi_handler[1]);
	else
	  sprintf (hname, "%s", vi[i].vi_handler);
      }
    else
      sprintf (hname, "V%02x", i);

    if (tt & VT_TGATE)
      fprintf (cf, "  setgate (idt[%d], 1, %s, %s, %d);\n", i, segdesc, hname,
	       (vi[i].vi_type & VT_USER) ? 3 :
	       (vi[i].vi_type & VT_ASH) ? 2 : 0);
    else {
      if (tt & VT_IMSK)
	fprintf (cf, "  setgate (idt[IRQ_OFFSET+%d], 0, %s, %s, %d);\n",
		 i - IRQ_OFFSET, segdesc, hname, (tt & VT_USER) ? 3 : 0);
      else 
	fprintf (cf, "  setgate (idt[%d], 0, %s, %s, %d);\n", i, segdesc,
		 hname, (vi[i].vi_type & VT_USER) ? 3 :
		 (vi[i].vi_type & VT_ASH) ? 2 : 0);
    }

    if (tt & VT_DIRECT)
      continue;

    fprintf (sf, "\n\t.align 2\n\t.globl _%s\n_%s:\n", hname, hname);
    trap_setup (sf, tt);
    fprintf (sf, "\tpushl $0x%x\n",
	     (tt & VT_IMSK) ? i - IRQ_OFFSET : i);
    if (tt & VT_FINT) {
      signal_irq_eoi (sf, i - IRQ_OFFSET);
    }
    if (i >= IPITRAP_OFFSET && i < IPITRAP_OFFSET+MAX_IPIS) {
      signal_apic_eoi (sf);
    }
    if (i >= IRQ_OFFSET && i < IRQ_OFFSET+MAX_IRQS) {
      irq_incr_count(sf, i - IRQ_OFFSET);
    }

    if (tt & VT_EBX)
      fprintf (sf, "\tpushl %%ebx\n");
    fprintf (sf, "1:\t.globl\t_Iproc%02x\n\t.set\t_Iproc%02x,1b+1\n", i, i);
    fprintf (sf,
	     "\tcall _%s\n"
	     "\taddl $0x%x,%%esp\n",
	     vi[i].vi_handler,
	     ((tt & (VT_ECODE | VT_EBX)) ? 8 : 4)
	     + ((tt & VT_PUSHAL) ? 20 : 0));

    if (tt & VT_FINT) {
      signal_irq_finished (sf, i - IRQ_OFFSET);
    }
    trap_finish (sf, tt);
  }
  for (i = IRQ_OFFSET+0; i < IRQ_OFFSET+MAX_IRQS; i++) {
     fprintf (cf, "  irqhandlers[%d] = &Iproc%x;\n", i-IRQ_OFFSET, i);
  }

  fprintf (cf, "}\n");
  
  return (0);
}
