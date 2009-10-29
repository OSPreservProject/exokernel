#include "../../lib/libexos/fd/modules.h"

extern int pty_init(void);
extern int npty_init(void);
extern int udp_socket_init();
extern int nfs_init();
extern int pipe_init();
extern int net_init();
extern int null_init();
extern int fd_fast_tcp_init();
extern int console_init();
extern int cffs_init();

fd_module_t start_modules[] = {
  //{"console",console_init},	/* for /dev/console */
  {"pty",pty_init},		/* for /oev/pty[a..] old ptys obsolete
				 * used still by hsh and children*/
  //{"npty",npty_init},		/* for /dev/[pt]typ[0-f] new ptys*/
  //{"null",null_init},		/* for /dev/null */
  {"net",net_init},		/* if using networking */
#if 0  
  /* NOT DEFINED SINCE IT IS USED BY RCONSOLED WHO OVERRIDE
   * THESE DEFAULT
   */
  {"arp_res",
   start_arp_res_filter},       /* also if using networking
				 * normally done by rconsoled, but
				 * if you are running stand alone you 
				 * may want it, copy
				 * arpd_res.c dpf_lib_arp.c from rconsoled*/
#endif
  {"udp",udp_socket_init},	/* for UDP sockets, a must if using NFS */
  //{"tcpsocket",			/* for TCP sockets */
  //fd_fast_tcp_init},
  {"nfs",nfs_init},		/* for NFS  */
  //{"unix pipes",pipe_init},	/* for unix pipe (if you use pipe call) */
  //{"cffs",cffs_init},		/* For the on-disk C-FFS file system... */
  {0,0}
};

