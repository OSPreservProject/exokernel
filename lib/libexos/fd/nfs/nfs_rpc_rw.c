
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

#if 0
#undef PRINTF_LEVEL
#define PRINTF_LEVEL 99
#endif
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include <sys/times.h>

#include <fd/proc.h>
#include <fd/path.h>
#include <exos/debug.h>
#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_xdr.h"

#include "errno.h"
#include "assert.h"
#include "malloc.h"

#define kprintf if (1) printf

#if 0  /* AE_RECV procedures */
static void pr_ae_recv(struct ae_recv *send_recv) {
    int i,j = 0;
    printf("NFS ae_recv: %p, ae_recv->n: %d\n",send_recv,send_recv->n);
    for (i=0; i < send_recv->n; i++) {
	printf("[%02d] data: %08x, sz: %2d\t",
	       i, (int)send_recv->r[i].data, send_recv->r[i].sz);
	j += send_recv->r[i].sz;
	demand (j <= (8192+300), too much data to send);
    }
    printf("total size: %d\n",j);
}
#else
#define pr_ae_recv(x)
#endif

//#define lprintf(f,a...) fprintf(stderr,f,##a)
#define lprintf(f,a...)

 //#define kprintf if (0) kprintf
/*
 * Read and Write  procedures in the NFS protocol.
 */

/* PREALLOC used to preallocate all the space in the rpc vector,
 * otherwise it just calls malloc and free everytime, 0-10% performance
 * improvement */
#define PREALLOC



static struct p_rpc overhead_p_rpc;
void pr_p_rpc(struct p_rpc *prpc);

static inline struct p_rpc *
p_overhead_rpc_alloc(int size,int count) {
    int i,number;
#ifdef PREALLOC
    static char space[MAX_P_RPC][1514];
#endif /* PREALLOC */
    number = (count / size) + ((count % size) ? 1 : 0);
    DPRINTF(CLUHELP_LEVEL,("number: %d size: %d count: %d\n",number,size,count));
    overhead_p_rpc.n = number;
    size+=NFS_SLACK_SPACE;		
    for (i = 0 ; i < number; i++) {
	overhead_p_rpc.rw[i].size = size;
#ifdef PREALLOC 
	overhead_p_rpc.rw[i].ptr = (int *)&space[i][0];
#else
	overhead_p_rpc.rw[i].ptr = (int *)__malloc(size); 
#endif /* PREALLOC */
	/* 	printf("m: %08x\n",(int)overhead_p_rpc.rw[i].ptr);   */

	overhead_p_rpc.rw[i].start = overhead_p_rpc.rw[i].ptr;
	overhead_p_rpc.rw[i].end = overhead_p_rpc.rw[i].ptr;

	/* this assert can go later */
	assert(overhead_p_rpc.rw[i].start != (int *)0);
	if (overhead_p_rpc.rw[i].start == (int *)0) {
	    return (struct p_rpc *)0;
	}
    }
    return &overhead_p_rpc;
}

static inline void 
p_overhead_rpc_free(struct p_rpc *prpc) {
#ifndef PREALLOC
    int i;
    assert(prpc != 0);
    for (i = 0 ; i < prpc->n; i++) {
	assert(prpc->rw[i].ptr != (int *)0);
	/*  	printf("f: %08x\n",(int)prpc->rw[i].ptr); */
	__free(prpc->rw[i].ptr);
    }
#endif /* !PREALLOC */
}

DEF_ALIAS_FN (nfs_proc_read,nfs_proc_read_p);

