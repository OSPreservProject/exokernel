
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

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#include <errno.h>
#include <string.h>

#include "routenif.h"

int if_ioctl(int d, unsigned long request, char *argp) {
  
  switch(request) {
  case OSIOCGIFCONF:
  case SIOCGIFCONF:
    {
      struct ifconf *ifconf = (struct ifconf *)argp;
      struct ifreq *ifr = ifconf->ifc_req;
      int ifc_len = ifconf->ifc_len;
      int ifc_copied = 0;
      int ifnum;
      int ifrsize = sizeof (ifr->ifr_name) + sizeof(ifr->ifr_addr);
      init_iter_ifnum();
      while(ifc_len > ifrsize) {
	struct sockaddr_in *sa =(struct sockaddr_in *) &ifr->ifr_addr;
	ifnum = iter_ifnum(0);
	if (ifnum == -1) break;
	/* copy name */
	strncpy(ifr->ifr_name,get_ifnum_name(ifnum),IFNAMSIZ);
	/* copy address */
	get_ifnum_ipaddr(ifnum, (char *)(&sa->sin_addr));
	sa->sin_len = 16;
	sa->sin_family = AF_INET;
	sa->sin_port = 0;
	ifc_len -= ifrsize;
	ifc_copied += ifrsize;
	(char *)ifr += ifrsize;
      }
      ifconf->ifc_len = ifc_copied;
      return 0;
    }
  case OSIOCGIFBRDADDR:
  case SIOCGIFBRDADDR:
  case SIOCGIFFLAGS:
  case OSIOCGIFADDR:
  case SIOCGIFADDR:
    {
      int ifnum;
      struct ifreq *ifr = (struct ifreq *)argp;
      struct sockaddr_in *sa;
      int flags;
      short *sflags;	/* set flags */
      ifnum = get_ifnum_by_name(ifr->ifr_name);
      if (ifnum == -1) {errno = EINVAL; return -1;}

      switch(request) {
      case OSIOCGIFBRDADDR:
      case SIOCGIFBRDADDR:
	/* get broadcast address */
	sa = (struct sockaddr_in *)&ifr->ifr_broadaddr;
	sa->sin_len = 16;
	sa->sin_family = AF_INET;
	sa->sin_port = 0;
	get_ifnum_broadcast(ifnum, (char *)&sa->sin_addr);
	break;
      case OSIOCGIFADDR:
      case SIOCGIFADDR:
	sa = (struct sockaddr_in *)&ifr->ifr_addr;
	sa->sin_len = 16;
	sa->sin_family = AF_INET;
	sa->sin_port = 0;
	get_ifnum_ipaddr(ifnum, (char *)&sa->sin_addr);
	break;
      case SIOCGIFFLAGS:
	get_ifnum_flags(ifnum, &flags);
	sflags = &ifr->ifr_flags;
	*sflags = 0;
	/* add more flags as they are used in exos/net/if.h */
	if (flags & IF_UP) *sflags |= IFF_UP;
	if (flags & IF_BROADCAST) *sflags |= IFF_BROADCAST;
	if (flags & IF_LOOPBACK) *sflags |= IFF_LOOPBACK;
	break;
      }
      return 0;
    }
  }
  return -1;
}

