
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
#include <unistd.h>
#include <string.h>
#include <xuser.h>
#include <sys/mmu.h>
#include <sys/sysinfo.h>
#include <sys/env.h>
#include <sys/pctr.h>

#include <fd/modules.h>

extern int pty_init(void);
extern int npty_init(void);
extern int udp_socket_init();
extern int nfs_init();
extern int pipe_init();
extern int net_init();
extern int null_init();
extern int fd_fast_tcp_init();
extern int console_init();

/* OVERRIDE default modules */
fd_module_t start_modules[] = {
				/* the /dev/dumb is always
				 * initialized, and fd's 0,1,2 are set to
				 * it at startup. */
#if 0
  {"console",console_init},	/* for /dev/console */
  {"pty",pty_init},		/* for /oev/pty[a..] old ptys obsolete
				 * used still by hsh and children*/
  {"npty",npty_init},		/* for /dev/[pt]typ[0-f] new ptys*/
  {"null",null_init},		/* for /dev/null */
  {"net",net_init},		/* if using networking */
  {"arp_res",
   start_arp_res_filter},       /* also if using networking
				 * normally done by rconsoled, but
				 * if you are running stand alone you 
				 * may want it, copy
				 * arpd_res.c dpf_lib_arp.c from rconsoled*/
  {"udp",udp_socket_init},	/* for UDP sockets, a must if using NFS */
  {"tcpsocket",			/* for TCP sockets */
   fd_fast_tcp_init},
  {"nfs",nfs_init},		/* for NFS  */
  {"unix pipes",pipe_init},	/* for unix pipe (if you use pipe call) */
#endif
  {0,0}
};

static int ipc1in (int, int, int, int, u_int) __attribute__ ((regparm (3)));
static int
ipc1in (int a1, int a2, int a3, int a4, u_int envid)
{
#if 0
  printf ("env 0x%x: IPC from env 0x%x: %d, %d, %d, %d\n",
	  sys_geteid (), envid, a1, a2, a3, a4);
#endif
  return (0xbad00bad);
}

extern int __asm_ashipc (int, int, int, int, u_int)
     __attribute__ ((regparm (3)));

int
main (int argc, char **argv)
{
  u_int peid = geteid ();
  u_int eid;
  int i;
  pctrval v;

  printf ("I'm a user!\n");
  printf ("   (in environment 0x%x)\n", geteid ());

  ipc1_in = ipc1in;
  u.u_entipc1 = (u_int) &xue_ipc1;

  u.u_entipca = 0x2000;
  eid = fork ();
  printf ("   fork = %d; EID = 0x%x\n", eid, geteid ());

  if (eid) {
    /* Sit and wait for IPC's in parent. */
    u.u_status = 0;
    asm ("int %0" :: "i" (T_YIELD)
	 : "eax", "ecx", "edx", "ebx", "esi", "edi", "ebp");
    for (;;)
      sys_cputs ("TILT!");
    return (0);
  }

#define doit() __asm_ashipc (9, 10, 11, 12, peid)
// #define doit() sipcout (peid, 100, 101, 102, 103)

  printf ("About to ipc...\n");
#if 1
  sys_ash_test (peid);
#endif

  v = 0; i = 0;

  v = rdtsc ();
  i = doit ();
  v = rdtsc () - v;
  printf ("IPC to env 0x%x returns 0x%x takes %qd cycles\n", peid, i, v);

#if 1
  for (i = 0; i < 100; i++)
    doit ();
#endif
#if 1
  {
    v = rdtsc ();
    for (i = 0; i < 10000; i++) {
      // if (doit () != 0x10288) printf ("wrong answer!\n");
      doit ();
    }
    v = rdtsc () - v;
    printf ("IPC takes %qd cycles\n", v / 10000);
  }
#endif

  printf (".. goodbye from environment 0x%x\n", geteid ());
  return (0);
}
