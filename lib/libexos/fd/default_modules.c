
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

#include <fd/modules.h>

extern int pty_init(void);
extern int npty_init(void);
extern int udp_socket_init();
extern int nfs_init();
extern int pipe_init();
extern int net_init();
extern int null_init();
extern int exos_tcpsocket_fdinit();
extern int console_init();
extern int cffs_init();
extern int synch_init();
extern int cdev_init();
extern int newpty_init();

fd_module_t start_modules[] = {
#ifdef NEWPTY
  {"cdev", cdev_init},		/* for character devices */
#endif
  {"synch",synch_init},		/* used for tsleep/wakeup sel{wakeup,record} */
  {"console",console_init},	/* for /dev/console */
  {"pty",pty_init},		/* for /oev/pty[a..] old ptys obsolete
				 * used still by hsh and children*/
#ifdef NEWPTY
  {"newpty",newpty_init},	/* for /dev/[pt]typ[0-f] new ptys*/
#else
  {"npty",npty_init},		/* for /dev/[pt]typ[0-f] new ptys*/
#endif
  {"null",null_init},		/* for /dev/null */
  {"net",net_init},		/* if using networking */
  {"udp",udp_socket_init},	/* for UDP sockets, a must if using NFS */
  {"tcpsocket",			/* for TCP sockets */
       exos_tcpsocket_fdinit},
  {"nfs",nfs_init},		/* for NFS  */
  {"unix pipes",pipe_init},	/* for unix pipe (if you use pipe call) */
  {"cffs",cffs_init},		/* For the on-disk C-FFS file system... */
  {0,0}
};

