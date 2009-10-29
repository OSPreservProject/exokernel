#include <stdio.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>

#include "bootp.h"
#include "bootptest.h"

#include <exos/netinet/in.h>
#include <exos/net/if.h>
#include <exos/net/route.h>
//#include <netinet/route.h>
//#include <netinet/arp_table.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <xuser.h>

#ifndef RCLOCAL
#define RCLOCAL "/etc/rc.local"
#endif

extern int debug;
static void
rfc1048_setglobals(u_char *bp, int length);


char *mynetmask = NULL;
char *myip = NULL;
char *mygateway = NULL;

char nfsroothost[255];
char nfsrootpath[255];

#define EXTENSION_LEN
char extension[EXTENSION_LEN];

static unsigned char vm_rfc1048[4] = VM_RFC1048;

void
bootstrap(char *ifname, struct bootp *bp, int length) {
  /* we need the following fields:
     EXTF for NFS ROOT and other interfaces 
     Y: my ip
     SM: my netmask
     G: my gateway 
     */

  int vdlen;
  int status;
  ipaddr_t broadcast;

  myip = (char *)&bp->bp_yiaddr;
  printf("\n");
  printf("Bootp packet length: %d, normal length %d\n",length, sizeof(*bp));

  /* Vendor data can extend to the end of the packet. */
  vdlen = MIN((length - (unsigned int)&bp->bp_vend - (unsigned int)bp), 
	      sizeof(bp->bp_vend));

  printf("Bootp vend length: %d, normal length %d\n",vdlen,sizeof(bp->bp_vend));

  extension[0] = 0;
  if (!bcmp(bp->bp_vend, vm_rfc1048, sizeof(u_int32))) {
    rfc1048_setglobals(bp->bp_vend,vdlen);
  } else {
    printf("ERROR: Not a rfc1048 type of boot reply, this is a requirement since we\n"
	   "need a netmask and a gateway, and EXTF for additional information\n");
    exit(-1);
  }


  {
    int a,b,c,d;
    sscanf(extension,"NFS %d.%d.%d.%d:%s",&a,&b,&c,&d,nfsrootpath);
    sprintf(nfsroothost,"%d.%d.%d.%d",a,b,c,d);
  }
  if (debug) {
    printf(" EXTENSION: %s\n",extension);
    printf(" Interface: %s\n",ifname);
    printf("IP Address: %s\n",pripaddr(myip));
    if (mynetmask) printf("   Netmask: %s\n",pripaddr(mynetmask));
    if (mygateway) printf("   Gateway: %s\n",pripaddr(mygateway));
    printf("NFSROOT: host %s path: %s\n",nfsroothost, nfsrootpath);


  }


  /* SET  */
  if (myip && mynetmask) {
    printf("Setting interface:\n ifconfig %s up broadcast ",ifname);
    printf("%s ",pripaddr(myip));
    printf("%s ",pripaddr(mynetmask));
    apply_netmask_broadcast(broadcast,(char *)myip, (char *)mynetmask);
    printf("%s\n",pripaddr(broadcast));
    if (!debug) {
	  void bootstrap_turnoff_interfaces();
	  ipaddr_t lo0mask = {255,255,255,255};

	  /* turn off all the interfaces we had turn off before */
	  bootstrap_turnoff_interfaces();

	  /* now turn on the rigth ones */
	  status = ifconfig(ifname, IF_UP|IF_BROADCAST, 
						(char *)myip, (char *)mynetmask,broadcast);
      printf("ifconfig %s => %d\n",ifname,status);
	  if (status == -1) {if_show(); sleep(10);}
	  
	  status = ifconfig("lo0",IF_UP|IF_LOOPBACK,
						IPADDR_LOOPBACK, lo0mask, IPADDR_BROADCAST);
	  printf("ifconfig lo0 => %d\n",status);
	  if (status == -1) {if_show(); sleep(10);}
    } 
  } else {
    if (!mynetmask) printf("DID NOT GET NETMASK\n");
    if (!myip) printf("DID NOT GET MY IP\n");
  }

  if (mygateway) {
    if (!debug) {
      status = route_add(ROUTE_INET,
			 ((ipaddr_t){0,0,0,0}),
			 ((ipaddr_t){0,0,0,0}),
			 mygateway);
      printf("route add default %s ==> %d\n",pripaddr(mygateway),status);
    }
  } else {
    printf("DID NOT GET GATEWAY\n");
  }
  


  if (!debug) {

    if_show();
	route_show();
	sleep(5);
    
    {
      extern void arp_daemon(void);
      arp_daemon();
    }


    printf("\n");
    
    {
      extern int nfs_mount_root();
      printf("MOUNTING ROOT %s|%sn",nfsroothost,nfsrootpath);
      status = nfs_mount_root(nfsroothost,nfsrootpath);
      printf("DONE\n");
      if (status != 0) {
		printf("failed (errno: %d)\n",errno);
		printf("check machine \"%s\" /etc/exports file, and make sure that you\n"
			   "either have -alldirs or the exact directory there\n"
			   "you have restarted mountd after making changes\n"
			   "and that you have the proper permission\n",nfsroothost);
		printf("fix the problem and try again\n");
      }
    }

#ifdef RCLOCAL
    {
      struct stat sb;
      if (stat(RCLOCAL,&sb) == 0) {
	sys_cputs("Spawning ");
	sys_cputs(RCLOCAL);
	sys_cputs("\n");
	if(fork() == 0) {
	  /* child */
	  exit(system(RCLOCAL));
	}
      }
    }
#endif /* RCLOCAL */
    
    UAREA.u_status = U_SLEEP;
    yield(-1);
    kprintf("SHOULD NEVER REACH HERE\n");
    


  }
}



