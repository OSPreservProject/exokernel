#include <exos/netinet/in.h>
#include <exos/net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <exos/kprintf.h>

static int usage(char *cp) {
  if (cp)
    (void) fprintf(stderr, "ifconfig: botched keyword: %s\n", cp);

  fprintf(stderr,"usage: ifconfig name up [loopback] [broadcast] [ipaddr [netmask [broadcastaddr]]]\n");
  fprintf(stderr,"usage: ifconfig name down");
  fprintf(stderr,"usage: ifconfig -a\n");
  return 1;
}
#define PR kprintf("%d\n",__LINE__)
int
main(int argc, char **argv) {
  ipaddr_t ipaddr, netmask, broadcast;
  char *name;
  int flags;
  int status;
  int ifnum;
  /* skip progname */
  argv++;
  argc--;

  if (argc >= 1 && !strcmp(argv[0],"-a")) {
    if_show();
    return 0;
  }

  if (argc < 2) return usage(NULL);

  name = argv[0];
  ifnum = get_ifnum_by_name(name);
  if (ifnum == -1) {
    fprintf(stderr,"%s: No such interface\n",name);
    return -1;
  }
#if 0
  {
    ipaddr_t b,i,n;
    int flags;
    get_ifnum_broadcast(ifnum, b);
    get_ifnum_netmask(ifnum,n);
    get_ifnum_ipaddr(ifnum,i);
    get_ifnum_flags(ifnum,&flags);
    printf("BEFORE flags %x:\n",flags);
    printf("BEFORE using bcast:     %s\n",pripaddr(b));
    printf("BEFORE using host:      %s\n",pripaddr(i));
    printf("BEFORE using mask:      %s\n",pripaddr(n));
  }
#endif
  if (!strcmp(argv[1],"up")) {
    flags = IF_UP;
  } else if (!strcmp(argv[1],"down")) {
    flags = IF_DOWN;
  } else {
    return usage(argv[1]);
  }

  argv += 2;
  argc -= 2;

  if (argc && !strcmp(argv[0],"loopback")) {
    flags |= IF_LOOPBACK;
    argv++;
    argc--;
  }
  if (argc && !strcmp(argv[0],"broadcast")) {
    flags |= IF_BROADCAST;
    argv++;
    argc--;
  }

  
  switch(argc) {
  default:
    return usage(NULL);
  case 3:
    if (!inet_aton(argv[2], (struct in_addr *)broadcast)) {
      printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
      return usage(NULL);
    }
  case 2:
    if (!inet_aton(argv[1], (struct in_addr *)netmask)) {
      printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
      return usage(NULL);
    }
  case 1:
    if (!inet_aton(argv[0], (struct in_addr *)ipaddr)) {
      printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
      return usage(NULL);
    }
  case 0:
    switch(argc) {
    case 0: get_ifnum_ipaddr(ifnum,ipaddr);
    case 1: get_ifnum_netmask(ifnum,netmask);
    case 2: get_ifnum_broadcast(ifnum,broadcast);
    default:
    }
  }

  if (flags == IF_DOWN) {
    get_ifnum_flags(ifnum,&flags);
    flags &= ~IF_UP;
    flags |= IF_DOWN;
  }

#if 0
  printf("AFTER flags %x:\n",flags);
  printf("AFTER using bcast:     %s\n",pripaddr(broadcast));
  printf("AFTER using host:      %s\n",pripaddr(ipaddr));
  printf("AFTER using mask:      %s\n",pripaddr(netmask));
#endif

  status= ifconfig(name,flags, ipaddr,netmask, broadcast);
  printf("%s\n",status ? "BAD" : "OK");
  return status;
}
