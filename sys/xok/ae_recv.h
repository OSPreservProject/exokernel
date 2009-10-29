
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


#ifndef _XOK_AE_RECV_H_
#define _XOK_AE_RECV_H_

#include <xok_include/string.h>
#include <xok/types.h>

#ifndef KERNEL
#ifndef min
#define min(_a, _b)				\
({						\
  typeof (_a) __a = (_a);			\
  typeof (_b) __b = (_b);			\
  __a <= __b ? __a : __b;			\
})
#endif
#else
#include <xok/defs.h>
#endif

#define AE_RECV_MAXSCATTER	8

struct ae_recv {
        u_int n;  /* number of entries */
        struct rec {
                u_int sz;
                unsigned char *data;
        } r[AE_RECV_MAXSCATTER]; 
};


static inline void ae_recv_structcopy (struct ae_recv *src, struct ae_recv *dst)
{
   int len = sizeof (int) + (src->n * sizeof (struct rec));
   bcopy (src, dst, len);
}


static inline int ae_recv_datacnt (struct ae_recv *recv)
{
   int i, cnt = 0;
   for (i=0; i<recv->n; i++) {
      cnt += recv->r[i].sz;
   }
   return (cnt);
}


static inline int ae_recv_datacpy (struct ae_recv *recv, char *buf, int startoff, int maxlen)
{
   int i = 0;
   int len = 0;
   while ((i < recv->n) && (startoff >= recv->r[i].sz)) {
      int cnt = min (recv->r[i].sz, startoff);
      startoff -= cnt;
      i++;
   }
   while ((i < recv->n) && (len < maxlen)) {
      int cnt = min ((recv->r[i].sz-startoff), (maxlen-len));
      bcopy ((recv->r[i].data+startoff), &buf[len], cnt);
      len += cnt;
      startoff = 0;
      i++;
   }
   return (len);
}

#ifdef KERNEL
#include <xok/mmu.h>
#include <xok/cpu.h>

static inline int ae_recv_datacpy_in (struct ae_recv *recv, char *buf, int startoff, int maxlen)
{
   int i = 0;
   int len = 0;
   while ((i < recv->n) && (startoff >= recv->r[i].sz)) {
      int cnt = min (recv->r[i].sz, startoff);
      startoff -= cnt;
      i++;
   }
   while ((i < recv->n) && (len < maxlen)) {
      int cnt = min ((recv->r[i].sz-startoff), (maxlen-len));
      copyin ((recv->r[i].data+startoff), &buf[len], cnt);
      len += cnt;
      startoff = 0;
      i++;
   }
   return (len);
}
#endif

#ifndef KERNEL
#undef min
#endif

#endif