static char *
rfc1048_opts[] = {
	/* Originally from RFC-1048: */
	"?PAD",				/*  0: Padding - special, no data. */
	"iSM",				/*  1: subnet mask (RFC950)*/
	"lTZ",				/*  2: time offset, seconds from UTC */
	"iGW",				/*  3: gateways (or routers) */
	"iTS",				/*  4: time servers (RFC868) */
	"iINS",				/*  5: IEN name servers (IEN116) */
	"iDNS",				/*  6: domain name servers (RFC1035)(1034?) */
	"iLOG",				/*  7: MIT log servers */
	"iCS",				/*  8: cookie servers (RFC865) */
	"iLPR",				/*  9: lpr server (RFC1179) */
	"iIPS",				/* 10: impress servers (Imagen) */
	"iRLP",				/* 11: resource location servers (RFC887) */
	"aHN",				/* 12: host name (ASCII) */
	"sBFS",				/* 13: boot file size (in 512 byte blocks) */

	/* Added by RFC-1395: */
	"aDUMP",			/* 14: Merit Dump File */
	"aDNAM",			/* 15: Domain Name (for DNS) */
	"iSWAP",			/* 16: Swap Server */
	"aROOT",			/* 17: Root Path */

	/* Added by RFC-1497: */
	"aEXTF",			/* 18: Extensions Path (more options) */

	/* Added by RFC-1533: (many, many options...) */
#if 1	/* These might not be worth recognizing by name. */

	/* IP Layer Parameters, per-host (RFC-1533, sect. 4) */
	"bIP-forward",		/* 19: IP Forwarding flag */
	"bIP-srcroute",		/* 20: IP Source Routing Enable flag */
	"iIP-filters",		/* 21: IP Policy Filter (addr pairs) */
	"sIP-maxudp",		/* 22: IP Max-UDP reassembly size */
	"bIP-ttlive",		/* 23: IP Time to Live */
	"lIP-pmtuage",		/* 24: IP Path MTU aging timeout */
	"sIP-pmtutab",		/* 25: IP Path MTU plateau table */

	/* IP parameters, per-interface (RFC-1533, sect. 5) */
	"sIP-mtu-sz",		/* 26: IP MTU size */
	"bIP-mtu-sl",		/* 27: IP MTU all subnets local */
	"bIP-bcast1",		/* 28: IP Broadcast Addr ones flag */
	"bIP-mask-d",		/* 29: IP do mask discovery */
	"bIP-mask-s",		/* 30: IP do mask supplier */
	"bIP-rt-dsc",		/* 31: IP do router discovery */
	"iIP-rt-sa",		/* 32: IP router solicitation addr */
	"iIP-routes",		/* 33: IP static routes (dst,router) */

	/* Link Layer parameters, per-interface (RFC-1533, sect. 6) */
	"bLL-trailer",		/* 34: do tralier encapsulation */
	"lLL-arp-tmo",		/* 35: ARP cache timeout */
	"bLL-ether2",		/* 36: Ethernet version 2 (IEEE 802.3) */

	/* TCP parameters (RFC-1533, sect. 7) */
	"bTCP-def-ttl",		/* 37: default time to live */
	"lTCP-KA-tmo",		/* 38: keepalive time interval */
	"bTCP-KA-junk",		/* 39: keepalive sends extra junk */

	/* Application and Service Parameters (RFC-1533, sect. 8) */
	"aNISDOM",			/* 40: NIS Domain (Sun YP) */
	"iNISSRV",			/* 41: NIS Servers */
	"iNTPSRV",			/* 42: NTP (time) Servers (RFC 1129) */
	"?VSINFO",			/* 43: Vendor Specific Info (encapsulated) */
	"iNBiosNS",			/* 44: NetBIOS Name Server (RFC-1001,1..2) */
	"iNBiosDD",			/* 45: NetBIOS Datagram Dist. Server. */
	"bNBiosNT",			/* 46: NetBIOS Note Type */
	"?NBiosS",			/* 47: NetBIOS Scope */
	"iXW-FS",			/* 48: X Window System Font Servers */
	"iXW-DM",			/* 49: X Window System Display Managers */

	/* DHCP extensions (RFC-1533, sect. 9) */
#endif
};
#define	KNOWN_OPTIONS (sizeof(rfc1048_opts) / sizeof(rfc1048_opts[0]))