int 
nfs_proc_read_p(struct nfs_fh *fhandle, int offset, int count, 
	      char *data, struct nfs_fattr *fattr) {
    int *p;
    int len = 0;
    int status;
    int ruid = 0;
    struct generic_server *server = fhandle->server;
    struct p_rpc *prpc;
    struct p_rpc_rec *rw;
    int i;
    int rsize;			/* to be used with new prpc */
    int remaining_count = count;
    int total_length = 0;
    char *tmp_data;
    char *tmp_data0;
    
    DPRINTF(CLUHELP_LEVEL,("NFS call  read %d @ %d\n", count, offset));
    rsize = server->rsize;
    //rsize = 1366;
    if ((offset < NFSMAXOFFSET) && ((offset % 4096) != 0)) {
      kprintf("warning non page aligned read: %d:%d\n",offset,count);
    }
    if (!(prpc = p_overhead_rpc_alloc(rsize, count)))
	return -EIO;

    for (i = 0; i < prpc->n ; i++) {
	rw = &prpc->rw[i];
	rw->start = nfs_rpc_header(rw->ptr, NFSPROC_READ, ruid);
	rw->start = xdr_encode_fhandle(rw->start, fhandle);
	*rw->start++ = htonl(offset + i*rsize);	/* offset */
	*rw->start++ = htonl((remaining_count >= rsize) ? 
			     rsize : remaining_count); /* count */
	rw->count = (remaining_count >= rsize) ? rsize : remaining_count;
	remaining_count -= rsize;
	*rw->start++ = htonl(rsize); /* traditional, could be any value */

	rw->end = rw->start;
	rw->r.n = 0;

	rw->xid = rw->ptr[0];
	rw->done = 0;

	/* 	print_rpc2(prpc->rw[i].ptr,128); */
    }
    /*     pr_p_rpc(prpc); */

    if ((status = p_generic_rpc_call(server,prpc)) < 0) {
	p_overhead_rpc_free(prpc);
	return status;
    }
    DPRINTF(CLUHELP_LEVEL,("P_GENERIC_RPC_CALL: %d\n",status));
    tmp_data = tmp_data0 = data;
    for (i = 0; i < prpc->n ; i++) {
	rw = &prpc->rw[i];

	if (!(p = generic_rpc_verify(rw->ptr))) {
	    status = NFSERR_IO;
	    break;
	} else if ((status = ntohl(*p++)) == NFS_OK) {
	    p = xdr_decode_fattr(p, fattr);
	    if (!(p = xdr_decode_data(p, tmp_data, &len, rw->count))) {
		DPRINTF(CLUHELP_LEVEL,("nfs_proc_read: giant data size\n")); 
		status = NFSERR_IO;
		break;
	    } else {
		DPRINTF(CLUHELP_LEVEL,("NFS reply read xid: %d, length: %d count %d\n", 
			 rw->xid,len,rw->count));
		tmp_data += len;
		total_length += len;
	    }
	}
    }
    DPRINTF(CLUHELP_LEVEL,("status: %d\n",status));

    p_overhead_rpc_free(prpc);
    return (status == NFS_OK) ? total_length : nfs_stat_to_errno(status);
}

/* not parallel */
int 
nfs_proc_read_np(struct nfs_fh *fhandle, int offset, int count, 
	      char *data, struct nfs_fattr *fattr) {
    int *p;
    int len = 0;
    int status;
    int ruid = 0;
    struct generic_server *server = fhandle->server;
    int *p0;
    
    DPRINTF(CLUHELP_LEVEL,("NFS call  read %d @ %d\n", count, offset));
    if (!(p0 = overhead_rpc_alloc(server->rsize)))
	return -EIO;

    p = nfs_rpc_header(p0, NFSPROC_READ, ruid);
    p = xdr_encode_fhandle(p, fhandle);
    *p++ = htonl(offset);
    *p++ = htonl(count);
    *p++ = htonl(count);		/* traditional, could be any value */

    DPRINTF(CLUHELP_LEVEL,("ORIGNAL\n"));
    /*     print_rpc2(p0,128); */
    if ((status = generic_rpc_call(server, p0, p)) < 0) {
	overhead_rpc_free(p0);
	return status;
    }


    if (!(p = generic_rpc_verify(p0)))
	status = NFSERR_IO;
    else if ((status = ntohl(*p++)) == NFS_OK) {
	p = xdr_decode_fattr(p, fattr);
	if (!(p = xdr_decode_data(p, data, &len, count))) {
	    DPRINTF(CLUHELP_LEVEL,("nfs_proc_read: giant data size\n")); 
	    status = NFSERR_IO;
	}
	else
	    DPRINTF(CLUHELP_LEVEL,("NFS reply read %d\n", len));
    }

    overhead_rpc_free(p0);
    return (status == NFS_OK) ? len : nfs_stat_to_errno(status);
}

int 
nfs_proc_write(struct nfs_fh *fhandle, int offset, int count, 
	       char *data, struct nfs_fattr *fattr) {
  struct ae_recv r;
  r.n = 1;
  r.r[0].sz = count;
  r.r[0].data = data;

  return nfs_proc_writev(fhandle,offset,count,&r,fattr);
}

