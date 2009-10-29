
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

#ifndef _NFSRPC_H_
#define _NFSRPC_H_

#include <sys/types.h>
#include "nfs_rpc_procs.h" 
#include "sys/socket.h"
#include "netinet/in.h"

#include "exos/locks.h"


#include "nfs_server.h"
#include "nfs_struct.h"

#include <xok/ae_recv.h>	/* for struct p_rpc_reck */

#define RPC_VERSION 2

#define RPCUDP 17
#define RPCTCP 6

#define SERVERTIMEO 20		/* initial timeout in .001 secs */
#define SERVERRETRANS 10
#define CURRENT_TIME 0


enum rpc_auth_flavor {
  RPC_AUTH_NULL = 0,
  RPC_AUTH_UNIX = 1,
  RPC_AUTH_SHORT = 2
};

enum rpc_msg_type {
  RPC_CALL = 0,
  RPC_REPLY = 1
};

enum rpc_reply_stat {
  RPC_MSG_ACCEPTED = 0,
  RPC_MSG_DENIED = 1
};

enum rpc_accept_stat {
  RPC_SUCCESS = 0,
  RPC_PROG_UNAVAIL = 1,
  RPC_PROG_MISMATCH = 2,
  RPC_PROC_UNAVAIL = 3,
  RPC_GARBAGE_ARGS = 4
};

enum rpc_reject_stat {
  RPC_MISMATCH = 0,
  RPC_AUTH_ERROR = 1
};

enum rpc_auth_stat {
  RPC_AUTH_BADCRED = 1,
  RPC_AUTH_REJECTEDCRED = 2,
  RPC_AUTH_BADVERF = 3,
  RPC_AUTH_REJECTEDVERF = 4,
  RPC_AUTH_TOOWEAK = 5
};


inline int *overhead_rpc_alloc(int size);
inline void overhead_rpc_free(int *p);

int *generic_rpc_header(int *p, 
			int program, 
			int version, 
			int procedure, 
			int ruid);

/* For parallel requests, for example :
   reading a page with fout 1K requests  */
#define MAX_P_RPC 4

typedef struct p_rpc_rec {
  int xid;		/* transaction id */
  int done;		/* state */
  int size;		/* size of ptr buffer */
  int count;		/* how many we expect to read */
  int read;		/* how many bytes we read */
  int *ptr;		/* not to be touched */
  int *start;			/* start of rpc header */
  int *end;			/* end of rpc header */
  struct ae_recv r;		/* payload */
} p_rpc_rec_t, *p_rpc_rec_p;
 
struct p_rpc {
  int n;			/* number of entries */
  struct p_rpc_rec rw[MAX_P_RPC];
};

extern int generic_rpc_call(struct generic_server *server, int *start, int *end);
extern int p_generic_rpc_call(struct generic_server *server, struct p_rpc *prpc);

int *generic_rpc_verify(int *p);

void print_rpc2(int *start, int length);
void print_rpc(int *start, int *end);

inline static int *
nfs_rpc_header(int *p, int procedure, int ruid) {
  return generic_rpc_header(p,NFS_PROGRAM,NFS_VERSION,procedure,ruid);
}

#endif /* !_NFSRPC_H_ */
