
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


#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <exos/netinet/hosttable.h>
#ifdef EXOPC
#include <xok/sysinfo.h>
#endif

#if 1
#define SYSHELP_LEVEL	2
#define DPRINTF(a,b)	if ((a) < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif


map_t *get_hostent_from_name(const char *name)
{
  map_t *m = hosttable_map;

  while(m->name[0] != 0) {
    DPRINTF(SYSHELP_LEVEL,("looking at name: %s\n",m->name));
    if ((strlen(name) == (strlen(m->name))) && (!bcmp(name, m->name, strlen(m->name)))) {
      DPRINTF(SYSHELP_LEVEL,("returning: %s\n",m->name));
#ifdef EXOPC
      if (m->xmitcardno >= __sysinfo.si_nnetworks) {
         m->xmitcardno = 1;
      }
#endif
      return(m);
    }
    m++;
  }
  DPRINTF(SYSHELP_LEVEL,("returning null pointer\n"));
  return (NULL);
}

map_t *get_hostent_from_ether(const char *ether)
{
  map_t *m = hosttable_map;

  while(m->name[0] != 0) {
    DPRINTF(SYSHELP_LEVEL,("looking at name: %s\n",m->name));
    if(!bcmp(ether, m->eth_addr, 6)) {
      DPRINTF(SYSHELP_LEVEL,("returning: %s\n",m->name));
#ifdef EXOPC
      if (m->xmitcardno >= __sysinfo.si_nnetworks) {
         m->xmitcardno = 1;
      }
#endif
      return(m);
    }
    m++;
  }
  DPRINTF(SYSHELP_LEVEL,("returning null pointer\n"));
  return (NULL);
}

map_t *get_hostent_from_ip(const char *ip)
{
  map_t *m = hosttable_map;

  while(m->name[0] != 0) {
    DPRINTF(SYSHELP_LEVEL,("looking at name: %s\n",m->name));
    if(!bcmp(ip, m->ip_addr, 4)) {
      DPRINTF(SYSHELP_LEVEL,("returning: %s\n",m->name));
#ifdef EXOPC
      if (m->xmitcardno >= __sysinfo.si_nnetworks) {
         m->xmitcardno = 1;
      }
#endif
      return(m);
    }
    m++;
  }
  DPRINTF(SYSHELP_LEVEL,("returning null pointer\n"));
  return (NULL);
}
