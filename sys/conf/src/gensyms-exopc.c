
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


/* just generate the exokernel defines and symbolic addresses, actual
 * writing to file is done in another file */

#include "gensyms.h"

#include <xok/mmu.h>
#include <xok/cpu.h>
#include <xok/env.h>
#include <xok/pci.h>
#include <xok/dc21040reg.h>
#include <exos/vm-layout.h>

#include <xok/sysinfoP.h> /* for offsetof, since we cant use OFFSETOF in
			     initializer */

#define SRL(val, shamt)   (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))

struct sym_type 
defsyms[] = {{"UAREA",                   "*(struct Uenv *)",(UADDR)},
	     {"__u_status",              "*(int *)",(UADDR + offsetof(struct Uenv, u_status))},
	     {"__u_entipc2",             "*(u_int *)",(UADDR + offsetof(struct Uenv, u_entipc2))},
	     {"__u_xesp",                "*(u_int *)",(UADDR + offsetof(struct Uenv, u_xsp))},
	     {"__u_ppc",                 "*(u_int *)",(UADDR + offsetof(struct Uenv, u_ppc))},
	     {"__u_donate",              "*(int *)",(UADDR + offsetof(struct Uenv, u_donate))},
	     {"__u_yield_count",         "*(u_int *)",(UADDR + offsetof(struct Uenv, u_yield_count))},
	     {"__u_epilogue_count",      "*(u_int *)",(UADDR + offsetof(struct Uenv, u_epilogue_count))},
	     {"__u_epilogue_abort_count","*(u_int *)",(UADDR + offsetof(struct Uenv, u_epilogue_abort_count))},
	     {"__envs",                  "(struct Env *)",(UENVS)},
	     {"__ppages",                "(struct Ppage *)",(UPPAGES)},
	     {"__sysinfo",               "*(struct Sysinfo *)",(SYSINFO)},
	     {"__u_in_critical",         "*(u_int *)",(UADDR + offsetof(struct Uenv, u_in_critical))},
	     {"__u_interrupted",         "*(u_int *)",(UADDR + offsetof(struct Uenv, u_interrupted))},
	     {"__u_fpu",                 "*(u_int *)",(UADDR + offsetof(struct Uenv, u_fpu))},
	     {"__u_next_timeout",        "*(unsigned long long *)",(UADDR + offsetof(struct Uenv, u_next_timeout))},
	     {"__cpu_id",                "*(u_int *)",(CPUCXT + offsetof(struct CPUContext, _cpu_id))},
	     {"__page_fault_mode",       "*(int *)",(CPUCXT + offsetof(struct CPUContext, _page_fault_mode))},
	     {"__si_osid",         	 "(char *)", (SYSINFO + offsetof(struct Sysinfo, si_osid))},
	     {"__si_intr_stats",         "(unsigned long long **)",(SYSINFO + offsetof(struct Sysinfo, si_intr_stats))},
#ifdef __SMP__
	     {"__si_global_slocks",         	 "(unsigned long long **)",(SYSINFO + offsetof(struct Sysinfo, si_global_slocks))},
	     {"__si_global_qlocks",         	 "(unsigned long long **)",(SYSINFO + offsetof(struct Sysinfo, si_global_qlocks))},
#endif
	     {"__bc",                    "*(struct bc *)",(UBC)},
	     /*	     {"__xr",                    "*(struct ???  *)",(UXN)), */
	     {"__xn_free_map",            "*(char *)",(UXNMAP)},
	     {"vpt",                     "(Pte *)",(UVPT)},
	     {"vpd",                     "(Pde *)",(UVPT + SRL(UVPT, 10))},
	     {"",0}};


int main (void) {
  gensyms(defsyms);
  return 0;
}


