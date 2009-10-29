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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <exos/sysctl.h>
#include <xok/sys_ucall.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dev/ic/mc146818reg.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <exos/process.h>

struct Profile {
  u_short *samples;
  size_t size;
  size_t offset;
  u_int scale;
};

static struct Profile p = {0,0,0,0};

int profil (char *samples, size_t size, u_long offset, u_int scale)
{
  p.samples = (u_short *)samples;
  p.size = size;
  p.offset = offset;
  /*
   * scale must be the last to get assigned,
   * because it is checked to see whether
   * or not to profile
   */
  p.scale = scale;
  if (p.scale)
    UAREA.u_rtc_rega = MC_BASE_32_KHz | MC_RATE_1024_Hz;
  else
    UAREA.u_rtc_rega = 0;
  yield(-1); /* when we return, the kernel will write out rega */
  return 0;
}

void
profil_tick(u_int eip) 
{ 
  if (p.scale != 0) 
    {
      uint64 i = (eip - p.offset)>>1;
      i *= p.scale;
      i >>= 16;
      if (i < p.size)
	++p.samples[i];
    }
}

/*
 * Return information about system clocks.
 */
int
sysctl_clockrate(char *where, size_t *sizep) {
  struct clockinfo clkinfo;

  /*
   * Construct clockinfo structure.
   */
  clkinfo.tick = __sysinfo.si_rate;
  clkinfo.tickadj = 0; /* XXX */
  clkinfo.hz = 1000000 / clkinfo.tick;
  switch (UAREA.u_rtc_rega & (MC_REGA_RSMASK | MC_REGA_DVMASK)) {
  case (MC_RATE_1 | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 256;
    break;
  case (MC_RATE_2 | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 128;
    break;
  case (MC_RATE_8192_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 8192;
    break;
  case (MC_RATE_4096_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 4096;
    break;
  case (MC_RATE_2048_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 2048;
    break;
  case (MC_RATE_1024_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 1024;
    break;
  case (MC_RATE_512_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 512;
    break;
  case (MC_RATE_256_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 256;
    break;
  case (MC_RATE_128_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 128;
    break;
  case (MC_RATE_64_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 64;
    break;
  case (MC_RATE_32_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 32;
    break;
  case (MC_RATE_16_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 16;
    break;
  case (MC_RATE_8_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 8;
    break;
  case (MC_RATE_4_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 4;
    break;
  case (MC_RATE_2_Hz | MC_REGA_DVMASK):
    clkinfo.profhz = clkinfo.stathz = 2;
    break;
  default:
    clkinfo.profhz = clkinfo.stathz = 0;
    break;
  }
  return (sysctl_rdstruct(where, sizep, NULL, &clkinfo, sizeof(clkinfo)));
}