static void
rfc1048_setglobals(bp, length)
	register u_char *bp;
	int length;
{
	u_char tag;
	u_char *ep;
	register int len;
	char *optstr;


	/* Step over magic cookie */
	bp += sizeof(int32);
	/* Setup end pointer */
	ep = bp + length;
	while (bp < ep) {
		tag = *bp++;
		/* Check for tags with no data first. */
		/* Now scan the length byte. */
		length--;
		len = *bp++;
		length--;
		length -= len;
		if (length < 0) {
		  printf("TRUNCATED PACKET %d\n",length);
		  return;
		}
		if (tag == TAG_PAD)
			continue;
		if (tag == TAG_END)
			return;
		if (tag < KNOWN_OPTIONS) {
			optstr = rfc1048_opts[tag];
			switch(tag) {
			case 3:	/* GW */
			  printf("Gateway\n");
			  /* using only first gateway: */
			  mygateway = bp;
			  /* ENDIANISM? */
			  break;
			case 1:	/* SM */
			  /* using only first netmask: */
			  printf("Netmask\n");
			  mynetmask = bp;
			  /* ENDIANISM? */
			  break;
			case 18: /* EXTF */
			  printf("EXTF\n");
			  bcopy((char *) bp, extension, len);
			  extension[len] = 0;
			}
		} else {
			printf(" T%d:", tag);
			optstr = "?";
		}
		/* Now scan the length byte. */

		if (len) {
		  bp += len;
		  len = 0;
		}
	} /* while bp < ep */
}

void 
bootstrap_turnon_interfaces() {
  int ifnum;
  char *name;
  int status;
  init_iter_ifnum();
  while((ifnum = iter_ifnum(0)) != -1) {
	name = get_ifnum_name(ifnum);
	if (!strcmp(name, "lo0")) continue;
	status = ifconfig(name,IF_UP|IF_BROADCAST,
					  IPADDR_BROADCAST,
					  IPADDR_BROADCAST,
					  IPADDR_BROADCAST);
	kprintf("enabling interface %s => %d\n",name,status);
  }
}
void 
bootstrap_turnoff_interfaces() {
  int ifnum;
  char *name;
  int status;
  init_iter_ifnum();
  while((ifnum = iter_ifnum(IF_UP)) != -1) {
	name = get_ifnum_name(ifnum);
	if (!strcmp(name, "lo0")) continue;
	status = ifconfig(name,IF_DOWN,
					  IPADDR_ANY,
					  IPADDR_ANY,
					  IPADDR_ANY);
	kprintf("disabling interface %s => %d\n",name,status);
  }

}

/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-argdecl-indent: 4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: -4
 * c-label-offset: -4
 * c-brace-offset: 0
 * End:
 */
