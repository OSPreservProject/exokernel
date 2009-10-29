
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

#include <xok/env.h>
#include <xok/sysinfo.h>
#include <xuser.h>
#include <exos/uwk.h>
#include <exos/process.h>

#include <sys/stat.h>

#include <exos/netinet/in.h>
#include <exos/net/if.h>
#include <exos/net/route.h>
#include <exos/net/arp.h>

#define DEFAULT_INTERFACE "ed0"


#define CONSOLE_CAP 0
extern char **environ;
extern pipe_init();

char aarray[4096];
char buffer[1024];


static void readline(char *buffer,int length) {
  volatile int *v;
  v = &__sysinfo.si_kbd_nchars;

  if (length == 0) return;
  length--;			/* save last character for \0 */
  for( ; length > 0 ; length--,buffer++) {
  top:
    if (__sysinfo.si_kbd_nchars == 0) {
      wk_waitfor_value_gt((&__sysinfo.si_kbd_nchars),
			  0,
			  CONSOLE_CAP);
    }
    sys_cgets(buffer,1);
    if (*buffer == '\n' || *buffer == '\r') {
      /* ECHO THE NEWLINE PROPERLY */
      buffer[1] = 0;
      buffer[0] = '\r';
      sys_cputs(buffer);
      buffer[0] = '\n';
      sys_cputs(buffer);


      buffer[0] = 0;
      return;
    } else if (*buffer == 8) {
      /* backspace */
      static char erase[] = {8,' ',8,0};
      sys_cputs(erase);
      goto top;
      
    }


    /* ECHO CHARACTERS */
    buffer[1] = 0;
    sys_cputs(buffer);
  }
  buffer[0] = 0;
  return;
}

/* Information it collects:
 ip address
 interface
 netmask
 default gateway

 ip address of NFS server
 NFS directory
 */
#define printf kprintf
int
main (int argc, char **argv)
{
  struct in_addr myip,serverip, mynetmask;
  int status;
  char interface[16];
  char sname[255];
  char dname[255];

  printf("\nWELCOME TO MANUAL BOOT\n");
  printf("YOU CAN'T DELETE. Don't worry if you make mistakes, you can come back later\n");
initial_interface:
retry_myip:
  printf("Please Enter your IP Address: ");
  readline(buffer,80);


  if (!inet_aton(buffer, (struct in_addr *)&myip)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    goto retry_myip;
  }

retry_mynetmask:
  printf("Please Enter your netmask: ");
  readline(buffer,80);
  if (!inet_aton(buffer, (struct in_addr *)&mynetmask)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    goto retry_mynetmask;
  }

  printf("Possible interfaces:\n");
  if_show();
  printf("Please Enter interface of this IP address [%s]: ",DEFAULT_INTERFACE);
  readline(buffer,80);
  /* check validity of interface */
  if (*buffer)
    strcpy(interface,buffer);
  else
    strcpy(interface,DEFAULT_INTERFACE);

  /* configure interface */
  {
    ipaddr_t broadcast;

    apply_netmask_broadcast(broadcast,(char *)&myip, (char *)&mynetmask);
    printf(" Interface: %s\n",interface);
    printf("Ip Address: %s\n",pripaddr((char *)&myip));
    printf("   Netmask: %s\n",pripaddr((char *)&mynetmask));
    printf(" Broadcast: %s\n",pripaddr(broadcast));
    printf("Is this right? type 'yes': ");
    readline(buffer,80);
    if (strcmp(buffer,"yes") != 0) goto initial_interface;
    
    status = ifconfig(interface, IF_UP|IF_BROADCAST, 
	     (char *)&myip, (char *)&mynetmask,broadcast);
    printf("ifconfig %s => %d\n",interface,status);
    if (status != 0) {
      printf("ifconfig failed\n");
      goto initial_interface;
    }
  }

retry_mygateway:
  {
    ipaddr_t mygateway;
    printf("Please Enter your Gateway [press return if you dont care]: ");
    readline(buffer,80);
    if (*buffer) {
      if (!inet_aton(buffer, (struct in_addr *)mygateway)) {
	printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
	goto retry_mygateway;
      }
      printf("Set default gateway to: %s\n",pripaddr(mygateway));
      printf("Is this right? type 'yes': ");
      readline(buffer,80);
      if (strcmp(buffer,"yes") != 0) goto retry_mygateway;
      
      status = route_add(ROUTE_INET,
			 ((ipaddr_t){0,0,0,0}),
			 ((ipaddr_t){0,0,0,0}),
			 mygateway);
      printf("route add default %s ==> %d\n",pripaddr(mygateway),status);
    }
  }

  /* configuring lo0 */
  {
    ipaddr_t lo0mask = {255,255,255,255};
    status = ifconfig("lo0",IF_UP|IF_LOOPBACK,
		      IPADDR_LOOPBACK, lo0mask, IPADDR_BROADCAST);
    printf("configuring loopback: ifconfig lo0 => %d\n",status);
  }

  if_show();
  route_show();
  printf("Press return to continue");
  readline(buffer,80);

#if 0
  {
    extern int start_arp_res_filter();
    printf("starting arp_res_filter\n");
    start_arp_res_filter();
  }
  sleep(1);
#endif

  {
    extern int arp_daemon(void);
    if (arp_daemon() != 0) {printf("problems starting arpd\n");}
  }

  printf("\n");

retry_nfsd:
retry_serverip:
  printf("Please Enter IP Address of your NFS Server: ");
  readline(buffer,80);
  if (!inet_aton(buffer, (struct in_addr *)&serverip)) {
    printf("We currently only accept addresses/mask/nets in the X.X.X.X form\n");
    goto retry_serverip;
  }
  sprintf(sname,"%s",pripaddr((char *)&serverip));

  printf("Please Enter name your NFS directory: ");
  readline(dname,255);
  
  {
    extern int nfs_mount_root();
    printf("MOUNTING ROOT \n");
    status = nfs_mount_root(sname,dname);
    printf("DONE\n");
    if (status != 0) {
      printf("failed (errno: %d)\n",errno);
      printf("check machine \"%s\" /etc/exports file, and make sure that you\n"
	     "either have -alldirs or the exact directory there\n"
	     "you have restarted mountd after making changes\n"
	     "and that you have the proper permission\n",sname);
      printf("fix the problem and try again\n");
      goto retry_nfsd;
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
	  return system(RCLOCAL);
	}
      }
    }
#endif /* RCLOCAL */

  UAREA.u_status = U_SLEEP;
  yield(-1);
  kprintf("SHOULD NEVER REACH HERE\n");

  return 0;
}
