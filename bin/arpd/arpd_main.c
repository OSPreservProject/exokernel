/*
 * Implements the server part of the ARP protocol, 
 * it responds to requests from other hosts for our IP/Ether mapping.
 */
#include <assert.h>
#include <string.h>

#include <stdlib.h>		/* for atoi, atexit */
#include <unistd.h>

#include <exos/net/arp.h>
#include <exos/netinet/in.h>

/*
 * Implements the server part of the ARP protocol, 
 * it responds to requests from other hosts for our IP/Ether mapping.
 */

int main(int argc, char **argv) {
  int status;
  extern void arpd_main (void);

  if(argc > 1 && !strcmp(argv[1],"-print")) {
    arp_print_table();
    return 0;
  }
  if(argc > 1 && !strcmp(argv[1],"-time")) {
    printf("TICKS %qd\n",TICKS);
    return 0;
  }
  if(argc > 1 && !strcmp(argv[1],"-resolve")) {
    char ip[4];
    char eth[6];
    int ifnum;
    if (argc <= 6) {
      printf("Usage: arpd -resolve xx xx xx xx ifnum\n");
      return -1;
    }
    ip[0] = atoi(argv[2]);
    ip[1] = atoi(argv[3]);
    ip[2] = atoi(argv[4]);
    ip[3] = atoi(argv[5]);
    ifnum = atoi(argv[6]);
    printf("RESOLVING: %s\n",pripaddr(ip));
    memset(eth,0,6);
    status = arp_resolve_ip(ip,eth,ifnum);
    printf("ARP_RESOLVE_IP RETURNED: %d (means timeout)\n",status);
    printf("ETHER: %s\n",prethaddr(eth));
    return 0;
  }

  if(argc > 1 && !strcmp(argv[1],"-delete")) {
    char ip[4];
    if (argc <= 5) {
      printf("Usage: arpd -delete xx xx xx xx\n");
      return -1;
    }
    ip[0] = atoi(argv[2]);
    ip[1] = atoi(argv[3]);
    ip[2] = atoi(argv[4]);
    ip[3] = atoi(argv[5]);
    printf("DELETING: %s\n",pripaddr(ip));
    status = arp_remove_ip(ip);
    printf("ARP_REMOVE_IP RETURNED: %d\n",status);
    return 0;
  }

  arpd_main ();
  return 0;
}
