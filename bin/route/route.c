/*      $OpenBSD: route.c,v 1.23 1997/07/13 23:12:09 angelos Exp $      */
/*      $NetBSD: route.c,v 1.16 1996/04/15 18:27:05 cgd Exp $   */

/*
 * Copyright (c) 1983, 1989, 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* HBXX took some (very little) code from OpenBSD, but include notice. */

#include <exos/net/if.h>
#include <exos/net/route.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


extern struct keytab {
        char    *kt_cp;
        int     kt_i;
} keywords[];
#define K_ADD 1
#define K_DELETE 2
#define K_SHOW 3
#define K_FLUSH 4

struct keytab keywords[] = {
  {"add",K_ADD},
  {"delete",K_DELETE},
  {"show",K_SHOW},
  {"flush",K_FLUSH},
  {0,0}
};

int nflag = 0;

int  parse_route_add(int argc, char **argv);
int parse_route_delete(int argc, char **argv);

static int usage(char *cp) {
  if (cp)
    (void) fprintf(stderr, "route: botched keyword: %s\n", cp);

  fprintf(stderr,"usage: route show\n");
  fprintf(stderr,"usage: route flush\n");
  fprintf(stderr,"usage: route add host netmask destination\n");
  fprintf(stderr,"usage: route delete host netmask\n");
  return -1;
  /* NOTREACHED */
}

int keyword(char *cp) {
  register struct keytab *kt = keywords;
  while (kt->kt_cp && strcmp(kt->kt_cp, cp))
    kt++;
  return kt->kt_i;
}

int main(int argc, char **argv) {

  if (argc < 2) return usage((char *)NULL);
  argc--;
  argv++;
  switch (keyword(*argv)) {
  case K_ADD:
    return parse_route_add(argc,argv);
  case K_DELETE:
    return parse_route_delete(argc,argv);
  case K_SHOW:
    route_show();
    break;
  case K_FLUSH:
    route_flush();
    break;
  default:
    return usage(*argv);
  }
  return 0;
}

extern char *pripaddr(unsigned char ip_addr[4]);
extern char *prethaddr(unsigned char eth_addr[6]);

int parse_route_add(int argc, char **argv) {
  ipaddr_t net, netmask,dst;
  int status;

  argv++;			/* skip keyword */
  argc--;

  if (argc != 3) {
    return usage("must include net netmask destination");
  }
  if (!inet_aton(argv[0], (struct in_addr *)net)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return usage(NULL);
  }
  if (!inet_aton(argv[1], (struct in_addr *)netmask)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return usage(NULL);
  }
  if (!inet_aton(argv[2], (struct in_addr *)dst)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return usage(NULL);
  }
  printf("route_add: net: %s ",pripaddr(net));
  printf("netmask: %s ",pripaddr(netmask));
  printf("destination: %s\n",pripaddr(dst));
  status = route_add(ROUTE_INET,net,netmask,dst);
  printf("%s\n",status ? "BAD" : "OK");
  return status;
}
int parse_route_delete(int argc, char **argv) {
  ipaddr_t net, netmask;
  int status;

  argv++;			/* skip keyword */
  argc--;

  if (argc != 2) {
    return usage(NULL);
  }
  if (!inet_aton(argv[0], (struct in_addr *)net)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return usage(NULL);
  }
  if (!inet_aton(argv[1], (struct in_addr *)netmask)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return usage(NULL);
  }
  printf("route_delete: net: %s ",pripaddr(net));
  printf("netmask: %s\n",pripaddr(netmask));
  status = route_delete(net,netmask);
  printf("%s\n",status ? "BAD" : "OK");
  return status;
}




int findroute(int argc,char **argv) {
  ipaddr_t ipaddr;
  ethaddr_t ethaddr;
  int ifnum;
  int status;

  if (argc  != 2) {
    printf("usage: findroute x.x.x.x\n");
    return -1;
  }
  if (!inet_aton(argv[1], (struct in_addr *)ipaddr)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    return -1;
  }
  ifnum = -1;
  memset(ethaddr,0x80,6);

  if (bcmp(ipaddr,IPADDR_BROADCAST,4) == 0) {
    init_iter_ifnum();
    printf("IP BROADCAST\n");
    while((ifnum = iter_ifnum(IF_UP|IF_BROADCAST)) >= 0) {
      memset(ethaddr,-1,6);
      get_ifnum_ethernet(ifnum, ethaddr);
      printf("  localeth: %s cardno: %d\n",prethaddr(ethaddr),get_ifnum_cardno(ifnum));
    }
    status = 0;
  } else {
    status = get_dsteth(ipaddr,ethaddr, &ifnum);
  }
  if (status == 0) {
    printf("ipaddr: %s -->\n  ethaddr: %s ifnum: %d ", 
	   pripaddr(ipaddr), prethaddr(ethaddr), ifnum);
    memset(ethaddr,-1,6);
    get_ifnum_ethernet(ifnum, ethaddr);
    printf("localeth: %s cardno: %d\n",prethaddr(ethaddr),get_ifnum_cardno(ifnum));
  }
  return status;
}
