
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

#include <exos/callcount.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <assert.h>

static struct rlimit rlp[RLIM_NLIMITS] = {
  {RLIM_INFINITY, RLIM_INFINITY},
  {RLIM_INFINITY, RLIM_INFINITY},
  {0x0000000010000000, 0x0000000010000000},
  {0x0000000000080000, 0x0000000000800000},
  {0x0000000000000000, RLIM_INFINITY},
  {0x00000000039aa000, 0x00000000039ab000},
  {0x0000000001339000, 0x00000000039ab000},
  {0x0000000000000050, RLIM_INFINITY},
  {0x0000000000000041, 0x00000000000006ec}};

int 
getrlimit(int resource, struct rlimit * rl) {
  OSCALLENTER(OSCALL_getrlimit);
  assert(resource >= 0 && resource < RLIM_NLIMITS);
  *rl = rlp[resource];
  OSCALLEXIT(OSCALL_getrlimit);
  return 0;

}

int 
setrlimit(int resource, const struct rlimit * rl) {
  OSCALLENTER(OSCALL_setrlimit);
  assert(resource >= 0 && resource < RLIM_NLIMITS);
  rlp[resource] = *rl;
  OSCALLEXIT(OSCALL_setrlimit);
  return 0;
}
