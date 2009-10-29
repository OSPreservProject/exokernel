
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

#include <malloc.h>
#include <sys/types.h>
#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "assert.h"
#include <exos/debug.h>

#include "netinet/in.h"

#include <xok/wk.h>

#include "tcp_socket.h"
#include "tcp_file_ops.h"

#ifdef AEGIS
#include "exos/vm-layout.h"
#endif /* AEGIS */


extern int 
tcp_socket(struct file * filp);
extern void 
init_handoff(void);

int
fd_fast_tcp_init(void) {
    int i;
    int status;
    START(fd_op[TCP_SOCKET_TYPE],init);
    DPRINTF(CLUHELP_LEVEL,("tcp_socket_init\n"));
    
#if 0
    printf("tcpsocket_init: just to test the memory, but tcp\n"
	   "uses its own static structures\n");
#endif
#ifdef AEGIS

    status = fd_shm_alloc(FD_SHM_OFFSET + TCP_SOCKET_TYPE,
			   (sizeof(struct tcp_shared_data)),
			   (char *)TCP_SHARED_REGION);

    StaticAssert((sizeof(struct tcp_shared_data)) <= TCP_SHARED_REGION_SZ);
    tcp_shared_data = (struct tcp_shared_data *) TCP_SHARED_REGION;
#endif /* AEGIS */
#ifdef EXOPC

    status = 1;
    tcp_shared_data = (struct tcp_shared_data *) 
      malloc(sizeof(struct tcp_shared_data));
    assert(tcp_shared_data);

#endif /* EXOPC */



    if (status == -1) {
	demand(0, problems attaching shm);
	STOP(fd_op[TCP_SOCKET_TYPE],init);
	return -1;
    }

    if (status) {
	/* 	printf("Initializing tcp shared data structture\n"); */
      assert(sizeof(struct tcp_socket_data) <= FILE_DATA_SIZE);

	for (i = 0; i < NR_SOCKETS; i++)
	    tcp_shared_data->used_ports[i] = 0;
#if 0				
	/* Using local structures */
	for (i = 0; i < MAX_CONNECTION; i++)
	    tcb_release(&(tcp_shared_data->connection[i]));
#endif
	tcp_shared_data->next_port = (getpid() << 9) | random();

    } else {
	/* printf("This is not first process, just attaching memory\n"); */
    }
    /* AF_UNIX, SOCK_STREAM, TCP */
    register_family_type_protocol(2,1,6,tcp_socket);
    /* AF_UNIX, SOCK_STREAM, IP */
    register_family_type_protocol(2,1,0,tcp_socket);
    register_file_ops(&tcp_file_ops, TCP_SOCKET_TYPE);
#if 0 /* commented out for now */
    init_handoff();
#endif
    STOP(fd_op[TCP_SOCKET_TYPE],init);
    return 0;
}







