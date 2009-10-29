
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <exos/uwk.h>

#include <sys/stat.h>

#include <exos/netinet/in.h>
#include <exos/net/route.h>
#include <exos/net/if.h>

#include <exos/netinet/hosttable.h>
#include <exos/site-conf.h>

void setup_net(void) {
  int ifnum;
  ipaddr_t myip;
  ipaddr_t mynetmask = MY_NETMASK;
  ipaddr_t mygateway = MY_GATEWAY;
  ethaddr_t eth;
  char *tmp;

  int status;
  char interface[] = MY_INTERFACE;

  if ((ifnum = get_ifnum_by_name(MY_INTERFACE)) == -1) {
    printf("Machine does not have an %s card, required for booting\n",MY_INTERFACE);
    exit (-1);
  }
  
  get_ifnum_ethernet(ifnum, eth);

  /* HOSTENT STUFF */
  (unsigned char *)tmp = get_ip_from_ether((unsigned char const *)eth);
  memcpy(myip,tmp,4);

  {
    ipaddr_t broadcast;

    apply_netmask_broadcast(broadcast,(char *)&myip, (char *)&mynetmask);
    printf(" Interface: %16s  ",interface);
    printf("Ip Address: %16s\n",pripaddr((char *)&myip));
    printf("   Netmask: %16s  ",pripaddr((char *)&mynetmask));
    printf(" Broadcast: %16s\n",pripaddr(broadcast));
    /* XXX - wait for fast ethernet cards to initialize */
    if (strcmp(interface, "de0") == 0) sleep(4);
    status = ifconfig(interface, IF_UP|IF_BROADCAST, 
	     (char *)&myip, (char *)&mynetmask,broadcast);
    printf("ifconfig %s => %d\n",interface,status);
    if (status != 0) {
      printf("ifconfig failed\n");
    }
  }
  status = route_add(ROUTE_INET,
		     ((ipaddr_t){0,0,0,0}),
		     ((ipaddr_t){0,0,0,0}),
		     mygateway);
  printf("route add default %s ==> %d\n",pripaddr(mygateway),status);


  /* configuring lo0: configure loopback interface and route
   * ip packets from our machineip to our machineip via loopback
   * interface
   */
  {
    ipaddr_t lo0mask = {255,255,255,255};
	if_show();
    status = ifconfig("lo0",IF_UP|IF_LOOPBACK,
		      IPADDR_LOOPBACK, lo0mask, IPADDR_BROADCAST);
    printf("ifconfig lo0 => %d, ",status);

    status = route_add(ROUTE_INET,
		     myip,IPADDR_BROADCAST, IPADDR_LOOPBACK);
    printf("route localhost ==> %d\n",status);
  }
}

void set_hostname (void) {
  ipaddr_t myip;
  char *tmp;
  struct hostent *h;
  int ifnum;
  ethaddr_t eth;

  if ((ifnum = get_ifnum_by_name(MY_INTERFACE)) == -1) {
    printf("Machine does not have an %s card, required for booting\n",MY_INTERFACE);
    exit (-1);
  }
  get_ifnum_ethernet(ifnum, eth);

  /* HOSTENT STUFF */
  (unsigned char *)tmp = get_ip_from_ether((unsigned char const *)eth);
  memcpy(myip,tmp,4);

  /* Do reverse name lookup */

  if ((h = gethostbyaddr(myip,4,AF_INET)) != NULL) {
    printf("My hostname is: %s\n",h->h_name);
    sethostname(h->h_name,strlen(h->h_name));
    setdomainname(MY_DOMAIN,strlen(MY_DOMAIN));
  } else {
    printf ("set_hostname: could not look our name up. halting.\n");
    while (1);
  }
}