int 
nfs_proc_writev(struct nfs_fh *fhandle, int offset, int count, 
	       struct ae_recv *r, struct nfs_fattr *fattr) {
    
    int *p;
    int status;
    int ruid = 0;
    struct generic_server *server = fhandle->server;
    char *tmp_data;
    int i;
    int wsize;			/* to be used with new prpc */
    int remaining_count = count;
    struct p_rpc *prpc;
    struct p_rpc_rec *rw;

        
    DPRINTF(CLUHELP_LEVEL,("NFS call  write %p  %d @ %d\n", r, count, offset));
    wsize = server->wsize;
    if (!(prpc = p_overhead_rpc_alloc(wsize, count)))
	return -EIO;

    demand(count <= wsize, we only handle up to 8K for now);

    tmp_data = NULL; // data;
    for (i = 0; i < prpc->n ; i++) {
      static char overflow[4];
	rw = &prpc->rw[i];
	rw->start = nfs_rpc_header(rw->ptr, NFSPROC_WRITE, ruid);
	rw->start = xdr_encode_fhandle(rw->start, fhandle);
	*rw->start++ = htonl(offset + i*wsize);	/* offset */
	*rw->start++ = htonl(offset + i*wsize);	/* offset */
	rw->count = (remaining_count >= wsize) ? wsize : remaining_count;
	*rw->start++ = htonl(rw->count); /* count */
	*rw->start++ = htonl(rw->count); /* data len */
	rw->end = rw->start;
	rw->r.n = 1;
	rw->r = *r;

	if ((rw->count % 4) != 0) {
	  rw->r.r[r->n].data = overflow;
	  rw->r.r[r->n].sz = 4 - (rw->count % 4);
	  rw->r.n++;
	}
	pr_ae_recv(&rw->r);
	tmp_data += rw->count;
	remaining_count -= rw->count;

	rw->xid = rw->ptr[0];
	rw->done = 0;

	/* 	print_rpc2(prpc->rw[i].ptr,128); */
    }
    /*     pr_p_rpc(prpc); */
    lprintf("p_generic_rpc_call start\n");
    if ((status = p_generic_rpc_call(server,prpc)) < 0) {
	p_overhead_rpc_free(prpc);
	return status;
    }
    lprintf("p_generic_rpc_call done\n");
    DPRINTF(CLUHELP_LEVEL,("P_GENERIC_RPC_CALL: %d\n",status));

    for (i = 0; i < prpc->n ; i++) {
	rw = &prpc->rw[i];

	if (!(p = generic_rpc_verify(rw->ptr))) {
	    status = NFSERR_IO;
	    break;
	} else if ((status = ntohl(*p++)) == NFS_OK) {
	    p = xdr_decode_fattr(p, fattr);
	    DPRINTF(CLUHELP_LEVEL,("NFS reply write xid: %d, count %d\n", 
		   rw->xid,rw->count));
	}
    }
    
    DPRINTF(CLUHELP_LEVEL,("status: %d\n",status));
    
    p_overhead_rpc_free(prpc);
    return nfs_stat_to_errno(status);
}

/*
 * NFS_PROC_NULL
 */

/* NULL does not need parallel requests only if you want  */
/* to measure how fast the parallel stuff is  */

int 
nfs_proc_null(struct nfs_fh *fhandle) {
    int *p;
    int status;
    int ruid = 0;
    static int done = 0;
    int *p0;
    struct generic_server *server = fhandle->server;
    
    if (done && 0) {return 0;} else {done = 1;}
    
    DPRINTF(CLUHELP_LEVEL,("NFS call  null\n"));
    if (!(p0 = overhead_rpc_alloc(server->rsize)))
	return -EIO;
    
    p = nfs_rpc_header(p0, NFSPROC_NULL, ruid);
    
    DPRINTF(CLUHELP_LEVEL,("ORIGNAL\n"));
    /*     print_rpc2(p0,128); */
    if ((status = generic_rpc_call(server, p0, p)) < 0) {
	printf("generic_rpc_call failed\n");
	overhead_rpc_free(p0);
	return status;
    }
    
    
	
    if (!(p = generic_rpc_verify(p0))){
	status = NFSERR_IO;
	printf("NP BAD NULL REQUEST %d\n",status);
	return -1;
    }
    overhead_rpc_free(p0);
    return 0;
}

/* Debugging */

void pr_p_rpc(struct p_rpc *prpc) {
    int n = prpc->n;
    int i;
    assert(n > 0);
    printf("prpc: %08x, number of vectors: %d\n",(int)prpc,n);
    for(i = 0; i < n ; i++) {
	printf("[%02d] sz: %d c: %d x: %d f: %d ptr: %06x "
	       "srt: %06x end: %06x \n",
	       i,(int)prpc->rw[i].size,(int)prpc->rw[i].count,
	       (int)prpc->rw[i].xid,(int)prpc->rw[i].done,
	       (int)prpc->rw[i].ptr,
	       (int)prpc->rw[i].start,(int)prpc->rw[i].end
	       );
    }
}
